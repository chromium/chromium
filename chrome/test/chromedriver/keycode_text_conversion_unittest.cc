// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/keycode_text_conversion.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/keyboard_layout.h"

namespace {

void CheckCharToKeyCode16(char16_t character,
                          ui::KeyboardCode key_code,
                          int modifiers) {
  ui::KeyboardCode actual_key_code = ui::VKEY_UNKNOWN;
  int actual_modifiers = 0;
  std::string error_msg;
  EXPECT_TRUE(ConvertCharToKeyCode(
      character, &actual_key_code, &actual_modifiers, &error_msg));
  EXPECT_EQ(key_code, actual_key_code)
      << "Char: " << std::u16string(1, character);
  EXPECT_EQ(modifiers, actual_modifiers)
      << "Char: " << std::u16string(1, character);
}

void CheckCharToKeyCode(char character, ui::KeyboardCode key_code,
                        int modifiers) {
  CheckCharToKeyCode16(base::UTF8ToUTF16(std::string(1, character))[0],
                       key_code, modifiers);
}

#if BUILDFLAG(IS_WIN)
void CheckCharToKeyCode(wchar_t character, ui::KeyboardCode key_code,
                        int modifiers) {
  CheckCharToKeyCode16(base::WideToUTF16(std::wstring(1, character))[0],
                       key_code, modifiers);
}
#endif

void CheckCantConvertChar(wchar_t character) {
  std::wstring character_string;
  character_string.push_back(character);
  char16_t character_utf16 = base::WideToUTF16(character_string)[0];
  ui::KeyboardCode actual_key_code = ui::VKEY_UNKNOWN;
  int actual_modifiers = 0;
  std::string error_msg;
  EXPECT_FALSE(ConvertCharToKeyCode(
      character_utf16, &actual_key_code, &actual_modifiers, &error_msg));
}

std::string ConvertKeyCodeToTextNoError(ui::KeyboardCode key_code,
                                        int modifiers) {
  std::string error_msg;
  std::string text;
  EXPECT_TRUE(ConvertKeyCodeToText(key_code, modifiers, &text, &error_msg));
  return text;
}

}  // namespace

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Fails on bots: crbug.com/174962
#define MAYBE_KeyCodeToText DISABLED_KeyCodeToText
#else
#define MAYBE_KeyCodeToText KeyCodeToText
#endif

TEST(KeycodeTextConversionTest, MAYBE_KeyCodeToText) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  EXPECT_EQ("a", ConvertKeyCodeToTextNoError(ui::VKEY_A, 0));
  EXPECT_EQ("A",
      ConvertKeyCodeToTextNoError(ui::VKEY_A, kShiftKeyModifierMask));

  EXPECT_EQ("1", ConvertKeyCodeToTextNoError(ui::VKEY_1, 0));
  EXPECT_EQ("!",
      ConvertKeyCodeToTextNoError(ui::VKEY_1, kShiftKeyModifierMask));

  EXPECT_EQ(",", ConvertKeyCodeToTextNoError(ui::VKEY_OEM_COMMA, 0));
  EXPECT_EQ("<", ConvertKeyCodeToTextNoError(
      ui::VKEY_OEM_COMMA, kShiftKeyModifierMask));

  EXPECT_EQ("", ConvertKeyCodeToTextNoError(ui::VKEY_F1, 0));
  EXPECT_EQ("",
      ConvertKeyCodeToTextNoError(ui::VKEY_F1, kShiftKeyModifierMask));

  EXPECT_EQ("/", ConvertKeyCodeToTextNoError(ui::VKEY_DIVIDE, 0));
  EXPECT_EQ("/",
      ConvertKeyCodeToTextNoError(ui::VKEY_DIVIDE, kShiftKeyModifierMask));

  EXPECT_EQ("", ConvertKeyCodeToTextNoError(ui::VKEY_SHIFT, 0));
  EXPECT_EQ("",
      ConvertKeyCodeToTextNoError(ui::VKEY_SHIFT, kShiftKeyModifierMask));
}

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Fails on bots: crbug.com/174962
#define MAYBE_CharToKeyCode DISABLED_CharToKeyCode
#else
#define MAYBE_CharToKeyCode CharToKeyCode
#endif

TEST(KeycodeTextConversionTest, MAYBE_CharToKeyCode) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  CheckCharToKeyCode('a', ui::VKEY_A, 0);
  CheckCharToKeyCode('A', ui::VKEY_A, kShiftKeyModifierMask);

  CheckCharToKeyCode('1', ui::VKEY_1, 0);
  CheckCharToKeyCode('!', ui::VKEY_1, kShiftKeyModifierMask);

  CheckCharToKeyCode(',', ui::VKEY_OEM_COMMA, 0);
  CheckCharToKeyCode('<', ui::VKEY_OEM_COMMA, kShiftKeyModifierMask);

  CheckCharToKeyCode('/', ui::VKEY_OEM_2, 0);
  CheckCharToKeyCode('?', ui::VKEY_OEM_2, kShiftKeyModifierMask);

  CheckCantConvertChar(L'\u00E9');
  CheckCantConvertChar(L'\u2159');
}

#if BUILDFLAG(IS_WIN)
TEST(KeycodeTextConversionTest, NonShiftModifiers) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_GERMAN);
  int ctrl_and_alt = kControlKeyModifierMask | kAltKeyModifierMask;
  CheckCharToKeyCode('@', ui::VKEY_Q, ctrl_and_alt);
  EXPECT_EQ("@", ConvertKeyCodeToTextNoError(ui::VKEY_Q, ctrl_and_alt));
}

TEST(KeycodeTextConversionTest, NonEnglish) {
  // For Greek and Russian keyboard layouts, which are very different from
  // QWERTY, Windows just uses virtual key codes that match the QWERTY layout,
  // and translates them to other characters.  If we wanted to test something
  // like German, whose layout is very similar to QWERTY, we'd need to be
  // careful, as in this case Windows maps the keyboard scan codes to the
  // appropriate (different) VKEYs instead of mapping the VKEYs to different
  // characters.
  {
    ui::ScopedKeyboardLayout greek_layout(ui::KEYBOARD_LAYOUT_GREEK);
    CheckCharToKeyCode(';', ui::VKEY_Q, 0);
    EXPECT_EQ(";", ConvertKeyCodeToTextNoError(ui::VKEY_Q, 0));
  }
  {
    // Regression test for chromedriver bug #405.
    ui::ScopedKeyboardLayout russian_layout(ui::KEYBOARD_LAYOUT_RUSSIAN);
    CheckCharToKeyCode(L'\u0438', ui::VKEY_B, 0);
    EXPECT_EQ(base::WideToUTF8(L"\u0438"),
              ConvertKeyCodeToTextNoError(ui::VKEY_B, 0));
  }
}
#endif
