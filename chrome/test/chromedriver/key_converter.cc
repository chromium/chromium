// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/key_converter.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/keycode_text_conversion.h"

namespace {

struct ModifierMaskAndKeyCode {
  int mask;
  ui::KeyboardCode key_code;
};

const ModifierMaskAndKeyCode kModifiers[] = {
  { kShiftKeyModifierMask, ui::VKEY_SHIFT },
  { kControlKeyModifierMask, ui::VKEY_CONTROL },
  { kAltKeyModifierMask, ui::VKEY_MENU }
};

// Ordered list of all the key codes corresponding to special WebDriver keys.
// These keys are "special" in the sense that their code points are defined by
// the W3C spec (https://w3c.github.io/webdriver/#dfn-normalised-key-value),
// and are in the Unicode Private Use Area. All other keys have their code
// points defined by the Unicode standard.
const ui::KeyboardCode kSpecialWebDriverKeys[] = {
    ui::VKEY_UNKNOWN,   // \uE000
    ui::VKEY_CANCEL,  // \uE001
    ui::VKEY_HELP,
    ui::VKEY_BACK,
    ui::VKEY_TAB,
    ui::VKEY_CLEAR,
    ui::VKEY_RETURN,
    ui::VKEY_RETURN,
    ui::VKEY_SHIFT,
    ui::VKEY_CONTROL,
    ui::VKEY_MENU,
    ui::VKEY_PAUSE,
    ui::VKEY_ESCAPE,
    ui::VKEY_SPACE,
    ui::VKEY_PRIOR,    // page up
    ui::VKEY_NEXT,     // page down
    ui::VKEY_END,      // \uE010
    ui::VKEY_HOME,
    ui::VKEY_LEFT,
    ui::VKEY_UP,
    ui::VKEY_RIGHT,
    ui::VKEY_DOWN,
    ui::VKEY_INSERT,
    ui::VKEY_DELETE,
    ui::VKEY_OEM_1,     // semicolon
    ui::VKEY_OEM_PLUS,  // equals
    ui::VKEY_NUMPAD0,
    ui::VKEY_NUMPAD1,
    ui::VKEY_NUMPAD2,
    ui::VKEY_NUMPAD3,
    ui::VKEY_NUMPAD4,
    ui::VKEY_NUMPAD5,
    ui::VKEY_NUMPAD6,   // \uE020
    ui::VKEY_NUMPAD7,
    ui::VKEY_NUMPAD8,
    ui::VKEY_NUMPAD9,
    ui::VKEY_MULTIPLY,
    ui::VKEY_ADD,
    ui::VKEY_OEM_COMMA,
    ui::VKEY_SUBTRACT,
    ui::VKEY_DECIMAL,
    ui::VKEY_DIVIDE,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,   // \uE030
    ui::VKEY_F1,
    ui::VKEY_F2,
    ui::VKEY_F3,
    ui::VKEY_F4,
    ui::VKEY_F5,
    ui::VKEY_F6,
    ui::VKEY_F7,
    ui::VKEY_F8,
    ui::VKEY_F9,
    ui::VKEY_F10,
    ui::VKEY_F11,
    ui::VKEY_F12,
    ui::VKEY_LWIN,      // meta
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_DBE_DBCSCHAR,  // \uE040 ZenkakuHankaku
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_RSHIFT,    // \uE050
    ui::VKEY_RCONTROL,
    ui::VKEY_RMENU,
    ui::VKEY_RWIN,      // meta
    ui::VKEY_PRIOR,     // page up
    ui::VKEY_NEXT,      // page down
    ui::VKEY_END,
    ui::VKEY_HOME,
    ui::VKEY_LEFT,
    ui::VKEY_UP,
    ui::VKEY_RIGHT,
    ui::VKEY_DOWN,
    ui::VKEY_INSERT,
    ui::VKEY_DELETE,
};

const base::char16 kWebDriverNullKey = 0xE000U;
const base::char16 kWebDriverShiftKey = 0xE008U;
const base::char16 kWebDriverControlKey = 0xE009U;
const base::char16 kWebDriverAltKey = 0xE00AU;
const base::char16 kWebDriverCommandKey = 0xE03DU;
const base::char16 kWebDriverRightShiftKey = 0xE050U;
const base::char16 kWebDriverRightControlKey = 0xE051U;
const base::char16 kWebDriverRightAltKey = 0xE052U;
const base::char16 kWebDriverRightCommandKey = 0xE053U;

// Returns whether the given key code has a corresponding printable char.
// Notice: The given key code should be a special WebDriver key code.
bool IsSpecialKeyPrintable(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_TAB || key_code == ui::VKEY_SPACE ||
      key_code == ui::VKEY_OEM_1 || key_code == ui::VKEY_OEM_PLUS ||
      key_code == ui::VKEY_OEM_COMMA ||
      (key_code >= ui::VKEY_NUMPAD0 && key_code <= ui::VKEY_DIVIDE);
}

// Returns whether the given key is a WebDriver key modifier.
bool IsModifierKey(base::char16 key) {
  switch (key) {
    case kWebDriverShiftKey:
    case kWebDriverControlKey:
    case kWebDriverAltKey:
    case kWebDriverCommandKey:
    case kWebDriverRightShiftKey:
    case kWebDriverRightControlKey:
    case kWebDriverRightAltKey:
    case kWebDriverRightCommandKey:
      return true;
    default:
      return false;
  }
}

// Gets the key code associated with |key|, if it is a special WebDriver key.
// Returns whether |key| is a special WebDriver key. If true, |key_code| will
// be set.
bool KeyCodeFromSpecialWebDriverKey(base::char16 key,
                                    ui::KeyboardCode* key_code) {
  int index = static_cast<int>(key) - 0xE000U;
  bool is_special_key =
      index >= 0 && index < static_cast<int>(base::size(kSpecialWebDriverKeys));
  if (is_special_key)
    *key_code = kSpecialWebDriverKeys[index];
  return is_special_key;
}

// Gets the key code associated with |key_utf16|, if it is a special shorthand
// key. Shorthand keys are common text equivalents for keys, such as the newline
// character, which is shorthand for the return key. Returns whether |key| is
// a shorthand key. If true, |key_code| will be set and |client_should_skip|
// will be set to whether the key should be skipped.
bool KeyCodeFromShorthandKey(base::char16 key_utf16,
                             ui::KeyboardCode* key_code,
                             bool* client_should_skip) {
  base::string16 key_str_utf16;
  key_str_utf16.push_back(key_utf16);
  std::string key_str_utf8 = base::UTF16ToUTF8(key_str_utf16);
  if (key_str_utf8.length() != 1)
    return false;
  bool should_skip = false;
  char key = key_str_utf8[0];
  if (key == '\n') {
    *key_code = ui::VKEY_RETURN;
  } else if (key == '\t') {
    *key_code = ui::VKEY_TAB;
  } else if (key == '\b') {
    *key_code = ui::VKEY_BACK;
  } else if (key == ' ') {
    *key_code = ui::VKEY_SPACE;
  } else if (key == '\r') {
    *key_code = ui::VKEY_UNKNOWN;
    should_skip = true;
  } else {
    return false;
  }
  *client_should_skip = should_skip;
  return true;
}

// The "normalised key value" table from W3C spec
// (https://w3c.github.io/webdriver/#dfn-normalised-key-value).
// The code point starts at \uE000 and must increase by 1 with each row,
// with placeholders (empty strings) used for unassigned code points.
const int kNormalisedKeyValueBase = 0xE000;
const char* const kNormalisedKeyValue[] = {
    "Unidentified",  // \uE000
    "Cancel",        // \uE001
    "Help",          // \uE002
    "Backspace",     // \uE003
    "Tab",           // \uE004
    "Clear",         // \uE005
    "Return",        // \uE006
    "Enter",         // \uE007
    "Shift",         // \uE008
    "Control",       // \uE009
    "Alt",           // \uE00A
    "Pause",         // \uE00B
    "Escape",        // \uE00C
    " ",             // \uE00D
    "PageUp",        // \uE00E
    "PageDown",      // \uE00F
    "End",           // \uE010
    "Home",          // \uE011
    "ArrowLeft",     // \uE012
    "ArrowUp",       // \uE013
    "ArrowRight",    // \uE014
    "ArrowDown",     // \uE015
    "Insert",        // \uE016
    "Delete",        // \uE017
    ";",             // \uE018
    "=",             // \uE019
    "0",             // \uE01A
    "1",             // \uE01B
    "2",             // \uE01C
    "3",             // \uE01D
    "4",             // \uE01E
    "5",             // \uE01F
    "6",             // \uE020
    "7",             // \uE021
    "8",             // \uE022
    "9",             // \uE023
    "*",             // \uE024
    "+",             // \uE025
    ",",             // \uE026
    "-",             // \uE027
    ".",             // \uE028
    "/",             // \uE029
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "F1",            // \uE031
    "F2",            // \uE032
    "F3",            // \uE033
    "F4",            // \uE034
    "F5",            // \uE035
    "F6",            // \uE036
    "F7",            // \uE037
    "F8",            // \uE038
    "F9",            // \uE039
    "F10",           // \uE03A
    "F11",           // \uE03B
    "F12",           // \uE03C
    "Meta",          // \uE03D
    "",
    "",
    "ZenkakuHankaku", // \uE040
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "Shift",         // \uE050
    "Control",       // \uE051
    "Alt",           // \uE052
    "Meta",          // \uE053
    "PageUp",        // \uE054
    "PageDown",      // \uE055
    "End",           // \uE056
    "Home",          // \uE057
    "ArrowLeft",     // \uE058
    "ArrowUp",       // \uE059
    "ArrowRight",    // \uE05A
    "ArrowDown",     // \uE05B
    "Insert",        // \uE05C
    "Delete",        // \uE05D
};

// The "code for key" table (https://w3c.github.io/webdriver/#dfn-code),
// with the following modifications:
// * Fixed some inconsistencies in the original table.
//   See https://github.com/w3c/webdriver/pull/1384.
// * Replaced "OSLeft" and "OSRight" with "MetaLeft" and "MetaRight", to be
//   compatible with Chrome.
//   TODO(johnchen@chromium.org): Find a better way to handle this.
const struct {
  base::char16 key;
  base::char16 alternate_key;
  std::string code;
} kCodeForKey[] = {
    {'`',    '~',    "Backquote"},
    {'\\',   '|',    "Backslash"},
    {0xE003, 0,      "Backspace"},
    {'[',    '{',    "BracketLeft"},
    {']',    '}',    "BracketRight"},
    {',',    '<',    "Comma"},
    {'0',    ')',    "Digit0"},
    {'1',    '!',    "Digit1"},
    {'2',    '@',    "Digit2"},
    {'3',    '#',    "Digit3"},
    {'4',    '$',    "Digit4"},
    {'5',    '%',    "Digit5"},
    {'6',    '^',    "Digit6"},
    {'7',    '&',    "Digit7"},
    {'8',    '*',    "Digit8"},
    {'9',    '(',    "Digit9"},
    {'=',    '+',    "Equal"},
    {'<',    '>',    "IntlBackslash"},
    {'a',    'A',    "KeyA"},
    {'b',    'B',    "KeyB"},
    {'c',    'C',    "KeyC"},
    {'d',    'D',    "KeyD"},
    {'e',    'E',    "KeyE"},
    {'f',    'F',    "KeyF"},
    {'g',    'G',    "KeyG"},
    {'h',    'H',    "KeyH"},
    {'i',    'I',    "KeyI"},
    {'j',    'J',    "KeyJ"},
    {'k',    'K',    "KeyK"},
    {'l',    'L',    "KeyL"},
    {'m',    'M',    "KeyM"},
    {'n',    'N',    "KeyN"},
    {'o',    'O',    "KeyO"},
    {'p',    'P',    "KeyP"},
    {'q',    'Q',    "KeyQ"},
    {'r',    'R',    "KeyR"},
    {'s',    'S',    "KeyS"},
    {'t',    'T',    "KeyT"},
    {'u',    'U',    "KeyU"},
    {'v',    'V',    "KeyV"},
    {'w',    'W',    "KeyW"},
    {'x',    'X',    "KeyX"},
    {'y',    'Y',    "KeyY"},
    {'z',    'Z',    "KeyZ"},
    {'-',    '_',    "Minus"},
    {'.',    '>',    "Period"},
    {'\'',   '"',    "Quote"},
    {';',    ':',    "Semicolon"},
    {'/',    '?',    "Slash"},
    {0xE00A, 0,      "AltLeft"},
    {0xE052, 0,      "AltRight"},
    {0xE009, 0,      "ControlLeft"},
    {0xE051, 0,      "ControlRight"},
    {0xE006, 0,      "Enter"},
    {0xE03D, 0,      "MetaLeft"},
    {0xE053, 0,      "MetaRight"},
    {0xE008, 0,      "ShiftLeft"},
    {0xE050, 0,      "ShiftRight"},
    {' ',    0xE00D, "Space"},
    {0xE004, 0,      "Tab"},
    {0xE017, 0,      "Delete"},
    {0xE010, 0,      "End"},
    {0xE002, 0,      "Help"},
    {0xE011, 0,      "Home"},
    {0xE016, 0,      "Insert"},
    {0xE00F, 0,      "PageDown"},
    {0xE00E, 0,      "PageUp"},
    {0xE015, 0,      "ArrowDown"},
    {0xE012, 0,      "ArrowLeft"},
    {0xE014, 0,      "ArrowRight"},
    {0xE013, 0,      "ArrowUp"},
    {0xE00C, 0,      "Escape"},
    {0xE031, 0,      "F1"},
    {0xE032, 0,      "F2"},
    {0xE033, 0,      "F3"},
    {0xE034, 0,      "F4"},
    {0xE035, 0,      "F5"},
    {0xE036, 0,      "F6"},
    {0xE037, 0,      "F7"},
    {0xE038, 0,      "F8"},
    {0xE039, 0,      "F9"},
    {0xE03A, 0,      "F10"},
    {0xE03B, 0,      "F11"},
    {0xE03C, 0,      "F12"},
    {0xE01A, 0xE05C, "Numpad0"},
    {0xE01B, 0xE056, "Numpad1"},
    {0xE01C, 0xE05B, "Numpad2"},
    {0xE01D, 0xE055, "Numpad3"},
    {0xE01E, 0xE058, "Numpad4"},
    {0xE01F, 0,      "Numpad5"},
    {0xE020, 0xE05A, "Numpad6"},
    {0xE021, 0xE057, "Numpad7"},
    {0xE022, 0xE059, "Numpad8"},
    {0xE023, 0xE054, "Numpad9"},
    {0xE025, 0,      "NumpadAdd"},
    {0xE026, 0,      "NumpadComma"},
    {0xE028, 0xE05D, "NumpadDecimal"},
    {0xE029, 0,      "NumpadDivide"},
    {0xE007, 0,      "NumpadEnter"},
    {0xE024, 0,      "NumpadMultiply"},
    {0xE027, 0,      "NumpadSubtract"},
};

// The "key location for key" table from W3C spec
// (https://w3c.github.io/webdriver/#dfn-key-location). For simplicity, it is
// implemented as a few 'if' statements, instead of as a true table.
int GetKeyLocation(uint32_t code_point) {
  if (code_point >= 0xe007 && code_point <= 0xe00a)
    return 1;
  if (code_point >= 0xe01a && code_point <= 0xe029)
    return 3;
  if (code_point == 0xe03d)
    return 1;
  if (code_point >= 0xe050 && code_point <= 0xe053)
    return 2;
  if (code_point >= 0xe054 && code_point <= 0xe05d)
    return 3;
  return 0;
}

}  // namespace

Status ConvertKeysToKeyEvents(const base::string16& client_keys,
                              bool release_modifiers,
                              int* modifiers,
                              std::list<KeyEvent>* client_key_events) {
  std::list<KeyEvent> key_events;

  base::string16 keys = client_keys;
  // Add an implicit NULL character to the end of the input to depress all
  // modifiers.
  if (release_modifiers)
    keys.push_back(kWebDriverNullKey);

  int sticky_modifiers = *modifiers;
  for (size_t i = 0; i < keys.size(); ++i) {
    base::char16 key = keys[i];

    if (key == kWebDriverNullKey) {
      // Release all modifier keys and clear |stick_modifiers|.
      KeyEventBuilder builder;
      builder.SetType(kKeyUpEventType);
      if (sticky_modifiers & kShiftKeyModifierMask)
        key_events.push_back(builder.SetKeyCode(ui::VKEY_SHIFT)->Build());
      if (sticky_modifiers & kControlKeyModifierMask)
        key_events.push_back(builder.SetKeyCode(ui::VKEY_CONTROL)->Build());
      if (sticky_modifiers & kAltKeyModifierMask)
        key_events.push_back(builder.SetKeyCode(ui::VKEY_MENU)->Build());
      if (sticky_modifiers & kMetaKeyModifierMask)
        key_events.push_back(builder.SetKeyCode(ui::VKEY_COMMAND)->Build());
      sticky_modifiers = 0;
      continue;
    }
    if (IsModifierKey(key)) {
      // Press or release the modifier, and adjust |sticky_modifiers|.
      bool modifier_down = false;
      ui::KeyboardCode key_code = ui::VKEY_UNKNOWN;
      if (key == kWebDriverShiftKey || key == kWebDriverRightShiftKey) {
        sticky_modifiers ^= kShiftKeyModifierMask;
        modifier_down = (sticky_modifiers & kShiftKeyModifierMask) != 0;
        key_code = ui::VKEY_SHIFT;
      } else if (key == kWebDriverControlKey ||
                 key == kWebDriverRightControlKey) {
        sticky_modifiers ^= kControlKeyModifierMask;
        modifier_down = (sticky_modifiers & kControlKeyModifierMask) != 0;
        key_code = ui::VKEY_CONTROL;
      } else if (key == kWebDriverAltKey || key == kWebDriverRightAltKey) {
        sticky_modifiers ^= kAltKeyModifierMask;
        modifier_down = (sticky_modifiers & kAltKeyModifierMask) != 0;
        key_code = ui::VKEY_MENU;
      } else if (key == kWebDriverCommandKey ||
                 key == kWebDriverRightCommandKey) {
        sticky_modifiers ^= kMetaKeyModifierMask;
        modifier_down = (sticky_modifiers & kMetaKeyModifierMask) != 0;
        key_code = ui::VKEY_COMMAND;
      } else {
        return Status(kUnknownError, "unknown modifier key");
      }
      KeyEventBuilder builder;
      if (modifier_down)
        builder.SetType(kRawKeyDownEventType);
      else
        builder.SetType(kKeyUpEventType);
      key_events.push_back(builder.SetKeyCode(key_code)
                               ->SetModifiers(sticky_modifiers)
                               ->Build());
      continue;
    }

    ui::KeyboardCode key_code = ui::VKEY_UNKNOWN;
    std::string unmodified_text, modified_text;
    int all_modifiers = sticky_modifiers;

    // Get the key code, text, and modifiers for the given key.
    bool should_skip = false;
    bool is_special_key = KeyCodeFromSpecialWebDriverKey(key, &key_code);
    std::string error_msg;
    if (is_special_key ||
        KeyCodeFromShorthandKey(key, &key_code, &should_skip)) {
      if (should_skip)
        continue;
      if (key_code == ui::VKEY_UNKNOWN) {
        return Status(kUnknownError, base::StringPrintf(
            "unknown WebDriver key(%d) at string index (%" PRIuS ")",
            static_cast<int>(key),
            i));
      }
      if (key_code == ui::VKEY_RETURN) {
        // For some reason Chrome expects a carriage return for the return key.
        modified_text = unmodified_text = "\r";
      } else if (is_special_key && !IsSpecialKeyPrintable(key_code)) {
        // To prevent char event for special keys like DELETE.
        modified_text = unmodified_text = std::string();
      } else {
        // WebDriver assumes a numpad key should translate to the number,
        // which requires NumLock to be on with some platforms. This isn't
        // formally in the spec, but is expected by their tests.
        int webdriver_modifiers = 0;
        if (key_code >= ui::VKEY_NUMPAD0 && key_code <= ui::VKEY_NUMPAD9)
          webdriver_modifiers = kNumLockKeyModifierMask;
        if (!ConvertKeyCodeToText(
            key_code, webdriver_modifiers, &unmodified_text, &error_msg))
          return Status(kUnknownError, error_msg);
        if (!ConvertKeyCodeToText(
            key_code, all_modifiers | webdriver_modifiers, &modified_text,
            &error_msg))
          return Status(kUnknownError, error_msg);
      }
    } else {
      int necessary_modifiers = 0;
      ConvertCharToKeyCode(key, &key_code, &necessary_modifiers, &error_msg);
      if (!error_msg.empty())
        return Status(kUnknownError, error_msg);
      all_modifiers |= necessary_modifiers;
      if (key_code != ui::VKEY_UNKNOWN) {
        if (!ConvertKeyCodeToText(key_code, 0, &unmodified_text, &error_msg))
          return Status(kUnknownError, error_msg);
        if (!ConvertKeyCodeToText(
            key_code, all_modifiers, &modified_text, &error_msg))
          return Status(kUnknownError, error_msg);
        if (unmodified_text.empty() || modified_text.empty()) {
          // To prevent char event for special cases like CTRL + x (cut).
          unmodified_text.clear();
          modified_text.clear();
        }
      } else {
        // Do a best effort and use the raw key we were given.
        unmodified_text = base::UTF16ToUTF8(keys.substr(i, 1));
        modified_text = base::UTF16ToUTF8(keys.substr(i, 1));
      }
    }

    // Create the key events.
    bool necessary_modifiers[3];
    for (int i = 0; i < 3; ++i) {
      necessary_modifiers[i] =
          all_modifiers & kModifiers[i].mask &&
          !(sticky_modifiers & kModifiers[i].mask);
      if (necessary_modifiers[i]) {
        KeyEventBuilder builder;
        key_events.push_back(builder.SetType(kRawKeyDownEventType)
                                   ->SetKeyCode(kModifiers[i].key_code)
                                   ->SetModifiers(sticky_modifiers)
                                   ->Build());
      }
    }

    KeyEventBuilder builder;
    builder.SetModifiers(all_modifiers)
        ->SetText(unmodified_text, modified_text)
        ->SetKeyCode(key_code)
        ->Generate(&key_events);

    for (int i = 2; i > -1; --i) {
      if (necessary_modifiers[i]) {
        KeyEventBuilder builder;
        key_events.push_back(builder.SetType(kKeyUpEventType)
                                   ->SetKeyCode(kModifiers[i].key_code)
                                   ->SetModifiers(sticky_modifiers)
                                   ->Build());
      }
    }
  }
  client_key_events->swap(key_events);
  *modifiers = sticky_modifiers;
  return Status(kOk);
}

Status ConvertKeyActionToKeyEvent(const base::DictionaryValue* action_object,
                                  base::DictionaryValue* input_state,
                                  bool is_key_down,
                                  std::list<KeyEvent>* key_events) {
  std::string raw_key;
  if (!action_object->GetString("value", &raw_key))
    return Status(kUnknownError, "missing 'value'");

  int32_t char_index = 0;
  uint32_t code_point;
  base::ReadUnicodeCharacter(raw_key.c_str(), raw_key.size(), &char_index,
                             &code_point);

  std::string key;
  if (code_point >= kNormalisedKeyValueBase &&
      code_point < kNormalisedKeyValueBase + base::size(kNormalisedKeyValue)) {
    key = kNormalisedKeyValue[code_point - kNormalisedKeyValueBase];
  }
  if (key.size() == 0)
    key = raw_key;

  base::DictionaryValue* pressed;
  if (!input_state->GetDictionary("pressed", &pressed))
    return Status(kUnknownError, "missing 'pressed'");
  bool already_pressed = pressed->HasKey(key);
  if (!is_key_down && !already_pressed)
    return Status(kOk);

  std::string code;
  if (code_point != 0) {
    for (auto& mapping : kCodeForKey) {
      if (mapping.key == code_point || mapping.alternate_key == code_point) {
        code = mapping.code;
        break;
      }
    }
  }

  int modifiers;
  if (!input_state->GetInteger("modifiers", &modifiers))
    return Status(kUnknownError, "missing 'modifiers'");

  bool is_modifier_key = false;
  bool is_special_key = false;
  bool should_skip = false;
  std::string unmodified_text, modified_text;
  ui::KeyboardCode key_code = ui::VKEY_UNKNOWN;
  std::string error_msg;

  is_modifier_key = IsModifierKey(code_point);
  if (!is_modifier_key)
    is_special_key = KeyCodeFromSpecialWebDriverKey(code_point, &key_code);

  if (is_modifier_key) {
    int updated_modifier;
    if (code_point == kWebDriverShiftKey) {
      updated_modifier = kShiftKeyModifierMask;
      key_code = ui::VKEY_SHIFT;
    } else if (code_point == kWebDriverRightShiftKey) {
      updated_modifier = kShiftKeyModifierMask;
      key_code = ui::VKEY_RSHIFT;
    } else if (code_point == kWebDriverControlKey) {
      updated_modifier = kControlKeyModifierMask;
      key_code = ui::VKEY_CONTROL;
    } else if (code_point == kWebDriverRightControlKey) {
      updated_modifier = kControlKeyModifierMask;
      key_code = ui::VKEY_RCONTROL;
    } else if (code_point == kWebDriverAltKey) {
      updated_modifier = kAltKeyModifierMask;
      key_code = ui::VKEY_MENU;
    } else if (code_point == kWebDriverRightAltKey) {
      updated_modifier = kAltKeyModifierMask;
      key_code = ui::VKEY_RMENU;
    } else if (code_point == kWebDriverCommandKey) {
      updated_modifier = kMetaKeyModifierMask;
      key_code = ui::VKEY_COMMAND;
    } else if (code_point == kWebDriverRightCommandKey) {
      updated_modifier = kMetaKeyModifierMask;
      key_code = ui::VKEY_RWIN;
    } else {
      return Status(kUnknownError, "unknown modifier key");
    }

    if (is_key_down)
      modifiers |= updated_modifier;
    else
      modifiers &= ~updated_modifier;

    input_state->SetInteger("modifiers", modifiers);
  } else if (is_special_key ||
             KeyCodeFromShorthandKey(code_point, &key_code, &should_skip)) {
    if (should_skip)
      return Status(kOk);
    if (key_code == ui::VKEY_RETURN) {
      // For some reason Chrome expects a carriage return for the return key.
      modified_text = unmodified_text = "\r";
    } else if (is_special_key && !IsSpecialKeyPrintable(key_code)) {
      // To prevent char event for special keys like DELETE.
      modified_text = unmodified_text = std::string();
    } else {
      // WebDriver assumes a numpad key should translate to the number,
      // which requires NumLock to be on with some platforms. This isn't
      // formally in the spec, but is expected by their tests.
      int webdriver_modifiers = 0;
      if (key_code >= ui::VKEY_NUMPAD0 && key_code <= ui::VKEY_NUMPAD9)
        webdriver_modifiers = kNumLockKeyModifierMask;
      if (!ConvertKeyCodeToText(key_code, webdriver_modifiers, &unmodified_text,
                                &error_msg))
        return Status(kUnknownError, error_msg);
      if (!ConvertKeyCodeToText(key_code, modifiers | webdriver_modifiers,
                                &modified_text, &error_msg))
        return Status(kUnknownError, error_msg);
    }
  } else {
    int necessary_modifiers = 0;
    ConvertCharToKeyCode(code_point, &key_code, &necessary_modifiers,
                         &error_msg);
    if (!error_msg.empty())
      return Status(kUnknownError, error_msg);
    if (key_code != ui::VKEY_UNKNOWN) {
      modifiers |= necessary_modifiers;
      if (!ConvertKeyCodeToText(key_code, 0, &unmodified_text, &error_msg))
        return Status(kUnknownError, error_msg);
      if (!ConvertKeyCodeToText(key_code, modifiers, &modified_text,
                                &error_msg))
        return Status(kUnknownError, error_msg);
      if (unmodified_text.empty() || modified_text.empty()) {
        // To prevent char event for special cases like CTRL + x (cut).
        unmodified_text.clear();
        modified_text.clear();
      }
    } else {
      // Do a best effort and use the raw key we were given.
      unmodified_text = raw_key;
      modified_text = raw_key;
    }
  }

  if (is_key_down)
    pressed->SetBoolean(key, true);
  else
    pressed->Remove(key, nullptr);

  KeyEventBuilder builder;
  builder.SetKeyCode(key_code)
      ->SetModifiers(modifiers)
      ->SetLocation(GetKeyLocation(code_point))
      ->SetDefaultKey(key)
      ->SetCode(code)
      ->SetIsFromAction();
  if (!is_modifier_key)
    builder.SetText(unmodified_text, modified_text);
  if (is_key_down) {
    key_events->push_back(builder.SetType(kKeyDownEventType)->Build());
  } else {
    key_events->push_back(builder.SetType(kKeyUpEventType)->Build());
  }

  return Status(kOk);
}
