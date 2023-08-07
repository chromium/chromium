// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/keycode_text_conversion.h"

// windows.h must be included before versionhelpers.h
#include <windows.h>

#include <stdlib.h>
#include <string.h>
#include <versionhelpers.h>

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

bool ConvertKeyCodeToText(
    ui::KeyboardCode key_code, int modifiers, std::string* text,
    std::string* error_msg) {
  UINT scan_code = ::MapVirtualKeyW(key_code, MAPVK_VK_TO_VSC);
  BYTE keyboard_state[256];
  memset(keyboard_state, 0, 256);
  *error_msg = std::string();
  if (modifiers & kShiftKeyModifierMask)
    keyboard_state[VK_SHIFT] |= 0x80;
  if (modifiers & kControlKeyModifierMask)
    keyboard_state[VK_CONTROL] |= 0x80;
  if (modifiers & kAltKeyModifierMask)
    keyboard_state[VK_MENU] |= 0x80;
  wchar_t chars[5];
  int code = ::ToUnicode(key_code, scan_code, keyboard_state, chars, 4, 0);
  // |ToUnicode| converts some non-text key codes like F1 to various
  // control chars. Filter those out.
  if (code <= 0 ||
      (code == 1 && chars[0] <= UCHAR_MAX &&
       absl::ascii_iscntrl(static_cast<unsigned char>(chars[0])))) {
    *text = std::string();
  } else {
    base::WideToUTF8(chars, code, text);
  }
  return true;
}

bool ConvertCharToKeyCode(char16_t key,
                          ui::KeyboardCode* key_code,
                          int* necessary_modifiers,
                          std::string* error_msg) {
  short vkey_and_modifiers = ::VkKeyScanW(key);
  bool translated = vkey_and_modifiers != -1 &&
                    LOBYTE(vkey_and_modifiers) != 0xFF &&
                    HIBYTE(vkey_and_modifiers) != 0xFF;
  *error_msg = std::string();
  if (translated) {
    *key_code = static_cast<ui::KeyboardCode>(LOBYTE(vkey_and_modifiers));
    int win_modifiers = HIBYTE(vkey_and_modifiers);
    int modifiers = 0;
    if (win_modifiers & 0x01)
      modifiers |= kShiftKeyModifierMask;
    if (win_modifiers & 0x02)
      modifiers |= kControlKeyModifierMask;
    if (win_modifiers & 0x04)
      modifiers |= kAltKeyModifierMask;
    // Ignore bit 0x08: It is for Hankaku key.
    *necessary_modifiers = modifiers;
  }
  return translated;
}

bool SwitchToUSKeyboardLayout() {
  // Prior to Windows 8, calling LoadKeyboardLayout() with KLF_SETFORPROCESS
  // activates specified keyboard layout for the entire process.
  //
  // Beginning in Windows 8: KLF_SETFORPROCESS flag is not used.
  // LoadKeyboardLayout always activates an input locale identifier for
  // the entire system if the current process owns the window with keyboard
  // focus.
  LPCTSTR kUsKeyboardLayout = TEXT("00000409");
  HKL hkl =
      ::LoadKeyboardLayout(kUsKeyboardLayout, KLF_SETFORPROCESS | KLF_ACTIVATE);

  // Inspect only the low word that contains keyboard language identifier,
  // ignoring the device identifier (Dvorak keyboard, etc) in the high word.
  return LOWORD(hkl) == 0x0409;
}
