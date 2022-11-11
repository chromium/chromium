// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_KEY_CONVERTER_H_
#define CHROME_TEST_CHROMEDRIVER_KEY_CONVERTER_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "ui/events/keycodes/keyboard_codes.h"

struct KeyEvent;
class Status;

// Converts keys into appropriate |KeyEvent|s. This will do a best effort
// conversion. However, if the input is invalid it will return a status with
// an error message. If |release_modifiers| is true, all modifiers would be
// depressed. |modifiers| acts both an input and an output, however, only when
// the conversion process is successful will |modifiers| be changed.
Status ConvertKeysToKeyEvents(const std::u16string& keys,
                              bool release_modifiers,
                              int* modifiers,
                              std::vector<KeyEvent>* key_events);

Status ConvertKeyActionToKeyEvent(const base::Value::Dict& action_object,
                                  base::Value::Dict& input_state,
                                  bool is_key_down,
                                  std::vector<KeyEvent>* client_key_events);

#endif  // CHROME_TEST_CHROMEDRIVER_KEY_CONVERTER_H_
