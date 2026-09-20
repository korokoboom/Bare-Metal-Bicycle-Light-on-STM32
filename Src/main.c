#include <stdint.h>

/* RCC (base 0x40023800) */
#define RCC_AHB1ENR  (*(volatile uint32_t*)0x40023830)
#define RCC_APB2ENR  (*(volatile uint32_t*)0x40023844)
#define RCC_APB1ENR  (*(volatile uint32_t*)0x40023840)
#define GPIOAEN      (1u << 0)
#define GPIOCEN      (1u << 2)
#define SYSCFGEN     (1u << 14)
#define TIM2EN      (1u << 0)

/* GPIO registers */
#define GPIOA_MODER  (*(volatile uint32_t*)0x40020000)
#define GPIOA_ODR    (*(volatile uint32_t*)0x40020014)
#define GPIOA_AFRL   (*(volatile uint32_t*)0x40020020)
#define GPIOC_PUPDR  (*(volatile uint32_t*)0x4002080C)

/* Timer registers */
#define TIM2_CR1	 (*(volatile uint32_t*)0x40000000)
#define TIM2_CR2	 (*(volatile uint32_t*)0x40000004)
#define TIM2_CCMR1	 (*(volatile uint32_t*)0x40000018)
#define TIM2_CCER	 (*(volatile uint32_t*)0x40000020)
#define TIM2_EGR     (*(volatile uint32_t*)0x40000014)
#define TIM2_PSC     (*(volatile uint32_t*)0x40000028)
#define TIM2_ARR     (*(volatile uint32_t*)0x4000002C)
#define TIM2_CCR2    (*(volatile uint32_t*)0x40000038)

/* SYSCFG (base 0x40013800) */
#define SYSCFG_EXTICR4 (*(volatile uint32_t*)0x40013814)
#define SYSCFG_EXTICR1 (*(volatile uint32_t*)0x40013808)

/* EXTI (base 0x40013C00) */
#define EXTI_IMR     (*(volatile uint32_t*)0x40013C00)
#define EXTI_RTSR    (*(volatile uint32_t*)0x40013C08)
#define EXTI_FTSR    (*(volatile uint32_t*)0x40013C0C)
#define EXTI_PR      (*(volatile uint32_t*)0x40013C14)

/* NVIC (EXTI15_10 = IRQ 40, which lives in ISER1 bit 8) */
#define NVIC_ISER0   (*(volatile uint32_t*)0xE000E100)
#define NVIC_ISER1   (*(volatile uint32_t*)0xE000E104)
#define EXTI15_10_EN (1u << 8)
#define EXTI1_IRQ    (1u << 7)

#define LED_PIN      (1u << 5)
#define TIM_PIN      (1u << 1)
#define BTN_PIN      (1u << 13)

/* 0 = LED off, 1 = LED on, 2 = LED blinking */
volatile uint32_t mode = 0;

/* EXTI interrupt handlers */
void EXTI15_10_IRQHandler(void)
{
	if (EXTI_PR & BTN_PIN) {
		mode = (mode + 1) % 3;
		switch (mode) {
			case 0: GPIOA_ODR &= ~LED_PIN; break;  /* LED off */
			case 1: GPIOA_ODR |= LED_PIN; break;   /* LED on */
			case 2: break;                         /* LED blinking, handled in EXTI1_IRQHandler */
		}
		EXTI_PR = BTN_PIN;
	}
}

void EXTI1_IRQHandler(void)
{
	if (EXTI_PR & TIM_PIN) {
		if(mode == 2){
			GPIOA_ODR ^= LED_PIN;   /* toggle LED on PWM rising edge */
		}
		EXTI_PR = TIM_PIN;
	}
}

/* Setup Functions */
void setup_clocks(void){
	/* enable GPIOA, GPIOC and SYSCFG, TIM2 clocks */
	RCC_AHB1ENR |= GPIOAEN;
	RCC_AHB1ENR |= GPIOCEN;
	RCC_APB2ENR |= SYSCFGEN;
	RCC_APB1ENR |= TIM2EN;
}

void setup_gpio(void){
	/* PA5 output mode */
	GPIOA_MODER = (GPIOA_MODER & ~(3u << 10)) | (1u << 10);

	/* PA1 alternate function mode */
	GPIOA_MODER = (GPIOA_MODER & ~(3u << 2)) | (2u << 2);
	GPIOA_AFRL = (GPIOA_AFRL & ~(0xFu << 4)) | (0x1u << 4);

	/* PC13 input with pull-up */
	GPIOC_PUPDR = (GPIOC_PUPDR & ~(3u << 26)) | (1u << 26);
}

void setup_timer(void){
	/* Setup timer values: 1 kHz tick (16 MHz / 16000), period = 125 ms */
	TIM2_PSC = 16000 - 1;
	TIM2_ARR = 125 - 1;

	/* Configure timer for PWM mode 1 on channel 2 */
	TIM2_CCMR1 = (TIM2_CCMR1 & ~(0xFu << 12)) | (0x6u << 12) | (1u << 11);

	/* Set duty cycle */
	TIM2_CCR2 = 62;

	/* Enable channel 2 output */
	TIM2_CCER |= (1u << 4);

	/* Generate an update event */
	TIM2_EGR = 1;

	/* Start the timer */
	TIM2_CR1 |= (1u << 0);
}

void setup_exti(void){
	/* route PA1 to EXTI1  */
	SYSCFG_EXTICR1 = SYSCFG_EXTICR1 & ~(0xFu << 4);

	/* unmask EXTI1, trigger on rising edge */
	EXTI_IMR |= TIM_PIN;
	EXTI_RTSR |= TIM_PIN;

	/* route PC13 to EXTI13 */
	SYSCFG_EXTICR4 = (SYSCFG_EXTICR4 & ~(0xFu << 4)) | (0x2u << 4);

	/* unmask EXTI13, trigger on falling edge */
	EXTI_IMR |= BTN_PIN;
	EXTI_FTSR |= BTN_PIN;

	/* enable interrupts in NVIC: EXTI1 (IRQ 7 -> ISER0), EXTI15_10 (IRQ 40 -> ISER1) */
	NVIC_ISER0 = EXTI1_IRQ;
	NVIC_ISER1 = EXTI15_10_EN;
}

int main(void)
{
	/* enable GPIOA, GPIOC and SYSCFG, TIM2 clocks */
	setup_clocks();

	/* configure GPIOA pin 5 as output, PA1 as alternate function (TIM2_CH2), PC13 as input with pull-up */
	setup_gpio();

	/* Setup timer values */
	setup_timer();

	/* Setup EXTI */
	setup_exti();

	/* LED starts OFF */
	GPIOA_ODR &= ~LED_PIN;

	for (;;) {
	}
}
