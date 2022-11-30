// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_KEYBOARD_INPUT_DATA_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_KEYBOARD_INPUT_DATA_H_

#include <string>

#include "third_party/icu/source/common/unicode/umachine.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace autofill_assistant {
namespace keyboard_input_data {

struct DevToolsDispatchKeyEventParams {
  DevToolsDispatchKeyEventParams();
  ~DevToolsDispatchKeyEventParams();
  DevToolsDispatchKeyEventParams(const DevToolsDispatchKeyEventParams&);
  DevToolsDispatchKeyEventParams& operator=(
      const DevToolsDispatchKeyEventParams&);

  // Legacy keyCode for the KeyEvent as described here:
  // https://w3c.github.io/uievents/#dom-keyboardevent-keycode
  ui::KeyboardCode key_code = ui::KeyboardCode::VKEY_UNKNOWN;

  // The list of commands can be found here:
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/editing/commands/editor_command_names.h
  std::string command;
};

// Get "best guess" information on how to synthesize a devtools keyup / keydown
// event based on the character provided. This approach performs the keycode
// value evaluation based on the W3C spec here:
// https://w3c.github.io/uievents/#determine-keydown-keyup-keyCode
// This is not fit for general usage and works best for US layout.
DevToolsDispatchKeyEventParams GetDevToolsDispatchKeyEventParamsForCodepoint(
    UChar32 codepoint);

}  // namespace keyboard_input_data
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_KEYBOARD_INPUT_DATA_H_
