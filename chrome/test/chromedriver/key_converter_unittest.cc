// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <list>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/key_converter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/keyboard_layout.h"

namespace {

void CheckEvents(const base::string16& keys,
                 const std::list<KeyEvent>& expected_events,
                 bool release_modifiers,
                 int expected_modifiers) {
  int modifiers = 0;
  std::list<KeyEvent> events;
  EXPECT_EQ(kOk, ConvertKeysToKeyEvents(keys, release_modifiers,
                                        &modifiers, &events).code());
  EXPECT_EQ(expected_events.size(), events.size());
  auto expected = expected_events.begin();
  std::list<KeyEvent>::const_iterator actual = events.begin();
  while (expected != expected_events.end() && actual != events.end()) {
    EXPECT_EQ(expected->type, actual->type);
    EXPECT_EQ(expected->modifiers, actual->modifiers);
    EXPECT_EQ(expected->modified_text, actual->modified_text);
    EXPECT_EQ(expected->unmodified_text, actual->unmodified_text);
    EXPECT_EQ(expected->key_code, actual->key_code);

    ++expected;
    ++actual;
  }
  EXPECT_EQ(expected_modifiers, modifiers);
}

void CheckEventsReleaseModifiers(const base::string16& keys,
                                 const std::list<KeyEvent>& expected_events) {
  CheckEvents(keys, expected_events, true /* release_modifier */,
      0 /* expected_modifiers */);
}

void CheckEventsReleaseModifiers(const std::string& keys,
                                 std::list<KeyEvent>& expected_events) {
  CheckEventsReleaseModifiers(base::UTF8ToUTF16(keys), expected_events);
}

void CheckNonShiftChar(ui::KeyboardCode key_code, char character) {
  int modifiers = 0;
  std::string char_string;
  char_string.push_back(character);
  std::list<KeyEvent> events;
  EXPECT_EQ(kOk, ConvertKeysToKeyEvents(base::ASCIIToUTF16(char_string),
                                        true /* release_modifiers*/,
                                        &modifiers, &events).code());
  ASSERT_EQ(3u, events.size()) << "Char: " << character;
  std::list<KeyEvent>::const_iterator it = events.begin();
  EXPECT_EQ(key_code, it->key_code) << "Char: " << character;
  ++it;  // Move to the second event.
  ASSERT_EQ(1u, it->modified_text.length()) << "Char: " << character;
  ASSERT_EQ(1u, it->unmodified_text.length()) << "Char: " << character;
  EXPECT_EQ(character, it->modified_text[0]) << "Char: " << character;
  EXPECT_EQ(character, it->unmodified_text[0]) << "Char: " << character;
  ++it;  // Move to the third event.
  EXPECT_EQ(key_code, it->key_code) << "Char: " << character;
}

void CheckShiftChar(ui::KeyboardCode key_code, char character, char lower) {
  int modifiers = 0;
  std::string char_string;
  char_string.push_back(character);
  std::list<KeyEvent> events;
  EXPECT_EQ(kOk, ConvertKeysToKeyEvents(base::ASCIIToUTF16(char_string),
                                        true /* release_modifiers*/,
                                        &modifiers, &events).code());
  ASSERT_EQ(5u, events.size()) << "Char: " << character;
  std::list<KeyEvent>::const_iterator it = events.begin();
  EXPECT_EQ(ui::VKEY_SHIFT, it->key_code) << "Char: " << character;
  ++it;  // Move to second event.
  EXPECT_EQ(key_code, it->key_code) << "Char: " << character;
  ++it;  // Move to third event.
  ASSERT_EQ(1u, it->modified_text.length()) << "Char: " << character;
  ASSERT_EQ(1u, it->unmodified_text.length()) << "Char: " << character;
  EXPECT_EQ(character, it->modified_text[0]) << "Char: " << character;
  EXPECT_EQ(lower, it->unmodified_text[0]) << "Char: " << character;
  ++it;  // Move to fourth event.
  EXPECT_EQ(key_code, it->key_code) << "Char: " << character;
  ++it;  // Move to fifth event.
  EXPECT_EQ(ui::VKEY_SHIFT, it->key_code) << "Char: " << character;
}

}  // namespace

TEST(KeyConverter, SingleChar) {
  KeyEventBuilder builder;
  std::list<KeyEvent> key_events;
  builder.SetText("h", "h")->SetKeyCode(ui::VKEY_H)->Generate(&key_events);
  CheckEventsReleaseModifiers("h", key_events);
}

TEST(KeyConverter, SingleNumber) {
  KeyEventBuilder builder;
  std::list<KeyEvent> key_events;
  builder.SetText("1", "1")->SetKeyCode(ui::VKEY_1)->Generate(&key_events);
  CheckEventsReleaseModifiers("1", key_events);
}

TEST(KeyConverter, MultipleChars) {
  KeyEventBuilder builder;
  std::list<KeyEvent> key_events;
  builder.SetText("h", "h")->SetKeyCode(ui::VKEY_H)->Generate(&key_events);
  builder.SetText("e", "e")->SetKeyCode(ui::VKEY_E)->Generate(&key_events);
  builder.SetText("y", "y")->SetKeyCode(ui::VKEY_Y)->Generate(&key_events);
  CheckEventsReleaseModifiers("hey", key_events);
}

TEST(KeyConverter, WebDriverSpecialChar) {
  KeyEventBuilder builder;
  std::list<KeyEvent> key_events;
  builder.SetKeyCode(ui::VKEY_SPACE)->SetText(" ", " ")->Generate(&key_events);
  base::string16 keys;
  keys.push_back(static_cast<base::char16>(0xE00DU));
  CheckEventsReleaseModifiers(keys, key_events);
}

TEST(KeyConverter, WebDriverSpecialNonCharKey) {
  KeyEventBuilder builder;
  std::list<KeyEvent> key_events;
  builder.SetKeyCode(ui::VKEY_F1)->Generate(&key_events);
  base::string16 keys;
  keys.push_back(static_cast<base::char16>(0xE031U));
  CheckEventsReleaseModifiers(keys, key_events);
}

TEST(KeyConverter, FrenchKeyOnEnglishLayout) {
  KeyEventBuilder builder;
  std::string e_acute = base::WideToUTF8(L"\u00E9");
  std::list<KeyEvent> key_events;
  builder.SetText(e_acute, e_acute)->Generate(&key_events);
  CheckEventsReleaseModifiers(base::WideToUTF16(L"\u00E9"), key_events);
}

#if defined(OS_WIN)
TEST(KeyConverter, NeedsCtrlAndAlt) {
  KeyEventBuilder ctrl_builder;
  ctrl_builder.SetKeyCode(ui::VKEY_CONTROL);

  KeyEventBuilder alt_builder;
  alt_builder.SetKeyCode(ui::VKEY_MENU);

  KeyEventBuilder q_builder;
  q_builder.SetModifiers(kControlKeyModifierMask | kAltKeyModifierMask)
      ->SetKeyCode(ui::VKEY_Q)
      ->SetText("q", "@");

  std::list<KeyEvent> key_events;
  key_events.push_back(ctrl_builder.SetType(kRawKeyDownEventType)->Build());
  key_events.push_back(alt_builder.SetType(kRawKeyDownEventType)->Build());
  q_builder.Generate(&key_events);
  key_events.push_back(alt_builder.SetType(kKeyUpEventType)->Build());
  key_events.push_back(ctrl_builder.SetType(kKeyUpEventType)->Build());

  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_GERMAN);
  CheckEventsReleaseModifiers("@", key_events);
}
#endif

TEST(KeyConverter, UppercaseCharDoesShift) {
  KeyEventBuilder shift_builder;
  shift_builder.SetKeyCode(ui::VKEY_SHIFT);
  KeyEventBuilder a_builder;
  a_builder.SetKeyCode(ui::VKEY_A)
      ->SetModifiers(kShiftKeyModifierMask)
      ->SetText("a", "A");
  std::list<KeyEvent> key_events;
  key_events.push_back(shift_builder.SetType(kRawKeyDownEventType)->Build());
  a_builder.Generate(&key_events);
  key_events.push_back(shift_builder.SetType(kKeyUpEventType)->Build());
  CheckEventsReleaseModifiers("A", key_events);
}

TEST(KeyConverter, UppercaseSymbolCharDoesShift) {
  KeyEventBuilder shift_builder;
  shift_builder.SetKeyCode(ui::VKEY_SHIFT);
  KeyEventBuilder one_builder;
  one_builder.SetModifiers(kShiftKeyModifierMask)
      ->SetKeyCode(ui::VKEY_1)
      ->SetText("1", "!");
  std::list<KeyEvent> key_events;
  key_events.push_back(shift_builder.SetType(kRawKeyDownEventType)->Build());
  one_builder.Generate(&key_events);
  key_events.push_back(shift_builder.SetType(kKeyUpEventType)->Build());
  CheckEventsReleaseModifiers("!", key_events);
}

TEST(KeyConverter, UppercaseCharUsesShiftOnlyIfNecessary) {
  std::list<KeyEvent> key_events;
  KeyEventBuilder shift_builder;
  key_events.push_back(shift_builder.SetType(kRawKeyDownEventType)
                           ->SetKeyCode(ui::VKEY_SHIFT)
                           ->SetModifiers(kShiftKeyModifierMask)
                           ->Build());
  KeyEventBuilder builder;
  builder.SetModifiers(kShiftKeyModifierMask);
  builder.SetKeyCode(ui::VKEY_A)->SetText("a", "A")->Generate(&key_events);
  builder.SetKeyCode(ui::VKEY_B)->SetText("b", "B")->Generate(&key_events);
  builder.SetKeyCode(ui::VKEY_C)->SetText("c", "C")->Generate(&key_events);
  key_events.push_back(
      shift_builder.SetType(kKeyUpEventType)->SetModifiers(0)->Build());
  base::string16 keys;
  keys.push_back(static_cast<base::char16>(0xE008U));
  keys.append(base::UTF8ToUTF16("aBc"));
  CheckEventsReleaseModifiers(keys, key_events);
}

TEST(KeyConverter, ToggleModifiers) {
  std::list<KeyEvent> key_events;
  KeyEventBuilder builder;
  key_events.push_back(builder.SetType(kRawKeyDownEventType)
                           ->SetKeyCode(ui::VKEY_SHIFT)
                           ->SetModifiers(kShiftKeyModifierMask)
                           ->Build());
  key_events.push_back(
      builder.SetType(kKeyUpEventType)->SetModifiers(0)->Build());
  key_events.push_back(builder.SetType(kRawKeyDownEventType)
                           ->SetKeyCode(ui::VKEY_CONTROL)
                           ->SetModifiers(kControlKeyModifierMask)
                           ->Build());
  key_events.push_back(
      builder.SetType(kKeyUpEventType)->SetModifiers(0)->Build());
  key_events.push_back(builder.SetType(kRawKeyDownEventType)
                           ->SetKeyCode(ui::VKEY_MENU)
                           ->SetModifiers(kAltKeyModifierMask)
                           ->Build());
  key_events.push_back(
      builder.SetType(kKeyUpEventType)->SetModifiers(0)->Build());
  key_events.push_back(builder.SetType(kRawKeyDownEventType)
                           ->SetKeyCode(ui::VKEY_COMMAND)
                           ->SetModifiers(kMetaKeyModifierMask)
                           ->Build());
  key_events.push_back(
      builder.SetType(kKeyUpEventType)->SetModifiers(0)->Build());
  base::string16 keys;
  keys.push_back(static_cast<base::char16>(0xE008U));
  keys.push_back(static_cast<base::char16>(0xE008U));
  keys.push_back(static_cast<base::char16>(0xE009U));
  keys.push_back(static_cast<base::char16>(0xE009U));
  keys.push_back(static_cast<base::char16>(0xE00AU));
  keys.push_back(static_cast<base::char16>(0xE00AU));
  keys.push_back(static_cast<base::char16>(0xE03DU));
  keys.push_back(static_cast<base::char16>(0xE03DU));
  CheckEventsReleaseModifiers(keys, key_events);
}

#if defined(OS_WIN)
// https://code.google.com/p/chromedriver/issues/detail?id=546
#define MAYBE_AllShorthandKeys DISABLED_AllShorthandKeys
#else
#define MAYBE_AllShorthandKeys AllShorthandKeys
#endif

TEST(KeyConverter, MAYBE_AllShorthandKeys) {
  KeyEventBuilder builder;
  std::list<KeyEvent> key_events;
  builder.SetKeyCode(ui::VKEY_RETURN)
      ->SetText("\r", "\r")
      ->Generate(&key_events);
  builder.Generate(&key_events);
  builder.SetKeyCode(ui::VKEY_TAB);
#if defined(USE_AURA) || defined(OS_LINUX)
  builder.SetText("\t", "\t")->Generate(&key_events);
#else
  builder.SetText(std::string(), std::string());
  key_events.push_back(builder.SetType(kRawKeyDownEventType)->Build());
  key_events.push_back(builder.SetType(kKeyUpEventType)->Build());
#endif
  builder.SetKeyCode(ui::VKEY_BACK);
#if defined(USE_AURA) || defined(OS_LINUX)
  builder.SetText("\b", "\b")->Generate(&key_events);
#else
  builder.SetText(std::string(), std::string());
  key_events.push_back(builder.SetType(kRawKeyDownEventType)->Build());
  key_events.push_back(builder.SetType(kKeyUpEventType)->Build());
#endif
  builder.SetKeyCode(ui::VKEY_SPACE)->SetText(" ", " ")->Generate(&key_events);
  CheckEventsReleaseModifiers("\n\r\n\t\b ", key_events);
}

#if defined(OS_LINUX)
// Fails on bots: crbug.com/174962
#define MAYBE_AllEnglishKeyboardSymbols DISABLED_AllEnglishKeyboardSymbols
#else
#define MAYBE_AllEnglishKeyboardSymbols AllEnglishKeyboardSymbols
#endif

TEST(KeyConverter, MAYBE_AllEnglishKeyboardSymbols) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);
  base::string16 keys;
  const ui::KeyboardCode kSymbolKeyCodes[] = {
      ui::VKEY_OEM_3,
      ui::VKEY_OEM_MINUS,
      ui::VKEY_OEM_PLUS,
      ui::VKEY_OEM_4,
      ui::VKEY_OEM_6,
      ui::VKEY_OEM_5,
      ui::VKEY_OEM_1,
      ui::VKEY_OEM_7,
      ui::VKEY_OEM_COMMA,
      ui::VKEY_OEM_PERIOD,
      ui::VKEY_OEM_2};
  std::string kLowerSymbols = "`-=[]\\;',./";
  std::string kUpperSymbols = "~_+{}|:\"<>?";
  for (size_t i = 0; i < kLowerSymbols.length(); ++i)
    CheckNonShiftChar(kSymbolKeyCodes[i], kLowerSymbols[i]);
  for (size_t i = 0; i < kUpperSymbols.length(); ++i)
    CheckShiftChar(kSymbolKeyCodes[i], kUpperSymbols[i], kLowerSymbols[i]);
}

TEST(KeyConverter, AllEnglishKeyboardTextChars) {
  std::string kLowerChars = "0123456789abcdefghijklmnopqrstuvwxyz";
  std::string kUpperChars = ")!@#$%^&*(ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  for (size_t i = 0; i < kLowerChars.length(); ++i) {
    int offset = 0;
    if (i < 10)
      offset = ui::VKEY_0;
    else
      offset = ui::VKEY_0 + 7;
    ui::KeyboardCode expected_code = static_cast<ui::KeyboardCode>(offset + i);
    CheckNonShiftChar(expected_code, kLowerChars[i]);
  }
  for (size_t i = 0; i < kUpperChars.length(); ++i) {
    int offset = 0;
    if (i < 10)
      offset = ui::VKEY_0;
    else
      offset = ui::VKEY_0 + 7;
    ui::KeyboardCode expected_code = static_cast<ui::KeyboardCode>(offset + i);
    CheckShiftChar(expected_code, kUpperChars[i], kLowerChars[i]);
  }
}

#if defined(OS_LINUX) || defined(OS_WIN)
// https://code.google.com/p/chromedriver/issues/detail?id=240
// https://code.google.com/p/chromedriver/issues/detail?id=546
#define MAYBE_AllSpecialWebDriverKeysOnEnglishKeyboard \
    DISABLED_AllSpecialWebDriverKeysOnEnglishKeyboard
#else
#define MAYBE_AllSpecialWebDriverKeysOnEnglishKeyboard \
    AllSpecialWebDriverKeysOnEnglishKeyboard
#endif

TEST(KeyConverter, MAYBE_AllSpecialWebDriverKeysOnEnglishKeyboard) {
  const char kTextForKeys[] = {
#if defined(USE_AURA) || defined(OS_LINUX)
      0, 0, 0, '\b', '\t', 0, '\r', '\r', 0, 0, 0, 0, 0x1B,
      ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x7F, ';', '=',
#else
      0, 0, 0, 0, 0, 0, '\r', '\r', 0, 0, 0, 0, 0,
      ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ';', '=',
#endif
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
      '*', '+', ',', '-', '.', '/'};
  for (size_t i = 0; i <= 0x3D; ++i) {
    if (i > 0x29 && i < 0x31)
      continue;
    base::string16 keys;
    int modifiers = 0;
    keys.push_back(0xE000U + i);
    std::list<KeyEvent> events;
    if (i == 1) {
      EXPECT_NE(kOk, ConvertKeysToKeyEvents(keys,
                                            true /* release_modifiers*/,
                                            &modifiers, &events).code())
          << "Index: " << i;
      EXPECT_EQ(0u, events.size()) << "Index: " << i;
    } else {
      EXPECT_EQ(kOk, ConvertKeysToKeyEvents(keys,
                                            true /* release_modifiers */,
                                            &modifiers, &events).code())
          << "Index: " << i;
      if (i == 0) {
        EXPECT_EQ(0u, events.size()) << "Index: " << i;
      } else if (i >= arraysize(kTextForKeys) || kTextForKeys[i] == 0) {
        EXPECT_EQ(2u, events.size()) << "Index: " << i;
      } else {
        ASSERT_EQ(3u, events.size()) << "Index: " << i;
        std::list<KeyEvent>::const_iterator it = events.begin();
        ++it;  // Move to the second event.
        ASSERT_EQ(1u, it->unmodified_text.length()) << "Index: " << i;
        EXPECT_EQ(kTextForKeys[i], it->unmodified_text[0])
            << "Index: " << i;
      }
    }
  }
}

TEST(KeyConverter, ModifiersState) {
  KeyEventBuilder builder;
  builder.SetType(kRawKeyDownEventType);
  std::list<KeyEvent> key_events;
  key_events.push_back(builder.SetKeyCode(ui::VKEY_SHIFT)
                           ->AddModifiers(kShiftKeyModifierMask)
                           ->Build());
  key_events.push_back(builder.SetKeyCode(ui::VKEY_CONTROL)
                           ->AddModifiers(kControlKeyModifierMask)
                           ->Build());
  key_events.push_back(builder.SetKeyCode(ui::VKEY_MENU)
                           ->AddModifiers(kAltKeyModifierMask)
                           ->Build());
  key_events.push_back(builder.SetKeyCode(ui::VKEY_COMMAND)
                           ->AddModifiers(kMetaKeyModifierMask)
                           ->Build());

  base::string16 keys;
  keys.push_back(static_cast<base::char16>(0xE008U));
  keys.push_back(static_cast<base::char16>(0xE009U));
  keys.push_back(static_cast<base::char16>(0xE00AU));
  keys.push_back(static_cast<base::char16>(0xE03DU));

  CheckEvents(keys, key_events, false /* release_modifiers */,
              kShiftKeyModifierMask | kControlKeyModifierMask |
                  kAltKeyModifierMask | kMetaKeyModifierMask);
}

TEST(KeyConverter, ReleaseModifiers) {
  std::list<KeyEvent> key_events;
  KeyEventBuilder builder;
  key_events.push_back(builder.SetType(kRawKeyDownEventType)
                           ->SetKeyCode(ui::VKEY_SHIFT)
                           ->AddModifiers(kShiftKeyModifierMask)
                           ->Build());
  key_events.push_back(builder.SetKeyCode(ui::VKEY_CONTROL)
                           ->AddModifiers(kControlKeyModifierMask)
                           ->Build());
  key_events.push_back(builder.SetType(kKeyUpEventType)
                           ->SetKeyCode(ui::VKEY_SHIFT)
                           ->SetModifiers(0)
                           ->Build());
  key_events.push_back(builder.SetKeyCode(ui::VKEY_CONTROL)->Build());
  base::string16 keys;
  keys.push_back(static_cast<base::char16>(0xE008U));
  keys.push_back(static_cast<base::char16>(0xE009U));

  CheckEvents(keys, key_events, true /* release_modifiers */, 0);
}
