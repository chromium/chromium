// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/keyboard_input_data.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"

namespace autofill_assistant {
namespace keyboard_input_data {
namespace {

// Get KeyboardCode as described in
// https://w3c.github.io/uievents/#determine-keydown-keyup-keyCode
ui::KeyboardCode GetKeyboardCodeForASCII(UChar32 codepoint) {
  // Numerical characters.
  if (base::IsAsciiDigit(codepoint)) {
    return static_cast<ui::KeyboardCode>(codepoint);
  }
  // Alphabetical characters.
  if (base::IsAsciiAlpha(codepoint)) {
    return static_cast<ui::KeyboardCode>(base::ToUpperASCII(codepoint));
  }

  static const base::NoDestructor<base::flat_map<UChar32, ui::KeyboardCode>>
      fixed_char_to_vkey(
          {// https://w3c.github.io/uievents/#fixed-virtual-key-codes
           {'\b', ui::KeyboardCode::VKEY_BACK},
           {'\t', ui::KeyboardCode::VKEY_TAB},
           {'\r', ui::KeyboardCode::VKEY_RETURN},
           {'\n', ui::KeyboardCode::VKEY_RETURN},
           {'\e', ui::KeyboardCode::VKEY_ESCAPE},
           {' ', ui::KeyboardCode::VKEY_SPACE},
           // https://w3c.github.io/uievents/#optionally-fixed-virtual-key-codes
           {';', ui::KeyboardCode::VKEY_OEM_1},
           {':', ui::KeyboardCode::VKEY_OEM_1},
           {'=', ui::KeyboardCode::VKEY_OEM_PLUS},
           {'+', ui::KeyboardCode::VKEY_OEM_PLUS},
           {',', ui::KeyboardCode::VKEY_OEM_COMMA},
           {'<', ui::KeyboardCode::VKEY_OEM_COMMA},
           {'-', ui::KeyboardCode::VKEY_OEM_MINUS},
           {'_', ui::KeyboardCode::VKEY_OEM_MINUS},
           {'.', ui::KeyboardCode::VKEY_OEM_PERIOD},
           {'>', ui::KeyboardCode::VKEY_OEM_PERIOD},
           {'/', ui::KeyboardCode::VKEY_OEM_2},
           {'?', ui::KeyboardCode::VKEY_OEM_2},
           {'`', ui::KeyboardCode::VKEY_OEM_3},
           {'~', ui::KeyboardCode::VKEY_OEM_3},
           {'[', ui::KeyboardCode::VKEY_OEM_4},
           {'{', ui::KeyboardCode::VKEY_OEM_4},
           {'\\', ui::KeyboardCode::VKEY_OEM_5},
           {'|', ui::KeyboardCode::VKEY_OEM_5},
           {']', ui::KeyboardCode::VKEY_OEM_6},
           {'}', ui::KeyboardCode::VKEY_OEM_6},
           {'\'', ui::KeyboardCode::VKEY_OEM_7},
           {'"', ui::KeyboardCode::VKEY_OEM_7}});

  auto vkey_it = fixed_char_to_vkey->find(codepoint);
  if (vkey_it != fixed_char_to_vkey->end()) {
    return vkey_it->second;
  }

  return ui::KeyboardCode::VKEY_UNKNOWN;
}

}  // namespace

DevToolsDispatchKeyEventParams::DevToolsDispatchKeyEventParams() = default;
DevToolsDispatchKeyEventParams::~DevToolsDispatchKeyEventParams() = default;
DevToolsDispatchKeyEventParams::DevToolsDispatchKeyEventParams(
    const DevToolsDispatchKeyEventParams&) = default;
DevToolsDispatchKeyEventParams& DevToolsDispatchKeyEventParams::operator=(
    const DevToolsDispatchKeyEventParams&) = default;

DevToolsDispatchKeyEventParams GetDevToolsDispatchKeyEventParamsForCodepoint(
    UChar32 codepoint) {
  static const base::NoDestructor<base::flat_map<UChar32, std::string>>
      char_to_commands({{'\b', "DeleteBackward"}});

  DevToolsDispatchKeyEventParams key_info;
  key_info.key_code = GetKeyboardCodeForASCII(codepoint);

  auto commands_it = char_to_commands->find(codepoint);
  if (commands_it != char_to_commands->end()) {
    key_info.command = commands_it->second;
  }

  return key_info;
}

}  // namespace keyboard_input_data
}  // namespace autofill_assistant
