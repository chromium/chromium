// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/keycode_text_conversion.h"

#import <Carbon/Carbon.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

base::Lock tis_lock_;

UniChar GetCharacter(UInt16 mac_key_code, UInt32 modifier_key_state) {
  UInt32 dead_key_state = 0;

  base::AutoLock lock(tis_lock_);
  base::apple::ScopedCFTypeRef<TISInputSourceRef> input_source(
      TISCopyCurrentKeyboardLayoutInputSource());
  return ui::TranslatedUnicodeCharFromKeyCode(
      input_source.get(), mac_key_code, kUCKeyActionDown, modifier_key_state,
      LMGetKbdLast(), &dead_key_state);
}

bool ConvertKeyCodeToText(ui::KeyboardCode key_code,
                          int modifiers,
                          std::string* text,
                          std::string* error_msg) {
  int mac_key_code = ui::MacKeyCodeForWindowsKeyCode(
      key_code, 0, /*us_keyboard_shifted_character=*/nullptr,
      /*keyboard_character=*/nullptr);
  *error_msg = std::string();
  if (mac_key_code < 0) {
    *text = std::string();
    return true;
  }

  int mac_modifiers = 0;
  if (modifiers & kShiftKeyModifierMask)
    mac_modifiers |= shiftKey;
  if (modifiers & kControlKeyModifierMask)
    mac_modifiers |= controlKey;
  if (modifiers & kAltKeyModifierMask)
    mac_modifiers |= optionKey;
  if (modifiers & kMetaKeyModifierMask)
    mac_modifiers |= cmdKey;
  // Convert EventRecord modifiers to format UCKeyTranslate accepts. See docs
  // on UCKeyTranslate for more info.
  UInt32 modifier_key_state = (mac_modifiers >> 8) & 0xFF;

  UniChar character =
      GetCharacter(static_cast<UInt16>(mac_key_code), modifier_key_state);

  if (character && !base::IsAsciiControl(character)) {
    std::u16string text16;
    text16.push_back(character);
    *text = base::UTF16ToUTF8(text16);
    return true;
  }
  *text = std::string();
  return true;
}

bool ConvertCharToKeyCode(char16_t key,
                          ui::KeyboardCode* key_code,
                          int* necessary_modifiers,
                          std::string* error_msg) {
  std::string key_string_utf8 = base::UTF16ToUTF8(std::u16string(1, key));
  bool found_code = false;
  *error_msg = std::string();
  // There doesn't seem to be a way to get a mac key code for a given unicode
  // character. So here we check every key code to see if it produces the
  // right character. We could cache the results and regenerate every time the
  // language changes, but this brute force technique has negligible performance
  // effects (on my laptop it is a submillisecond difference).
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
    if (!ConvertKeyCodeToText(
        code, kShiftKeyModifierMask, &key_string_utf8_tmp, error_msg))
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
