// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Arduino.h>

// This Arduino sketch is used for manual testing with the browser test
// SerialApiTest.SerialExtension. We have tested with Arduino IDE 1.0.1.

void setup() {
  Serial.begin(57600);
}

void loop() {
  if (Serial.available() > 0)
    Serial.print((char)Serial.read());
}
