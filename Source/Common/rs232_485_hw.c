/*
 *  Open HR20
 *
 *  target:     ATmega169 in Honnywell Rondostat HR20E / ATmega32
 *
 *  compiler:   WinAVR-20071221
 *              avr-libc 1.6.0
 *              GCC 4.2.2
 *
 *  copyright:  2008 Juergen Sachs (juergen-sachs-at-gmx-dot-de)
 *				2008 Jiri Dobry (jdobry-at-centrum-dot-cz)
 *
 *  license:    This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU Library General Public
 *              License as published by the Free Software Foundation; either
 *              version 2 of the License, or (at your option) any later version.
 *
 *              This program is distributed in the hope that it will be useful,
 *              but WITHOUT ANY WARRANTY; without even the implied warranty of
 *              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *              GNU General Public License for more details.
 *
 *              You should have received a copy of the GNU General Public License
 *              along with this program. If not, see http:*www.gnu.org/licenses
 */

/*!
 * \file       rs232_485_hw.c
 * \brief      hardware layer of the rs232 and rs485
 * \author     Juergen Sachs (juergen-sachs-at-gmx-dot-de); Jiri Dobry <jdobry-at-centrum-dot-cz>
 * \date       $Date: 2011-11-15 01:08:52 +0100 (Wt, 15 lis 2011) $
 * $Rev: 364 $
 */

// AVR LibC includes
#include <stdint.h>
#include <avr/io.h>
#include <stdio.h>

// HR20 Project includes
#include "config.h"
#include "main.h"
#include "com.h"

/* The following must be AFTER the last include line */
#if (defined COM_RS232) || (defined COM_RS485)

#if defined (_AVR_IOM169_H_) || defined (_AVR_IOM32_H_)
#define UDR0 UDR
#define RXEN0 RXEN
#define RXCIE0 RXCIE
#define UCSR0A UCSRA
#define UCSR0B UCSRB
#define TXC0 TXC
#define TXCIE0 TXCIE
#define UDRIE0 UDRIE
#define TXEN0 TXEN
#define UBRR0H UBRRH
#define UBRR0L UBRRL
#define UCSR0C UCSRC
#define UCSZ00 UCSZ0
#define UCSZ01 UCSZ1
#define U2X0 U2X
#define URSEL0 URSEL
#endif

/*!
 *******************************************************************************
 *  Interrupt for receiving bytes from serial port
 *
 *  \note
 ******************************************************************************/
#if defined (_AVR_IOM169P_H_) || defined (_AVR_IOM169_H_) || defined (_AVR_IOM329_H_)
ISR(USART0_RX_vect)
#elif defined(_AVR_IOM16_H_) || defined(_AVR_IOM32_H_)
ISR(USART_RXC_vect)
#endif
{
	COM_rx_char_isr(UDR0);	// Add char to input buffer
#if !defined(MASTER_CONFIG_H)
	UCSR0B &= ~(_BV(RXEN0) | _BV(RXCIE0)); // disable receive
#endif
#if !defined(_AVR_IOM16_H_) && !defined(_AVR_IOM32_H_)
	PCMSK0 |= (1 << PCINT0); // activate interrupt
#endif
}

/*!
 *******************************************************************************
 *  Interrupt, we can send one more byte to serial port
 *
 *  \note
 *  - We send one byte
 *  - If we have send all, disable interrupt
 ******************************************************************************/
#if defined (_AVR_IOM169P_H_) || defined(_AVR_IOM329_H_)
ISR(USART0_UDRE_vect)
#elif defined(_AVR_IOM169_H_) || defined(_AVR_IOM16_H_)  || defined(_AVR_IOM32_H_)
ISR(USART_UDRE_vect)
#endif
{
	char c;
	if ((c = COM_tx_char_isr()) != '\0')
	{
		UDR0 = c;
	}
	else	// no more chars, disable Interrupt
	{
		UCSR0B &= ~(_BV(UDRIE0));
		UCSR0A |= _BV(TXC0); // clear interrupt flag
		UCSR0B |= (_BV(TXCIE0));
	}
}

/*!
 *******************************************************************************
 *  Interrupt for transmit done
 *
 *  \note
 ******************************************************************************/
#if defined (_AVR_IOM169P_H_) || defined (_AVR_IOM169_H_) || defined(_AVR_IOM329_H_)
ISR(USART0_TX_vect)
#elif defined(_AVR_IOM16_H_) || defined(_AVR_IOM32_H_)
ISR(USART_TXC_vect)
#endif
{
#if defined COM_RS485
	// TODO: change rs485 to receive
#error "code is not complete for rs485"
#endif
	UCSR0B &= ~(_BV(TXCIE0) | _BV(TXEN0));
}

/*!
 *******************************************************************************
 *  Initialize serial, setup Baudrate
 *
 *  \note
 *  - set Baudrate
 ******************************************************************************/
void RS_Init(void)
{

	// Baudrate
	//long ubrr_val = ((F_CPU)/(baud*8L)-1);
	uint16_t ubrr_val = ((F_CPU) / (COM_BAUD_RATE * 8L) - 1);

	UCSR0A = _BV(U2X0);
	UBRR0H = (unsigned char)(ubrr_val >> 8);
	UBRR0L = (unsigned char)(ubrr_val & 0xFF);
	UCSR0C = (_BV(UCSZ00) | _BV(UCSZ01));     // Asynchron 8N1 
#if defined(_AVR_IOM16_H_) || defined(_AVR_IOM32_H_)
	UCSR0C = (1 << URSEL0) | (_BV(UCSZ00) | _BV(UCSZ01));     // Asynchron 8N1
#if defined(MASTER_CONFIG_H)
	UCSR0B = _BV(RXCIE0) | _BV(RXEN0);
#endif
#else 
	UCSR0C = (_BV(UCSZ00) | _BV(UCSZ01));     // Asynchron 8N1
#endif
#if !defined(_AVR_IOM16_H_) && !defined(_AVR_IOM32_H_)
	PCMSK0 |= (1 << PCINT0); // activate interrupt
#endif
}

/*!
 *******************************************************************************
 *  Starts sending the content of the output buffer
 *
 *  \note
 *  - we send the first char to serial port
 *  - start the interrupt
 ******************************************************************************/
void RS_startSend(void)
{
	cli();
#if defined COM_RS485
	// TODO: change rs485 to transmit
#error "code is not complete for rs485"
#endif
	if ((UCSR0B & _BV(UDRIE0)) == 0)
	{
		UCSR0B &= ~(_BV(TXCIE0));
		UCSR0A |= _BV(TXC0); // clear interrupt flag
		UCSR0B |= _BV(UDRIE0) | _BV(TXEN0);
		// UDR0 = COM_tx_char_isr(); // done in interrupt
	}
	sei();
}

#endif /* COM_RS232 */
