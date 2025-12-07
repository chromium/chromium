// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/input/web_input_event_builders_mac.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#include <IOKit/hidsystem/IOLLEvent.h>  // for NX_ constants
#include <stddef.h>

#import <string_view>

#include "base/apple/owned_objc.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#import "ui/events/test/cocoa_test_event_utils.h"

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using input::WebKeyboardEventBuilder;

namespace {

struct KeyMappingEntry {
  int mac_key_code;
  unichar character;
  int windows_key_code;
  ui::DomCode dom_code;
  ui::DomKey dom_key;
};

struct ModifierKey {
  int mac_key_code;
  int device_dependent_modifier_flag;
  int device_independent_modifier_flag;
};

// Modifier keys, grouped into left/right pairs.
const ModifierKey kModifierKeys[] = {
    // Left Shift
    {kVK_Shift, NX_DEVICELSHIFTKEYMASK, NSEventModifierFlagShift},
    // Right Shift
    {kVK_RightShift, NX_DEVICERSHIFTKEYMASK, NSEventModifierFlagShift},
    // Left Command
    {kVK_Command, NX_DEVICELCMDKEYMASK, NSEventModifierFlagCommand},
    // Right Command
    {kVK_RightCommand, NX_DEVICERCMDKEYMASK, NSEventModifierFlagCommand},
    // Left Option
    {kVK_Option, NX_DEVICELALTKEYMASK, NSEventModifierFlagOption},
    // Right Option
    {kVK_RightOption, NX_DEVICERALTKEYMASK, NSEventModifierFlagOption},
    // Left Control
    {kVK_Control, NX_DEVICELCTLKEYMASK, NSEventModifierFlagControl},
    // Right Control
    {kVK_RightControl, NX_DEVICERCTLKEYMASK, NSEventModifierFlagControl},
};

NSEvent* BuildFakeKeyEvent(NSUInteger key_code,
                           std::u16string_view character,
                           NSUInteger modifier_flags,
                           NSEventType event_type) {
  NSString* string = base::SysUTF16ToNSString(character);
  return [NSEvent keyEventWithType:event_type
                          location:NSZeroPoint
                     modifierFlags:modifier_flags
                         timestamp:0.0
                      windowNumber:0
                           context:nil
                        characters:string
       charactersIgnoringModifiers:string
                         isARepeat:NO
                           keyCode:key_code];
}

NSEvent* BuildFakeKeyEvent(NSUInteger key_code,
                           char16_t code_point,
                           NSUInteger modifier_flags,
                           NSEventType event_type) {
  return BuildFakeKeyEvent(key_code, std::u16string_view(&code_point, 1),
                           modifier_flags, event_type);
}

NSEvent* BuildFakeMouseEvent(CGEventType mouse_type,
                             CGPoint location,
                             CGMouseButton button,
                             CGEventMouseSubtype subtype,
                             float rotation = 0.0,
                             float pressure = 0.0,
                             float tilt_x = 0.0,
                             float tilt_y = 0.0,
                             float tangential_pressure = 0.0,
                             NSUInteger button_number = 0) {
  base::apple::ScopedCFTypeRef<CGEventRef> cg_event_ref(CGEventCreateMouseEvent(
      /*source=*/nullptr, mouse_type, location, button));
  CGEventRef cg_event = cg_event_ref.get();
  CGEventSetIntegerValueField(cg_event, kCGMouseEventSubtype, subtype);
  CGEventSetDoubleValueField(cg_event, kCGTabletEventRotation, rotation);
  CGEventSetDoubleValueField(cg_event, kCGMouseEventPressure, pressure);
  CGEventSetDoubleValueField(cg_event, kCGTabletEventTiltX, tilt_x);
  CGEventSetDoubleValueField(cg_event, kCGTabletEventTiltY, tilt_y);
  CGEventSetDoubleValueField(cg_event, kCGTabletEventTangentialPressure,
                             tangential_pressure);
  CGEventSetIntegerValueField(cg_event, kCGMouseEventButtonNumber,
                              button_number);
  NSEvent* event = [NSEvent eventWithCGEvent:cg_event];
  return event;
}

}  // namespace

// Test that arrow keys don't have numpad modifier set.
TEST(WebInputEventBuilderMacTest, ArrowKeyNumPad) {
  // Left
  NSEvent* mac_event =
      BuildFakeKeyEvent(kVK_LeftArrow, NSLeftArrowFunctionKey,
                        NSEventModifierFlagNumericPad, NSEventTypeKeyDown);
  WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.GetModifiers());
  EXPECT_EQ(ui::DomCode::ARROW_LEFT,
            static_cast<ui::DomCode>(web_event.dom_code));
  EXPECT_EQ(ui::DomKey::ARROW_LEFT, web_event.dom_key);

  // Right
  mac_event =
      BuildFakeKeyEvent(kVK_RightArrow, NSRightArrowFunctionKey,
                        NSEventModifierFlagNumericPad, NSEventTypeKeyDown);
  web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.GetModifiers());
  EXPECT_EQ(ui::DomCode::ARROW_RIGHT,
            static_cast<ui::DomCode>(web_event.dom_code));
  EXPECT_EQ(ui::DomKey::ARROW_RIGHT, web_event.dom_key);

  // Down
  mac_event =
      BuildFakeKeyEvent(kVK_DownArrow, NSDownArrowFunctionKey,
                        NSEventModifierFlagNumericPad, NSEventTypeKeyDown);
  web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.GetModifiers());
  EXPECT_EQ(ui::DomCode::ARROW_DOWN,
            static_cast<ui::DomCode>(web_event.dom_code));
  EXPECT_EQ(ui::DomKey::ARROW_DOWN, web_event.dom_key);

  // Up
  mac_event =
      BuildFakeKeyEvent(kVK_UpArrow, NSUpArrowFunctionKey,
                        NSEventModifierFlagNumericPad, NSEventTypeKeyDown);
  web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.GetModifiers());
  EXPECT_EQ(ui::DomCode::ARROW_UP,
            static_cast<ui::DomCode>(web_event.dom_code));
  EXPECT_EQ(ui::DomKey::ARROW_UP, web_event.dom_key);
}

// Test that control sequence generate the correct vkey code.
TEST(WebInputEventBuilderMacTest, ControlSequence) {
  // Ctrl-[ generates escape.
  NSEvent* mac_event =
      BuildFakeKeyEvent(kVK_ANSI_LeftBracket, 0x1b, NSEventModifierFlagControl,
                        NSEventTypeKeyDown);
  WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(ui::VKEY_OEM_4, web_event.windows_key_code);
  EXPECT_EQ(ui::DomCode::BRACKET_LEFT,
            static_cast<ui::DomCode>(web_event.dom_code));
  // Will only pass on US layout.
  EXPECT_EQ(ui::DomKey::FromCharacter('['), web_event.dom_key);
}

// Test that numpad keys get mapped correctly.
TEST(WebInputEventBuilderMacTest, NumPadMapping) {
  KeyMappingEntry table[] = {
      {kVK_ANSI_KeypadDecimal, '.', ui::VKEY_DECIMAL,
       ui::DomCode::NUMPAD_DECIMAL, ui::DomKey::FromCharacter('.')},
      {kVK_ANSI_KeypadMultiply, '*', ui::VKEY_MULTIPLY,
       ui::DomCode::NUMPAD_MULTIPLY, ui::DomKey::FromCharacter('*')},
      {kVK_ANSI_KeypadPlus, '+', ui::VKEY_ADD, ui::DomCode::NUMPAD_ADD,
       ui::DomKey::FromCharacter('+')},
      {kVK_ANSI_KeypadClear, NSClearLineFunctionKey, ui::VKEY_CLEAR,
       ui::DomCode::NUM_LOCK, ui::DomKey::CLEAR},
      {kVK_ANSI_KeypadDivide, '/', ui::VKEY_DIVIDE, ui::DomCode::NUMPAD_DIVIDE,
       ui::DomKey::FromCharacter('/')},
      {kVK_ANSI_KeypadEnter, 3, ui::VKEY_RETURN, ui::DomCode::NUMPAD_ENTER,
       ui::DomKey::ENTER},
      {kVK_ANSI_KeypadMinus, '-', ui::VKEY_SUBTRACT,
       ui::DomCode::NUMPAD_SUBTRACT, ui::DomKey::FromCharacter('-')},
      {kVK_ANSI_KeypadEquals, '=', ui::VKEY_OEM_PLUS, ui::DomCode::NUMPAD_EQUAL,
       ui::DomKey::FromCharacter('=')},
      {kVK_ANSI_Keypad0, '0', ui::VKEY_NUMPAD0, ui::DomCode::NUMPAD0,
       ui::DomKey::FromCharacter('0')},
      {kVK_ANSI_Keypad1, '1', ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1,
       ui::DomKey::FromCharacter('1')},
      {kVK_ANSI_Keypad2, '2', ui::VKEY_NUMPAD2, ui::DomCode::NUMPAD2,
       ui::DomKey::FromCharacter('2')},
      {kVK_ANSI_Keypad3, '3', ui::VKEY_NUMPAD3, ui::DomCode::NUMPAD3,
       ui::DomKey::FromCharacter('3')},
      {kVK_ANSI_Keypad4, '4', ui::VKEY_NUMPAD4, ui::DomCode::NUMPAD4,
       ui::DomKey::FromCharacter('4')},
      {kVK_ANSI_Keypad5, '5', ui::VKEY_NUMPAD5, ui::DomCode::NUMPAD5,
       ui::DomKey::FromCharacter('5')},
      {kVK_ANSI_Keypad6, '6', ui::VKEY_NUMPAD6, ui::DomCode::NUMPAD6,
       ui::DomKey::FromCharacter('6')},
      {kVK_ANSI_Keypad7, '7', ui::VKEY_NUMPAD7, ui::DomCode::NUMPAD7,
       ui::DomKey::FromCharacter('7')},
      {kVK_ANSI_Keypad8, '8', ui::VKEY_NUMPAD8, ui::DomCode::NUMPAD8,
       ui::DomKey::FromCharacter('8')},
      {kVK_ANSI_Keypad9, '9', ui::VKEY_NUMPAD9, ui::DomCode::NUMPAD9,
       ui::DomKey::FromCharacter('9')},
  };

  for (const auto& mapping_entry : table) {
    NSEvent* mac_event =
        BuildFakeKeyEvent(mapping_entry.mac_key_code, mapping_entry.character,
                          0, NSEventTypeKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(mapping_entry.windows_key_code, web_event.windows_key_code);
    EXPECT_EQ(mapping_entry.dom_code,
              static_cast<ui::DomCode>(web_event.dom_code));
    EXPECT_EQ(mapping_entry.dom_key, web_event.dom_key);
  }
}

// Test that left- and right-hand modifier keys are interpreted correctly when
// pressed simultaneously.
TEST(WebInputEventFactoryTestMac, SimultaneousModifierKeys) {
  for (size_t i = 0; i < std::size(kModifierKeys) / 2; ++i) {
    const ModifierKey& left = kModifierKeys[2 * i];
    const ModifierKey& right = kModifierKeys[2 * i + 1];
    // Press the left key.
    NSEvent* mac_event =
        BuildFakeKeyEvent(left.mac_key_code, 0,
                          left.device_dependent_modifier_flag |
                              left.device_independent_modifier_flag,
                          NSEventTypeFlagsChanged);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::Type::kRawKeyDown, web_event.GetType());
    // Press the right key.
    mac_event = BuildFakeKeyEvent(right.mac_key_code, 0,
                                  left.device_dependent_modifier_flag |
                                      right.device_dependent_modifier_flag |
                                      left.device_independent_modifier_flag,
                                  NSEventTypeFlagsChanged);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::Type::kRawKeyDown, web_event.GetType());
    // Release the right key.
    mac_event = BuildFakeKeyEvent(right.mac_key_code, 0,
                                  left.device_dependent_modifier_flag |
                                      left.device_independent_modifier_flag,
                                  NSEventTypeFlagsChanged);
    // Release the left key.
    mac_event =
        BuildFakeKeyEvent(left.mac_key_code, 0, 0, NSEventTypeFlagsChanged);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::Type::kKeyUp, web_event.GetType());
  }
}

// Test that individual modifier keys are still reported correctly, even if the
// device dependent modifier flags are not set.
TEST(WebInputEventBuilderMacTest, MissingUndocumentedModifierFlags) {
  for (const auto& key : kModifierKeys) {
    NSEvent* mac_event = BuildFakeKeyEvent(key.mac_key_code, 0,
                                           key.device_independent_modifier_flag,
                                           NSEventTypeFlagsChanged);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::Type::kRawKeyDown, web_event.GetType());
    mac_event =
        BuildFakeKeyEvent(key.mac_key_code, 0, 0, NSEventTypeFlagsChanged);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::Type::kKeyUp, web_event.GetType());
  }
}

// Test system key events recognition.
TEST(WebInputEventBuilderMacTest, SystemKeyEvents) {
  // Cmd + B should not be treated as system event.
  NSEvent* macEvent = BuildFakeKeyEvent(
      kVK_ANSI_B, 'B', NSEventModifierFlagCommand, NSEventTypeKeyDown);
  WebKeyboardEvent webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_FALSE(webEvent.is_system_key);

  // Cmd + I should not be treated as system event.
  macEvent = BuildFakeKeyEvent(kVK_ANSI_I, 'I', NSEventModifierFlagCommand,
                               NSEventTypeKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_FALSE(webEvent.is_system_key);

  // Cmd + <some other modifier> + <B|I> should be treated as system event.
  macEvent = BuildFakeKeyEvent(
      kVK_ANSI_B, 'B', NSEventModifierFlagCommand | NSEventModifierFlagShift,
      NSEventTypeKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_TRUE(webEvent.is_system_key);
  macEvent = BuildFakeKeyEvent(
      kVK_ANSI_I, 'I', NSEventModifierFlagCommand | NSEventModifierFlagControl,
      NSEventTypeKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_TRUE(webEvent.is_system_key);

  // Cmd + <any letter other then B and I> should be treated as system event.
  macEvent = BuildFakeKeyEvent(kVK_ANSI_S, 'S', NSEventModifierFlagCommand,
                               NSEventTypeKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_TRUE(webEvent.is_system_key);

  // Cmd + <some other modifier> + <any letter other then B and I> should be
  // treated as system event.
  macEvent = BuildFakeKeyEvent(
      kVK_ANSI_S, 'S', NSEventModifierFlagCommand | NSEventModifierFlagShift,
      NSEventTypeKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_TRUE(webEvent.is_system_key);
}

// Test generating |windowsKeyCode| from |NSEvent| 'keydown'/'keyup', US
// keyboard and InputSource.
TEST(WebInputEventBuilderMacTest, USAlnumNSEventToKeyCode) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar character;
    unichar shift_character;
    ui::KeyboardCode key_code;
  } table[] = {
      {kVK_ANSI_0, '0', ')', ui::VKEY_0}, {kVK_ANSI_1, '1', '!', ui::VKEY_1},
      {kVK_ANSI_2, '2', '@', ui::VKEY_2}, {kVK_ANSI_3, '3', '#', ui::VKEY_3},
      {kVK_ANSI_4, '4', '$', ui::VKEY_4}, {kVK_ANSI_5, '5', '%', ui::VKEY_5},
      {kVK_ANSI_6, '6', '^', ui::VKEY_6}, {kVK_ANSI_7, '7', '&', ui::VKEY_7},
      {kVK_ANSI_8, '8', '*', ui::VKEY_8}, {kVK_ANSI_9, '9', '(', ui::VKEY_9},
      {kVK_ANSI_A, 'a', 'A', ui::VKEY_A}, {kVK_ANSI_B, 'b', 'B', ui::VKEY_B},
      {kVK_ANSI_C, 'c', 'C', ui::VKEY_C}, {kVK_ANSI_D, 'd', 'D', ui::VKEY_D},
      {kVK_ANSI_E, 'e', 'E', ui::VKEY_E}, {kVK_ANSI_F, 'f', 'F', ui::VKEY_F},
      {kVK_ANSI_G, 'g', 'G', ui::VKEY_G}, {kVK_ANSI_H, 'h', 'H', ui::VKEY_H},
      {kVK_ANSI_I, 'i', 'I', ui::VKEY_I}, {kVK_ANSI_J, 'j', 'J', ui::VKEY_J},
      {kVK_ANSI_K, 'k', 'K', ui::VKEY_K}, {kVK_ANSI_L, 'l', 'L', ui::VKEY_L},
      {kVK_ANSI_M, 'm', 'M', ui::VKEY_M}, {kVK_ANSI_N, 'n', 'N', ui::VKEY_N},
      {kVK_ANSI_O, 'o', 'O', ui::VKEY_O}, {kVK_ANSI_P, 'p', 'P', ui::VKEY_P},
      {kVK_ANSI_Q, 'q', 'Q', ui::VKEY_Q}, {kVK_ANSI_R, 'r', 'R', ui::VKEY_R},
      {kVK_ANSI_S, 's', 'S', ui::VKEY_S}, {kVK_ANSI_T, 't', 'T', ui::VKEY_T},
      {kVK_ANSI_U, 'u', 'U', ui::VKEY_U}, {kVK_ANSI_V, 'v', 'V', ui::VKEY_V},
      {kVK_ANSI_W, 'w', 'W', ui::VKEY_W}, {kVK_ANSI_X, 'x', 'X', ui::VKEY_X},
      {kVK_ANSI_Y, 'y', 'Y', ui::VKEY_Y}, {kVK_ANSI_Z, 'z', 'Z', ui::VKEY_Z}};

  for (const DomKeyTestCase& entry : table) {
    // Test without modifiers.
    NSEvent* mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.character,
                                           0, NSEventTypeKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windows_key_code);
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.character, 0,
                                  NSEventTypeKeyUp);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windows_key_code);
    // Test with Shift.
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.shift_character,
                                  NSEventModifierFlagShift, NSEventTypeKeyDown);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windows_key_code);
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.shift_character,
                                  NSEventModifierFlagShift, NSEventTypeKeyUp);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windows_key_code);
  }
}

// Test generating |windowsKeyCode| from |NSEvent| 'keydown'/'keyup', JIS
// keyboard and InputSource.
TEST(WebInputEventBuilderMacTest, JISNumNSEventToKeyCode) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar character;
    unichar shift_character;
    ui::KeyboardCode key_code;
  } table[] = {
      {kVK_ANSI_0, '0', '0', ui::VKEY_0},  {kVK_ANSI_1, '1', '!', ui::VKEY_1},
      {kVK_ANSI_2, '2', '\"', ui::VKEY_2}, {kVK_ANSI_3, '3', '#', ui::VKEY_3},
      {kVK_ANSI_4, '4', '$', ui::VKEY_4},  {kVK_ANSI_5, '5', '%', ui::VKEY_5},
      {kVK_ANSI_6, '6', '&', ui::VKEY_6},  {kVK_ANSI_7, '7', '\'', ui::VKEY_7},
      {kVK_ANSI_8, '8', '(', ui::VKEY_8},  {kVK_ANSI_9, '9', ')', ui::VKEY_9}};

  for (const DomKeyTestCase& entry : table) {
    // Test without modifiers.
    NSEvent* mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.character,
                                           0, NSEventTypeKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windows_key_code);
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.character, 0,
                                  NSEventTypeKeyUp);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windows_key_code);
    // Test with Shift.
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.shift_character,
                                  NSEventModifierFlagShift, NSEventTypeKeyDown);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windows_key_code);
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.shift_character,
                                  NSEventModifierFlagShift, NSEventTypeKeyUp);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windows_key_code);
  }
}

// Test generating |windowsKeyCode| from |NSEvent| 'keydown'/'keyup',
// US keyboard and Dvorak InputSource.
TEST(WebInputEventBuilderMacTest, USDvorakAlnumNSEventToKeyCode) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar character;
    ui::KeyboardCode key_code;
  } table[] = {
      {kVK_ANSI_0, '0', ui::VKEY_0}, {kVK_ANSI_1, '1', ui::VKEY_1},
      {kVK_ANSI_2, '2', ui::VKEY_2}, {kVK_ANSI_3, '3', ui::VKEY_3},
      {kVK_ANSI_4, '4', ui::VKEY_4}, {kVK_ANSI_5, '5', ui::VKEY_5},
      {kVK_ANSI_6, '6', ui::VKEY_6}, {kVK_ANSI_7, '7', ui::VKEY_7},
      {kVK_ANSI_8, '8', ui::VKEY_8}, {kVK_ANSI_9, '9', ui::VKEY_9},
      {kVK_ANSI_A, 'a', ui::VKEY_A}, {kVK_ANSI_B, 'x', ui::VKEY_X},
      {kVK_ANSI_C, 'j', ui::VKEY_J}, {kVK_ANSI_D, 'e', ui::VKEY_E},
      {kVK_ANSI_E, '.', ui::VKEY_OEM_PERIOD}, {kVK_ANSI_F, 'u', ui::VKEY_U},
      {kVK_ANSI_G, 'i', ui::VKEY_I}, {kVK_ANSI_H, 'd', ui::VKEY_D},
      {kVK_ANSI_I, 'c', ui::VKEY_C}, {kVK_ANSI_J, 'h', ui::VKEY_H},
      {kVK_ANSI_K, 't', ui::VKEY_T}, {kVK_ANSI_L, 'n', ui::VKEY_N},
      {kVK_ANSI_M, 'm', ui::VKEY_M}, {kVK_ANSI_N, 'b', ui::VKEY_B},
      {kVK_ANSI_O, 'r', ui::VKEY_R}, {kVK_ANSI_P, 'l', ui::VKEY_L},
      {kVK_ANSI_Q, '\'', ui::VKEY_OEM_7}, {kVK_ANSI_R, 'p', ui::VKEY_P},
      {kVK_ANSI_S, 'o', ui::VKEY_O}, {kVK_ANSI_T, 'y', ui::VKEY_Y},
      {kVK_ANSI_U, 'g', ui::VKEY_G}, {kVK_ANSI_V, 'k', ui::VKEY_K},
      {kVK_ANSI_W, ',', ui::VKEY_OEM_COMMA}, {kVK_ANSI_X, 'q', ui::VKEY_Q},
      {kVK_ANSI_Y, 'f', ui::VKEY_F}, {kVK_ANSI_Z, ';', ui::VKEY_OEM_1}};

  for (const DomKeyTestCase& entry : table) {
    // Test without modifiers.
    NSEvent* mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.character,
                                           0, NSEventTypeKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windows_key_code);
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.character, 0,
                                  NSEventTypeKeyUp);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windows_key_code);
  }
}

// 'Dvorak - QWERTY Command' layout will map the key back to QWERTY when Command
// is pressed.
// e.g. Key 'b' maps to 'x' but 'Command-b' remains 'Command-b'.
TEST(WebInputEventBuilderMacTest, USDvorakQWERTYCommand) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar cmd_character;
  } table[] = {{kVK_ANSI_0, '0'}, {kVK_ANSI_1, '1'}, {kVK_ANSI_2, '2'},
               {kVK_ANSI_3, '3'}, {kVK_ANSI_4, '4'}, {kVK_ANSI_5, '5'},
               {kVK_ANSI_6, '6'}, {kVK_ANSI_7, '7'}, {kVK_ANSI_8, '8'},
               {kVK_ANSI_9, '9'}, {kVK_ANSI_A, 'a'}, {kVK_ANSI_B, 'b'},
               {kVK_ANSI_C, 'c'}, {kVK_ANSI_D, 'd'}, {kVK_ANSI_E, 'e'},
               {kVK_ANSI_F, 'f'}, {kVK_ANSI_G, 'g'}, {kVK_ANSI_H, 'h'},
               {kVK_ANSI_I, 'i'}, {kVK_ANSI_J, 'j'}, {kVK_ANSI_K, 'k'},
               {kVK_ANSI_L, 'l'}, {kVK_ANSI_M, 'm'}, {kVK_ANSI_N, 'n'},
               {kVK_ANSI_O, 'o'}, {kVK_ANSI_P, 'p'}, {kVK_ANSI_Q, 'q'},
               {kVK_ANSI_R, 'r'}, {kVK_ANSI_S, 's'}, {kVK_ANSI_T, 't'},
               {kVK_ANSI_U, 'u'}, {kVK_ANSI_V, 'v'}, {kVK_ANSI_W, 'w'},
               {kVK_ANSI_X, 'x'}, {kVK_ANSI_Y, 'y'}, {kVK_ANSI_Z, 'z'}};

  for (const DomKeyTestCase& entry : table) {
    NSEvent* mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, entry.cmd_character,
                          NSEventModifierFlagCommand, NSEventTypeKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.cmd_character),
              web_event.dom_key);
  }
}

// Test conversion from key combination with Control to DomKey.
// TODO(input-dev): Move DomKey tests for all platforms into one place.
// http://crbug.com/587589
// This test case only works for U.S. layout.
TEST(WebInputEventBuilderMacTest, DomKeyCtrlShift) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar character;
    unichar shift_character;
  } table[] = {
      {kVK_ANSI_0, '0', ')'}, {kVK_ANSI_1, '1', '!'}, {kVK_ANSI_2, '2', '@'},
      {kVK_ANSI_3, '3', '#'}, {kVK_ANSI_4, '4', '$'}, {kVK_ANSI_5, '5', '%'},
      {kVK_ANSI_6, '6', '^'}, {kVK_ANSI_7, '7', '&'}, {kVK_ANSI_8, '8', '*'},
      {kVK_ANSI_9, '9', '('}, {kVK_ANSI_A, 'a', 'A'}, {kVK_ANSI_B, 'b', 'B'},
      {kVK_ANSI_C, 'c', 'C'}, {kVK_ANSI_D, 'd', 'D'}, {kVK_ANSI_E, 'e', 'E'},
      {kVK_ANSI_F, 'f', 'F'}, {kVK_ANSI_G, 'g', 'G'}, {kVK_ANSI_H, 'h', 'H'},
      {kVK_ANSI_I, 'i', 'I'}, {kVK_ANSI_J, 'j', 'J'}, {kVK_ANSI_K, 'k', 'K'},
      {kVK_ANSI_L, 'l', 'L'}, {kVK_ANSI_M, 'm', 'M'}, {kVK_ANSI_N, 'n', 'N'},
      {kVK_ANSI_O, 'o', 'O'}, {kVK_ANSI_P, 'p', 'P'}, {kVK_ANSI_Q, 'q', 'Q'},
      {kVK_ANSI_R, 'r', 'R'}, {kVK_ANSI_S, 's', 'S'}, {kVK_ANSI_T, 't', 'T'},
      {kVK_ANSI_U, 'u', 'U'}, {kVK_ANSI_V, 'v', 'V'}, {kVK_ANSI_W, 'w', 'W'},
      {kVK_ANSI_X, 'x', 'X'}, {kVK_ANSI_Y, 'y', 'Y'}, {kVK_ANSI_Z, 'z', 'Z'}};

  for (const DomKeyTestCase& entry : table) {
    // Tests ctrl_dom_key.
    NSEvent* mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, entry.character,
                          NSEventModifierFlagControl, NSEventTypeKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.character), web_event.dom_key);
    // Tests ctrl_shift_dom_key.
    mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, entry.shift_character,
                          NSEventModifierFlagControl | NSEventModifierFlagShift,
                          NSEventTypeKeyDown);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.shift_character),
              web_event.dom_key);
  }
}

// This test case only works for U.S. layout.
TEST(WebInputEventBuilderMacTest, DomKeyCtrlAlt) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar alt_character;
    unichar ctrl_alt_character;
  } table[] = {{kVK_ANSI_0, u"º"[0], u"0"[0]}, {kVK_ANSI_1, u"¡"[0], u"1"[0]},
               {kVK_ANSI_2, u"™"[0], u"2"[0]}, {kVK_ANSI_3, u"£"[0], u"3"[0]},
               {kVK_ANSI_4, u"¢"[0], u"4"[0]}, {kVK_ANSI_5, u"∞"[0], u"5"[0]},
               {kVK_ANSI_6, u"§"[0], u"6"[0]}, {kVK_ANSI_7, u"¶"[0], u"7"[0]},
               {kVK_ANSI_8, u"•"[0], u"8"[0]}, {kVK_ANSI_9, u"ª"[0], u"9"[0]},
               {kVK_ANSI_A, u"å"[0], u"å"[0]}, {kVK_ANSI_B, u"∫"[0], u"∫"[0]},
               {kVK_ANSI_C, u"ç"[0], u"ç"[0]}, {kVK_ANSI_D, u"∂"[0], u"∂"[0]},
               {kVK_ANSI_F, u"ƒ"[0], u"ƒ"[0]}, {kVK_ANSI_G, u"©"[0], u"©"[0]},
               {kVK_ANSI_H, u"˙"[0], u"˙"[0]}, {kVK_ANSI_J, u"∆"[0], u"∆"[0]},
               {kVK_ANSI_K, u"˚"[0], u"˚"[0]}, {kVK_ANSI_L, u"¬"[0], u"¬"[0]},
               {kVK_ANSI_M, u"µ"[0], u"µ"[0]}, {kVK_ANSI_O, u"ø"[0], u"ø"[0]},
               {kVK_ANSI_P, u"π"[0], u"π"[0]}, {kVK_ANSI_Q, u"œ"[0], u"œ"[0]},
               {kVK_ANSI_R, u"®"[0], u"®"[0]}, {kVK_ANSI_S, u"ß"[0], u"ß"[0]},
               {kVK_ANSI_T, u"†"[0], u"†"[0]}, {kVK_ANSI_V, u"√"[0], u"√"[0]},
               {kVK_ANSI_W, u"∑"[0], u"∑"[0]}, {kVK_ANSI_X, u"≈"[0], u"≈"[0]},
               {kVK_ANSI_Y, u"¥"[0], u"¥"[0]}, {kVK_ANSI_Z, u"Ω"[0], u"Ω"[0]}};

  for (const DomKeyTestCase& entry : table) {
    // Tests alt_dom_key.
    NSEvent* mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, entry.alt_character,
                          NSEventModifierFlagOption, NSEventTypeKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.alt_character), web_event.dom_key)
        << "a " << entry.alt_character;
    // Tests ctrl_alt_dom_key.
    mac_event = BuildFakeKeyEvent(
        entry.mac_key_code, entry.ctrl_alt_character,
        NSEventModifierFlagControl | NSEventModifierFlagOption,
        NSEventTypeKeyDown);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.ctrl_alt_character),
              web_event.dom_key)
        << "a_c " << entry.ctrl_alt_character;
  }

  struct DeadDomKeyTestCase {
    int mac_key_code;
    unichar alt_accent_character;
  } dead_key_table[] = {{kVK_ANSI_E, u"´"[0]},
                        {kVK_ANSI_I, u"ˆ"[0]},
                        {kVK_ANSI_N, u"˜"[0]},
                        {kVK_ANSI_U, u"¨"[0]}};

  for (const DeadDomKeyTestCase& entry : dead_key_table) {
    // Tests alt_accent_character.
    NSEvent* mac_event = BuildFakeKeyEvent(
        entry.mac_key_code, 0, NSEventModifierFlagOption, NSEventTypeKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(
        ui::DomKey::DeadKeyFromCombiningCharacter(entry.alt_accent_character),
        web_event.dom_key)
        << "a " << entry.alt_accent_character;

    // Tests alt_accent_character with ctrl.
    mac_event = BuildFakeKeyEvent(
        entry.mac_key_code, 0,
        NSEventModifierFlagControl | NSEventModifierFlagOption,
        NSEventTypeKeyDown);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(
        ui::DomKey::DeadKeyFromCombiningCharacter(entry.alt_accent_character),
        web_event.dom_key)
        << "a_c " << entry.alt_accent_character;
  }
}

TEST(WebInputEventBuilderMacTest, DomKeyNonPrintable) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar character;
    ui::DomKey dom_key;
  } table[] = {
      {kVK_Return, kReturnCharCode, ui::DomKey::ENTER},
      {kVK_Tab, kTabCharCode, ui::DomKey::TAB},
      {kVK_Delete, kBackspaceCharCode, ui::DomKey::BACKSPACE},
      {kVK_Escape, kEscapeCharCode, ui::DomKey::ESCAPE},
      {kVK_F1, NSF1FunctionKey, ui::DomKey::F1},
      {kVK_F2, NSF2FunctionKey, ui::DomKey::F2},
      {kVK_F3, NSF3FunctionKey, ui::DomKey::F3},
      {kVK_F4, NSF4FunctionKey, ui::DomKey::F4},
      {kVK_F5, NSF5FunctionKey, ui::DomKey::F5},
      {kVK_F6, NSF6FunctionKey, ui::DomKey::F6},
      {kVK_F7, NSF7FunctionKey, ui::DomKey::F7},
      {kVK_F8, NSF8FunctionKey, ui::DomKey::F8},
      {kVK_F9, NSF9FunctionKey, ui::DomKey::F9},
      {kVK_F10, NSF10FunctionKey, ui::DomKey::F10},
      {kVK_F11, NSF11FunctionKey, ui::DomKey::F11},
      {kVK_F12, NSF12FunctionKey, ui::DomKey::F12},
      {kVK_F13, NSF13FunctionKey, ui::DomKey::F13},
      {kVK_F14, NSF14FunctionKey, ui::DomKey::F14},
      {kVK_F15, NSF15FunctionKey, ui::DomKey::F15},
      {kVK_F16, NSF16FunctionKey, ui::DomKey::F16},
      {kVK_F17, NSF17FunctionKey, ui::DomKey::F17},
      {kVK_F18, NSF18FunctionKey, ui::DomKey::F18},
      {kVK_F19, NSF19FunctionKey, ui::DomKey::F19},
      {kVK_F20, NSF20FunctionKey, ui::DomKey::F20},
      {kVK_Help, kHelpCharCode, ui::DomKey::HELP},
      {kVK_Home, NSHomeFunctionKey, ui::DomKey::HOME},
      {kVK_PageUp, NSPageUpFunctionKey, ui::DomKey::PAGE_UP},
      {kVK_ForwardDelete, NSDeleteFunctionKey, ui::DomKey::DEL},
      {kVK_End, NSEndFunctionKey, ui::DomKey::END},
      {kVK_PageDown, NSPageDownFunctionKey, ui::DomKey::PAGE_DOWN},
      {kVK_LeftArrow, NSLeftArrowFunctionKey, ui::DomKey::ARROW_LEFT},
      {kVK_RightArrow, NSRightArrowFunctionKey, ui::DomKey::ARROW_RIGHT},
      {kVK_DownArrow, NSDownArrowFunctionKey, ui::DomKey::ARROW_DOWN},
      {kVK_UpArrow, NSUpArrowFunctionKey, ui::DomKey::ARROW_UP}};

  for (const DomKeyTestCase& entry : table) {
    // Tests non-printable key.
    NSEvent* mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.character,
                                           0, NSEventTypeKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.dom_key, web_event.dom_key) << entry.mac_key_code;
    // Tests non-printable key with Shift.
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.character,
                                  NSEventModifierFlagShift, NSEventTypeKeyDown);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.dom_key, web_event.dom_key) << "s " << entry.mac_key_code;
  }
}

TEST(WebInputEventBuilderMacTest, DomKeyFlagsChanged) {
  struct DomKeyTestCase {
    int mac_key_code;
    ui::DomKey dom_key;
  } table[] = {{kVK_Command, ui::DomKey::META},
               {kVK_Shift, ui::DomKey::SHIFT},
               {kVK_RightShift, ui::DomKey::SHIFT},
               {kVK_CapsLock, ui::DomKey::CAPS_LOCK},
               {kVK_Option, ui::DomKey::ALT},
               {kVK_RightOption, ui::DomKey::ALT},
               {kVK_Control, ui::DomKey::CONTROL},
               {kVK_RightControl, ui::DomKey::CONTROL},
               {kVK_Function, ui::DomKey::FN}};

  for (const DomKeyTestCase& entry : table) {
    NSEvent* mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, 0, 0, NSEventTypeFlagsChanged);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.dom_key, web_event.dom_key) << entry.mac_key_code;
  }
}

TEST(WebInputEventBuilderMacTest, ContextMenuKey) {
  const NSEventType kEventTypeToTest[] = {NSEventTypeKeyDown, NSEventTypeKeyUp};
  for (auto type : kEventTypeToTest) {
    NSEvent* mac_event = BuildFakeKeyEvent(kVK_ContextualMenu, 0, 0, type);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(ui::DomKey::CONTEXT_MENU, web_event.dom_key);
    EXPECT_EQ(ui::VKEY_APPS, web_event.windows_key_code);
  }
}

TEST(WebInputEventBuilderMacTest, EmojiKey) {
  const NSEventType kEventTypeToTest[] = {NSEventTypeKeyDown, NSEventTypeKeyUp};
  for (auto type : kEventTypeToTest) {
    // The 💩 emoji bound to F1.
    NSEvent* mac_event = BuildFakeKeyEvent(kVK_F1, u"\U0001F4A9", 0, type);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(ui::DomKey::FromCharacter(U'\U0001F4A9'), web_event.dom_key);
    EXPECT_EQ(ui::VKEY_F1, web_event.windows_key_code);
  }
}

TEST(WebInputEventBuilderMacTest, InvalidSurrogateKey) {
  const NSEventType kEventTypeToTest[] = {NSEventTypeKeyDown, NSEventTypeKeyUp};
  for (auto type : kEventTypeToTest) {
    for (auto code_point : {char16_t(0xD800), char16_t(0xDFFF)}) {
      // A surrogate bound to F1.
      NSEvent* mac_event = BuildFakeKeyEvent(kVK_F1, code_point, 0, type);
      WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
      EXPECT_EQ(ui::DomKey::F1, web_event.dom_key);
      EXPECT_EQ(ui::VKEY_F1, web_event.windows_key_code);
    }
  }
}

// Test that a ui::Event and blink::WebInputEvent made from the same NSEvent
// have the same values for comparable fields.
TEST(WebInputEventBuilderMacTest, ScrollWheelMatchesUIEvent) {
  bool precise = false;
  CGFloat delta_x = 123;
  CGFloat delta_y = 321;
  NSPoint location = NSMakePoint(11, 22);

  // WebMouseWheelEventBuilder requires a non-nil view to map coordinates. So
  // create a dummy window, but don't show it.
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.releasedWhenClosed = NO;

  NSEvent* mac_event = cocoa_test_event_utils::TestScrollEvent(
      location, window, delta_x, delta_y, precise, NSEventPhaseNone,
      NSEventPhaseNone);
  EXPECT_EQ(delta_x, mac_event.deltaX);
  EXPECT_EQ(delta_y, mac_event.deltaY);

  blink::WebMouseWheelEvent web_event =
      input::WebMouseWheelEventBuilder::Build(mac_event, window.contentView);
  ui::MouseWheelEvent ui_event((base::apple::OwnedNSEvent(mac_event)));

  EXPECT_EQ(delta_x * ui::kScrollbarPixelsPerCocoaTick, web_event.delta_x);
  EXPECT_EQ(web_event.delta_x, ui_event.x_offset());

  EXPECT_EQ(delta_y * ui::kScrollbarPixelsPerCocoaTick, web_event.delta_y);
  EXPECT_EQ(web_event.delta_y, ui_event.y_offset());

  EXPECT_EQ(11, web_event.PositionInWidget().x());
  EXPECT_EQ(web_event.PositionInWidget().x(), ui_event.x());

  // Both ui:: and blink:: events use an origin at the top-left.
  EXPECT_EQ(100 - 22, web_event.PositionInWidget().y());
  EXPECT_EQ(web_event.PositionInWidget().y(), ui_event.y());
}

// Test if the value of twist and rotation_angle are set correctly when the
// NSEvent's rotation is less than 90.
TEST(WebInputEventBuilderMacTest, TouchEventsWithPointerTypePenRotationLess90) {
  NSEvent* mac_event =
      BuildFakeMouseEvent(kCGEventLeftMouseDown, {6, 9}, kCGMouseButtonLeft,
                          kCGEventMouseSubtypeTabletPoint, 60.0);
  // Create a dummy window, but don't show it.
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.releasedWhenClosed = NO;
  blink::WebTouchEvent touch_event =
      input::WebTouchEventBuilder::Build(mac_event, window.contentView);
  EXPECT_EQ(60, touch_event.touches[0].twist);
  EXPECT_EQ(60, touch_event.touches[0].rotation_angle);
}

// Test if the value of twist and rotation_angle are set correctly when the
// NSEvent's rotation is between 90 and 180.
TEST(WebInputEventBuilderMacTest,
     TouchEventsWithPointerTypePenRotationLess180) {
  NSEvent* mac_event =
      BuildFakeMouseEvent(kCGEventLeftMouseDown, {6, 9}, kCGMouseButtonLeft,
                          kCGEventMouseSubtypeTabletPoint, 160.0);
  // Create a dummy window, but don't show it.
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.releasedWhenClosed = NO;
  blink::WebTouchEvent touch_event =
      input::WebTouchEventBuilder::Build(mac_event, [window contentView]);
  EXPECT_EQ(160, touch_event.touches[0].twist);
  EXPECT_EQ(20, touch_event.touches[0].rotation_angle);
}

// Test if the value of twist and rotation_angle are set correctly when the
// NSEvent's rotation is between 180 and 360.
TEST(WebInputEventBuilderMacTest,
     TouchEventsWithPointerTypePenRotationLess360) {
  NSEvent* mac_event =
      BuildFakeMouseEvent(kCGEventLeftMouseDown, {6, 9}, kCGMouseButtonLeft,
                          kCGEventMouseSubtypeTabletPoint, 260.0);
  // Create a dummy window, but don't show it.
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.releasedWhenClosed = NO;
  blink::WebTouchEvent touch_event =
      input::WebTouchEventBuilder::Build(mac_event, window.contentView);
  EXPECT_EQ(260, touch_event.touches[0].twist);
  EXPECT_EQ(80, touch_event.touches[0].rotation_angle);
}

// Test if the value of twist and rotation_angle are set correctly when the
// NSEvent's rotation is greater than 360.
TEST(WebInputEventBuilderMacTest,
     TouchEventsWithPointerTypePenRotationGreater360) {
  NSEvent* mac_event =
      BuildFakeMouseEvent(kCGEventLeftMouseDown, {6, 9}, kCGMouseButtonLeft,
                          kCGEventMouseSubtypeTabletPoint, 390.0);
  // Create a dummy window, but don't show it.
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.releasedWhenClosed = NO;
  blink::WebTouchEvent touch_event =
      input::WebTouchEventBuilder::Build(mac_event, window.contentView);
  EXPECT_EQ(30, touch_event.touches[0].twist);
  EXPECT_EQ(30, touch_event.touches[0].rotation_angle);
}

// Test if all the values of a WebTouchEvent are set correctly.
TEST(WebInputEventBuilderMacTest, BuildWebTouchEvents) {
  NSEvent* mac_event =
      BuildFakeMouseEvent(kCGEventLeftMouseDown, {6, 9}, kCGMouseButtonLeft,
                          kCGEventMouseSubtypeTabletPoint, /*rotation=*/60.0,
                          /*pressure=*/0.3, /*tilt_x=*/0.5, /*tilt_y=*/0.6,
                          /*tangential_pressure=*/0.7);
  // Create a dummy window, but don't show it.
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.releasedWhenClosed = NO;
  blink::WebTouchEvent touch_event =
      input::WebTouchEventBuilder::Build(mac_event, window.contentView);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchStart, touch_event.GetType());
  EXPECT_FALSE(touch_event.hovering);
  EXPECT_EQ(1U, touch_event.touches_length);
  EXPECT_EQ(gfx::PointF(6, 9), touch_event.touches[0].PositionInScreen());
  EXPECT_EQ(blink::WebTouchPoint::State::kStatePressed,
            touch_event.touches[0].state);
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kPen,
            touch_event.touches[0].pointer_type);
  EXPECT_EQ(0, touch_event.touches[0].id);
  EXPECT_FLOAT_EQ(0.3, std::round(touch_event.touches[0].force * 10) / 10);
  EXPECT_EQ(0.5 * 90, std::round(touch_event.touches[0].tilt_x));
  EXPECT_EQ(0.6 * 90, std::round(touch_event.touches[0].tilt_y));
  EXPECT_FLOAT_EQ(
      0.7, std::round(touch_event.touches[0].tangential_pressure * 10) / 10);
  EXPECT_EQ(60, touch_event.touches[0].twist);
  EXPECT_FLOAT_EQ(60.0, touch_event.touches[0].rotation_angle);
}

// Test if the mouse back button values of a WebMouseEvent are set correctly.
TEST(WebInputEventBuilderMacTest, BuildWebMouseEventsWithBackButton) {
  NSEvent* mac_event =
      BuildFakeMouseEvent(kCGEventOtherMouseDown, {6, 9}, kCGMouseButtonLeft,
                          kCGEventMouseSubtypeDefault, /*rotation=*/0.0,
                          /*pressure=*/0.0, /*tilt_x=*/0.0, /*tilt_y=*/0.0,
                          /*tangential_pressure=*/0.0, /*button_number=*/3);
  // Create a dummy window, but don't show it.
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.releasedWhenClosed = NO;
  blink::WebMouseEvent mouse_event =
      input::WebMouseEventBuilder::Build(mac_event, window.contentView);
  EXPECT_EQ(blink::WebInputEvent::Type::kMouseDown, mouse_event.GetType());
  EXPECT_EQ(gfx::PointF(6, 9), mouse_event.PositionInScreen());
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kMouse,
            mouse_event.pointer_type);
  EXPECT_EQ(blink::WebMouseEvent::Button::kBack, mouse_event.button);
}

// Test if the mouse forward button values of a WebMouseEvent are set correctly.
TEST(WebInputEventBuilderMacTest, BuildWebMouseEventsWithForwardButton) {
  NSEvent* mac_event =
      BuildFakeMouseEvent(kCGEventOtherMouseDown, {6, 9}, kCGMouseButtonLeft,
                          kCGEventMouseSubtypeDefault, /*rotation=*/0.0,
                          /*pressure=*/0.0, /*tilt_x=*/0.0, /*tilt_y=*/0.0,
                          /*tangential_pressure=*/0.0, /*button_number=*/4);
  // Create a dummy window, but don't show it.
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.releasedWhenClosed = NO;
  blink::WebMouseEvent mouse_event =
      input::WebMouseEventBuilder::Build(mac_event, window.contentView);
  EXPECT_EQ(blink::WebInputEvent::Type::kMouseDown, mouse_event.GetType());
  EXPECT_EQ(gfx::PointF(6, 9), mouse_event.PositionInScreen());
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kMouse,
            mouse_event.pointer_type);
  EXPECT_EQ(blink::WebMouseEvent::Button::kForward, mouse_event.button);
}
