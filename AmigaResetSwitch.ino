/* 
 * This file is part of the AmiResetSwitch project 
 * (https://github.com/matsstaff/AmiResetSwitch).
 * Copyright (c) 2018 Mats Staffansson.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <avr/sleep.h>
#include <EEPROM.h>

//#define PERSIST_STATE_TO_EEPROM

#define DEFAULT_TIMEOUT_MS 2400
#define DRIVEID_TIMEOUT_MS 400

unsigned char numresets=1;

void kbreset(){
  // Do nothing (just wake up)
}

void setup(){
  // Pin 2 (INT0) must be input
  pinMode(2, INPUT);

  // Controls 'switching' of _SEL0 and _SEL1 on even CIA.
  pinMode(A2, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(13, OUTPUT);

  // Pullups for transistors
  pinMode(A0, INPUT_PULLUP);
  pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);

  pinMode(4, INPUT_PULLUP); // <--- This is used for PCINT20 (_SEL1 MB)
  pinMode(6, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);

  // Power
  pinMode(10, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(7, OUTPUT);
  digitalWrite(10, LOW);
  digitalWrite(7, LOW);
  digitalWrite(3, HIGH);

  // We do *not* want to any actual pinchange interrupts.
  PCICR=0;
  // But we do want the pinchange interrupt flag for PCINT20
  PCMSK2=(1<<PCINT20);

#ifdef PERSIST_STATE_TO_EEPROM
  // Read current 'double reset' counter
  numresets=EEPROM.read(EEPROM.read(256));
#endif
}


void loop(){
#ifdef PERSIST_STATE_TO_EEPROM
  unsigned char eevalue=numresets;
#endif
  unsigned long timeref;
  unsigned long timeout;

  // Set normal (un-switched) drive state
  digitalWrite(A2, HIGH);
  digitalWrite(8, HIGH);
  digitalWrite(5, LOW);
  digitalWrite(13, LOW);

  // Wait until reset goes high
  while(digitalRead(2)==LOW); 

  // Make sure flag is clear
  PCIFR=(1<<PCIF2);

  timeref=millis();
  timeout=DEFAULT_TIMEOUT_MS;
  while(millis()-timeref<timeout){
    
    // Another reset detected?
    if(digitalRead(2)==LOW){
      
      // Increment 'double-reset counter'
      numresets++;
      
      // Wait until reset is high again before moving on
      while(digitalRead(2)==LOW);
      
      // Reset wait
      timeref=millis();
      timeout=DEFAULT_TIMEOUT_MS;
      PCIFR=(1<<PCIF2); // Make sure flag is clear
    }
    
    // Drive-ID detected (_SEL1 line has had action)?
    if(PCIFR&(1<<PCIF2)){
      // Give user a bit more time
      timeref=millis();
      timeout=DRIVEID_TIMEOUT_MS;
      PCIFR=(1<<PCIF2); // *MUST* clear flag
    }
  }

  // Perform drive switch if needed
  // Note: only last bit actually used (even or odd number of double resets)
  digitalWrite(A2,numresets&1);
  digitalWrite(8,numresets&1);
  digitalWrite(5,!(numresets&1));
  digitalWrite(13,!(numresets&1));

#ifdef PERSIST_STATE_TO_EEPROM
  // Save state to EEPROM if it has changed
  if((numresets^eevalue)&0x1){
    unsigned char eeaddr=EEPROM.read(256);
    eevalue++;
    if(eevalue==0){
      eeaddr++;
      EEPROM.write(256,eeaddr);
    }
    EEPROM.write(eeaddr,eevalue);
  }
#endif

  // Go to sleep
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(0,kbreset,LOW); // Note: INT0 is on pin 2
  sleep_mode();
  
  // Awoke from interrupt
  sleep_disable();
  detachInterrupt(0); 
}
