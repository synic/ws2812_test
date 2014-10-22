#include "stm32f30x.h"

uint8_t dma_buffer[2100];
const int num_leds = 60;
const uint8_t PERIOD = 59;
const uint8_t LOW = 12;
const uint8_t HIGH = 35;
const uint8_t RESET_LEN = 50;

void delay(volatile uint64_t delay) {
    delay *= 1000;
    while(delay--) {}
}

void ws2812_set_color(uint8_t led, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t n = (led * 24);
    uint8_t i;

    for(i = 8; i-- > 0; n++) {
        dma_buffer[n] = g & (1 << i) ? HIGH : LOW;
    }

    for(i = 8; i-- > 0; n++) {
        dma_buffer[n] = r & (1 << i) ? HIGH : LOW;
    }

    for(i = 8; i-- > 0; n++) {
        dma_buffer[n] = b & (1 << i) ? HIGH : LOW;
    }

}

void ws2812_set_color_single(uint8_t led, uint32_t c) {
    volatile uint8_t
        r = (uint8_t)(c >> 16),
        g = (uint8_t)(c >> 8),
        b = (uint8_t)(c);

    ws2812_set_color(led, r, g, b);
}

uint32_t ws2812_color(uint8_t r, uint8_t b, uint8_t g) {
    return ((uint32_t)r << 16) | ((uint32_t) g << 8) | b;
}

void ws2812_clear(void) {
    uint8_t i;

    for(i = 0; i < num_leds; i++) {
        ws2812_set_color(i, 0, 0, 0);
    }
}

void ws2812_show(void) {
    uint16_t memaddr;
    uint16_t buffersize;

    buffersize = (num_leds * 24) + RESET_LEN;
    memaddr = (num_leds * 24);

    while(memaddr < buffersize) {
        dma_buffer[memaddr] = 0;
        memaddr++;
    }

    TIM_SetCounter(TIM2, PERIOD);
    DMA_SetCurrDataCounter(DMA1_Channel7, buffersize);
    DMA_Cmd(DMA1_Channel7, ENABLE);
    TIM_Cmd(TIM2, ENABLE);

    while(!DMA_GetFlagStatus(DMA1_FLAG_TC7));

    DMA_Cmd(DMA1_Channel7, DISABLE);
    TIM_Cmd(TIM2, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_TC7);
}

uint32_t wheel(char pos) {
    if(pos < 85) {
        return ws2812_color(pos * 3, 255 - pos * 3, 0);
    }
    else if(pos < 170) {
        pos -= 85;
        return ws2812_color(255 - pos * 3, 0, pos * 3);
    }
    else {
        pos -= 170;
        return ws2812_color(0, pos * 3, 255 - pos * 3);
    }
}

void rainbow(uint32_t wait) {
    volatile uint16_t i, j;

    for(j = 0; j < 256 * 5; j++) {
        for(i = 0; i < num_leds; i++) {
            ws2812_set_color_single(
                i, wheel(((i * 256 / num_leds) + j) & 255));
        }
        ws2812_show();
        delay(wait);
    }
}


void setup_gpio(void)
{
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;

    // set up pin PA10 for the timer
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_10);
}

void setup_timer(void) {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseInitTypeDef TIM_InitStructure;
    TIM_InitStructure.TIM_Period = PERIOD;
    TIM_InitStructure.TIM_Prescaler = 0;
    TIM_InitStructure.TIM_ClockDivision = 0;
    TIM_InitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_InitStructure);

    // configure the output channel
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init(TIM2, &TIM_OCInitStructure);
    TIM_OC4PreloadConfig(TIM2, TIM_OCPreload_Enable);
}

void setup_dma(void) {
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_InitTypeDef DMA_InitStructure;

    DMA_DeInit(DMA1_Channel7);

    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&TIM2->CCR4;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)dma_buffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_BufferSize = 1;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel7, &DMA_InitStructure);

    TIM_DMACmd(TIM2, TIM_DMA_CC4, ENABLE);
}

int main(void) {
    if(SysTick_Config(SystemCoreClock / 1000)) {
        while(1) { }
    }

    setup_gpio();
    setup_timer();
    setup_dma();

    ws2812_clear();
    while(1) {
        rainbow(0);
    }

    return 0;
}

