// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/keycode_text_conversion.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

void InitializeOzoneKeyboardEngineManager() {
  static std::unique_ptr<ui::StubKeyboardLayoutEngine> keyboard_layout_engine_ =
      std::make_unique<ui::StubKeyboardLayoutEngine>();
  ui::KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
      keyboard_layout_engine_.get());
}

#if BUILDFLAG(IS_OZONE_X11)
bool ConvertKeyCodeToTextOzone
#else
bool ConvertKeyCodeToText
#endif
    (ui::KeyboardCode key_code,
     int modifiers,
     std::string* text,
     std::string* error_msg) {
  ui::KeyboardLayoutEngine* keyboard_layout_engine =
      ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
  if (!keyboard_layout_engine) {
    return false;
  }
  ui::DomCode dom_code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  int event_flags = ui::EF_NONE;

  // Chrome OS keyboards don't have meta or num lock keys, so these modifier
  // masks are ignored. Only handle alt, ctrl and shift.
  if (modifiers & kAltKeyModifierMask)
    event_flags |= ui::EF_ALT_DOWN;
  if (modifiers & kControlKeyModifierMask)
    event_flags |= ui::EF_CONTROL_DOWN;
  if (modifiers & kShiftKeyModifierMask)
    event_flags |= ui::EF_SHIFT_DOWN;

  ui::DomKey dom_key;
  ui::KeyboardCode key_code_ignored;

  if (!keyboard_layout_engine->Lookup(dom_code, event_flags, &dom_key,
                                      &key_code_ignored) ||
      !dom_key.IsCharacter()) {
    // The keycode lookup failed, or mapped to a key that isn't a unicode
    // character. Convert it to the empty string.
    *text = std::string();
    return true;
  }

  base::WriteUnicodeCharacter(dom_key.ToCharacter(), text);
  return true;
}

#if BUILDFLAG(IS_OZONE_X11)
bool ConvertCharToKeyCodeOzone
#else
bool ConvertCharToKeyCode
#endif
    (char16_t key,
     ui::KeyboardCode* key_code,
     int* necessary_modifiers,
     std::string* error_msg) {
  std::string key_string_utf8 = base::UTF16ToUTF8(std::u16string(1, key));
  bool found_code = false;
  *error_msg = std::string();
  // There doesn't seem to be a way to get a CrOS key code for a given unicode
  // character. So here we check every key code to see if it produces the
  // right character, as we do on Mac (see keycode_text_conversion_mac.mm).
  for (int i = 0; i < 256; ++i) {
    ui::KeyboardCode code = static_cast<ui::KeyboardCode>(i);
    // Skip the numpad keys.
    if (code >= ui::VKEY_NUMPAD0 && code <= ui::VKEY_DIVIDE)
      continue;
    std::string key_string;
    if (!ConvertKeyCodeToText(code, 0, &key_string, error_msg))
      return false;
    found_code = key_string_utf8 == key_string;
    std::string key_string_utf8_tmp;
    if (!ConvertKeyCodeToText(code, kShiftKeyModifierMask, &key_string_utf8_tmp,
                              error_msg))
      return false;
    if (!found_code && key_string_utf8 == key_string_utf8_tmp) {
      *necessary_modifiers = kShiftKeyModifierMask;
      found_code = true;
    }
    if (found_code) {
      *key_code = code;
      break;
    }
  }
  return found_code;
}
