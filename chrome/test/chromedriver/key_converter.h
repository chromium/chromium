// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_KEY_CONVERTER_H_
#define CHROME_TEST_CHROMEDRIVER_KEY_CONVERTER_H_

#include <list>
#include <string>

#include "base/strings/string16.h"
#include "base/values.h"
#include "ui/events/keycodes/keyboard_codes.h"

struct KeyEvent;
class Status;

// Converts keys into appropriate |KeyEvent|s. This will do a best effort
// conversion. However, if the input is invalid it will return a status with
// an error message. If |release_modifiers| is true, all modifiers would be
// depressed. |modifiers| acts both an input and an output, however, only when
// the conversion process is successful will |modifiers| be changed.
Status ConvertKeysToKeyEvents(const base::string16& keys,
                              bool release_modifiers,
                              int* modifiers,
                              std::list<KeyEvent>* key_events);

Status ConvertKeyActionToKeyEvent(const base::DictionaryValue* action_object,
                                  base::DictionaryValue* input_state,
                                  bool is_key_down,
                                  std::list<KeyEvent>* client_key_events);

#endif  // CHROME_TEST_CHROMEDRIVER_KEY_CONVERTER_H_
