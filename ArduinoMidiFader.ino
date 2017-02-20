/*
 * Copyright (c) 2017 Evan Kale
 * Email: EvanKale91@gmail.com
 * Web: www.youtube.com/EvanKale
 * Social: @EvanKale91
 *
 * This file is part of ArduinoMidiFader.
 *
 * ArduinoMidiFader is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
* Compilation note:
* - Requires Ralf Kistner's arcore patch (adds MIDI-USB support)
*   into Arduino IDE enviroment to compile.
* - Get it here: https://github.com/rkistner/arcore
*/

//Total num of MIDI controls
#define NUM_CONROLS 12

//Num of channels to read directly through analog pins
#define ANALOG_NUM_CHANNELS 4

//Num of channels read through 74HC4051 mux chip
#define MUX_NUM_CHANNELS 8

//Mux output to Arduino analog pin
#define MUX_COM A10

//Mux address select digital pins
#define MUX_ADDRESS_SEL_0 7
#define MUX_ADDRESS_SEL_1 8
#define MUX_ADDRESS_SEL_2 9

//Debug baud rate
#define SERIAL_RATE 9600

//MIDI channel to use
#define MIDI_CHANNEL 0
//CC slots 14-31 are recognized as assignable controls by MIDI standard
#define MIDI_CC_START 14

//maps to map log taper slider pots to linear readings
int sliderFromMap[] = {0, 62, 187, 550, 1023};
int sliderToMap[] = {0, 127};
byte sliderFromMapSize;
byte sliderToMapSize;

//maps to map log taper rotary pots to linear readings
int knobFromMap[] = {0, 55, 130, 516, 1023};
int knobToMap[] = {0, 127};
byte knobFromMapSize;
byte knobToMapSize;

//Holds current MIDI control values
int ccValue[NUM_CONROLS];

//Stores pin numbers of analog channels
byte analogChannelPin[ANALOG_NUM_CHANNELS];

void setup()
{
  Serial.begin(SERIAL_RATE);

  pinMode(MUX_ADDRESS_SEL_0, OUTPUT);
  pinMode(MUX_ADDRESS_SEL_1, OUTPUT);
  pinMode(MUX_ADDRESS_SEL_2, OUTPUT);

  sliderFromMapSize = sizeof(sliderFromMap) / sizeof(int);
  sliderToMapSize = sizeof(sliderToMap) / sizeof(int);

  knobFromMapSize = sizeof(knobFromMap) / sizeof(int);
  knobToMapSize = sizeof(knobToMap) / sizeof(int);

  analogChannelPin[0] = A0;
  analogChannelPin[1] = A1;
  analogChannelPin[2] = A2;
  analogChannelPin[3] = A3;
}

void loop()
{
  //Loop through all mux channels
  for (byte muxChannel = 0; muxChannel < MUX_NUM_CHANNELS; ++muxChannel)
  {
    int midiValue = multiMap(readMuxChannel(muxChannel), sliderFromMap, sliderFromMapSize, sliderToMap, sliderToMapSize);

    if (ccValue[muxChannel] != midiValue)
    {
      ccValue[muxChannel] = midiValue;
      midiControlChange(MIDI_CHANNEL, MIDI_CC_START + muxChannel, midiValue);
    }
  }

  //Loop through all analog channels
  for (byte analogChannel = 0; analogChannel < ANALOG_NUM_CHANNELS; ++analogChannel)
  {
    int analogValue = analogRead(analogChannelPin[analogChannel]);
    int midiValue = 0;

    if (analogChannel == 0)
    {
      //read single remaining slider
      midiValue = multiMap(analogValue, sliderFromMap, sliderFromMapSize, sliderToMap, sliderToMapSize);
    }
    else
    {
      //the rest are rotary pots
      midiValue = multiMap(analogValue, knobFromMap, knobFromMapSize, knobToMap, knobToMapSize);
    }

    if (ccValue[MUX_NUM_CHANNELS + analogChannel] != midiValue)
    {
      ccValue[MUX_NUM_CHANNELS + analogChannel] = midiValue;
      midiControlChange(MIDI_CHANNEL, MIDI_CC_START + MUX_NUM_CHANNELS + analogChannel, midiValue);
    }
  }
}

//Reads analog value of mux chip at selected channel
int readMuxChannel(byte channel)
{
  //Select mux channel
  digitalWrite(MUX_ADDRESS_SEL_0, channel & 1);
  digitalWrite(MUX_ADDRESS_SEL_1, (channel >> 1) & 1);
  digitalWrite(MUX_ADDRESS_SEL_2, (channel >> 2) & 1);

  //Read mux output
  return analogRead(MUX_COM);
}

//Linear interpolates a value in fromMap to toMap
int multiMap(int value, int fromMap[], int fromMapSize, int toMap[], int toMapSize)
{
  //Boundary cases
  if (value <= fromMap[0]) return toMap[0];
  if (value >= fromMap[fromMapSize - 1]) return toMap[toMapSize - 1];

  //Find the fromMap interval that value lies in
  byte fromInterval = 0;
  while (value > fromMap[fromInterval + 1])
    fromInterval++;

  //Find the percentage of the interval that value lies in
  float fromIntervalPercentage = (float)(value - fromMap[fromInterval]) / (fromMap[fromInterval + 1] - fromMap[fromInterval]);

  //Map it to the toMap interval and percentage of that interval
  float toIntervalPercentage = ((fromInterval + fromIntervalPercentage) / (fromMapSize - 1)) * (toMapSize - 1);
  byte toInterval = (byte)toIntervalPercentage;
  toIntervalPercentage = toIntervalPercentage - toInterval;

  //Linear interpolate
  return toMap[toInterval] + toIntervalPercentage * (toMap[toInterval + 1] - toMap[toInterval]);
}

//Sends MIDI CC signal
void midiControlChange(byte channel, byte ccNum, byte value)
{
  //Send a MIDI control change event through USB
  MIDIEvent event = {0x0B, 0xB0 | channel, ccNum, value};
  MIDIUSB.write(event);
  MIDIUSB.flush();
}
