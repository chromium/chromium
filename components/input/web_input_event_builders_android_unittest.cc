// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/web_input_event_builders_android.h"

#include <android/input.h>
#include <android/keycodes.h>

#include "base/android/jni_android.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/events/android/key_event_utils.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/scoped_event_test_tick_clock.h"
#include "ui/events/velocity_tracker/motion_event.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;

namespace {

const int kCombiningAccent = 0x80000000;
const int kCombiningAccentMask = 0x7fffffff;
const int kCompositionKeyCode = 229;

WebKeyboardEvent CreateFakeWebKeyboardEvent(JNIEnv* env,
                                            int key_code,
                                            int web_modifier,
                                            int unicode_character) {
  ScopedJavaLocalRef<jobject> keydown_event =
      ui::events::android::CreateKeyEvent(env, 0, key_code);

  WebKeyboardEvent web_event = input::WebKeyboardEventBuilder::Build(
      env, keydown_event, WebKeyboardEvent::Type::kKeyDown, web_modifier,
      blink::WebInputEvent::GetStaticTimeStampForTests(), key_code, 0,
      unicode_character, false);
  return web_event;
}

}  // anonymous namespace

class WebInputEventBuilderAndroidTest : public testing::Test {};

// This test case is based on VirtualKeyboard layout.
// https://github.com/android/platform_frameworks_base/blob/master/data/keyboards/Virtual.kcm
TEST(WebInputEventBuilderAndroidTest, DomKeyCtrlShift) {
  JNIEnv* env = AttachCurrentThread();

  struct DomKeyTestCase {
    int key_code;
    int character;
    int shift_character;
  } table[] = {
      {AKEYCODE_0, '0', ')'}, {AKEYCODE_1, '1', '!'}, {AKEYCODE_2, '2', '@'},
      {AKEYCODE_3, '3', '#'}, {AKEYCODE_4, '4', '$'}, {AKEYCODE_5, '5', '%'},
      {AKEYCODE_6, '6', '^'}, {AKEYCODE_7, '7', '&'}, {AKEYCODE_8, '8', '*'},
      {AKEYCODE_9, '9', '('}, {AKEYCODE_A, 'a', 'A'}, {AKEYCODE_B, 'b', 'B'},
      {AKEYCODE_C, 'c', 'C'}, {AKEYCODE_D, 'd', 'D'}, {AKEYCODE_E, 'e', 'E'},
      {AKEYCODE_F, 'f', 'F'}, {AKEYCODE_G, 'g', 'G'}, {AKEYCODE_H, 'h', 'H'},
      {AKEYCODE_I, 'i', 'I'}, {AKEYCODE_J, 'j', 'J'}, {AKEYCODE_K, 'k', 'K'},
      {AKEYCODE_L, 'l', 'L'}, {AKEYCODE_M, 'm', 'M'}, {AKEYCODE_N, 'n', 'N'},
      {AKEYCODE_O, 'o', 'O'}, {AKEYCODE_P, 'p', 'P'}, {AKEYCODE_Q, 'q', 'Q'},
      {AKEYCODE_R, 'r', 'R'}, {AKEYCODE_S, 's', 'S'}, {AKEYCODE_T, 't', 'T'},
      {AKEYCODE_U, 'u', 'U'}, {AKEYCODE_V, 'v', 'V'}, {AKEYCODE_W, 'w', 'W'},
      {AKEYCODE_X, 'x', 'X'}, {AKEYCODE_Y, 'y', 'Y'}, {AKEYCODE_Z, 'z', 'Z'}};

  for (const DomKeyTestCase& entry : table) {
    // Tests DomKey without modifier.
    WebKeyboardEvent web_event =
        CreateFakeWebKeyboardEvent(env, entry.key_code, 0, entry.character);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.character), web_event.dom_key)
        << ui::KeycodeConverter::DomKeyToKeyString(web_event.dom_key);

    // Tests DomKey with Ctrl.
    web_event = CreateFakeWebKeyboardEvent(env, entry.key_code,
                                           WebKeyboardEvent::kControlKey, 0);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.character), web_event.dom_key)
        << ui::KeycodeConverter::DomKeyToKeyString(web_event.dom_key);

    // Tests DomKey with Ctrl and Shift.
    web_event = CreateFakeWebKeyboardEvent(
        env, entry.key_code,
        WebKeyboardEvent::kControlKey | WebKeyboardEvent::kShiftKey, 0);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.shift_character),
              web_event.dom_key)
        << ui::KeycodeConverter::DomKeyToKeyString(web_event.dom_key);
  }
}

// This test case is based on VirtualKeyboard layout.
// https://github.com/android/platform_frameworks_base/blob/master/data/keyboards/Virtual.kcm
TEST(WebInputEventBuilderAndroidTest, DomKeyCtrlAlt) {
  JNIEnv* env = AttachCurrentThread();

  struct DomKeyTestCase {
    int key_code;
    int character;
    int alt_character;
  } table[] = {{AKEYCODE_0, '0', 0},         {AKEYCODE_1, '1', 0},
               {AKEYCODE_2, '2', 0},         {AKEYCODE_3, '3', 0},
               {AKEYCODE_4, '4', 0},         {AKEYCODE_5, '5', 0},
               {AKEYCODE_6, '6', 0},         {AKEYCODE_7, '7', 0},
               {AKEYCODE_8, '8', 0},         {AKEYCODE_9, '9', 0},
               {AKEYCODE_A, 'a', 0},         {AKEYCODE_B, 'b', 0},
               {AKEYCODE_C, 'c', u'\u00e7'}, {AKEYCODE_D, 'd', 0},
               {AKEYCODE_E, 'e', u'\u0301'}, {AKEYCODE_F, 'f', 0},
               {AKEYCODE_G, 'g', 0},         {AKEYCODE_H, 'h', 0},
               {AKEYCODE_I, 'i', u'\u0302'}, {AKEYCODE_J, 'j', 0},
               {AKEYCODE_K, 'k', 0},         {AKEYCODE_L, 'l', 0},
               {AKEYCODE_M, 'm', 0},         {AKEYCODE_N, 'n', u'\u0303'},
               {AKEYCODE_O, 'o', 0},         {AKEYCODE_P, 'p', 0},
               {AKEYCODE_Q, 'q', 0},         {AKEYCODE_R, 'r', 0},
               {AKEYCODE_S, 's', u'\u00df'}, {AKEYCODE_T, 't', 0},
               {AKEYCODE_U, 'u', u'\u0308'}, {AKEYCODE_V, 'v', 0},
               {AKEYCODE_W, 'w', 0},         {AKEYCODE_X, 'x', 0},
               {AKEYCODE_Y, 'y', 0},         {AKEYCODE_Z, 'z', 0}};

  for (const DomKeyTestCase& entry : table) {
    // Tests DomKey with Alt.
    WebKeyboardEvent web_event = CreateFakeWebKeyboardEvent(
        env, entry.key_code, WebKeyboardEvent::kAltKey, entry.alt_character);
    ui::DomKey expected_alt_dom_key;
    if (entry.alt_character == 0) {
      expected_alt_dom_key = ui::DomKey::FromCharacter(entry.character);
    } else if (entry.alt_character & kCombiningAccent) {
      expected_alt_dom_key = ui::DomKey::DeadKeyFromCombiningCharacter(
          entry.alt_character & kCombiningAccentMask);
    } else {
      expected_alt_dom_key = ui::DomKey::FromCharacter(entry.alt_character);
    }
    EXPECT_EQ(expected_alt_dom_key, web_event.dom_key)
        << ui::KeycodeConverter::DomKeyToKeyString(web_event.dom_key);

    // Tests DomKey with Ctrl and Alt.
    web_event = CreateFakeWebKeyboardEvent(
        env, entry.key_code,
        WebKeyboardEvent::kControlKey | WebKeyboardEvent::kAltKey, 0);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.character), web_event.dom_key)
        << ui::KeycodeConverter::DomKeyToKeyString(web_event.dom_key);
  }
}

// Testing AKEYCODE_LAST_CHANNEL because it's overlapping with
// COMPOSITION_KEY_CODE (both 229).
TEST(WebInputEventBuilderAndroidTest, LastChannelKey) {
  JNIEnv* env = AttachCurrentThread();

  // AKEYCODE_LAST_CHANNEL (229) is not defined in minimum NDK.
  WebKeyboardEvent web_event = CreateFakeWebKeyboardEvent(env, 229, 0, 0);
  EXPECT_EQ(229, web_event.native_key_code);
  EXPECT_EQ(ui::KeyboardCode::VKEY_UNKNOWN, web_event.windows_key_code);
  EXPECT_EQ(static_cast<int>(ui::DomCode::NONE), web_event.dom_code);
  EXPECT_EQ(ui::DomKey::MEDIA_LAST, web_event.dom_key);
}

// Synthetic key event should produce DomKey::UNIDENTIFIED.
TEST(WebInputEventBuilderAndroidTest, DomKeySyntheticEvent) {
  WebKeyboardEvent web_event = input::WebKeyboardEventBuilder::Build(
      nullptr, nullptr, WebKeyboardEvent::Type::kKeyDown, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests(), kCompositionKeyCode,
      0, 0, false);
  EXPECT_EQ(kCompositionKeyCode, web_event.native_key_code);
  EXPECT_EQ(ui::KeyboardCode::VKEY_UNKNOWN, web_event.windows_key_code);
  EXPECT_EQ(static_cast<int>(ui::DomCode::NONE), web_event.dom_code);
  EXPECT_EQ(ui::DomKey::UNIDENTIFIED, web_event.dom_key);
}

// Testing new Android keycode introduced in API 24.
TEST(WebInputEventBuilderAndroidTest, CutCopyPasteKey) {
  JNIEnv* env = AttachCurrentThread();

  struct DomKeyTestCase {
    int key_code;
    ui::DomKey key;
  } test_cases[] = {
      {AKEYCODE_CUT, ui::DomKey::CUT},
      {AKEYCODE_COPY, ui::DomKey::COPY},
      {AKEYCODE_PASTE, ui::DomKey::PASTE},
  };

  for (const auto& entry : test_cases) {
    WebKeyboardEvent web_event =
        CreateFakeWebKeyboardEvent(env, entry.key_code, 0, 0);
    EXPECT_EQ(entry.key, web_event.dom_key);
  }
}

TEST(WebInputEventBuilderAndroidTest, WebMouseEventCoordinates) {
  constexpr int kEventTimeNs = 5'000'000;
  const base::TimeTicks event_time =
      base::TimeTicks() + base::Nanoseconds(kEventTimeNs);

  ui::test::ScopedEventTestTickClock clock;
  clock.SetNowTicks(event_time);

  ui::MotionEventAndroid::Pointer p0(1, 13.7f, -7.13f, 5.3f, 1.2f, 0.1f, 0.2f,
                                     ui::MotionEventAndroid::GetAndroidToolType(
                                         ui::MotionEvent::ToolType::MOUSE));
  const float raw_offset_x = 11.f;
  const float raw_offset_y = 22.f;
  const float kPixToDip = 0.5f;

  ui::MotionEventAndroidJava motion_event(
      AttachCurrentThread(), nullptr, kPixToDip, 0.f, 0.f, 0.f,
      base::TimeTicks() + base::Nanoseconds(kEventTimeNs),
      AMOTION_EVENT_ACTION_DOWN, 1, 0, -1, 0, 0, 1, AMETA_ALT_ON, 0,
      raw_offset_x, raw_offset_y, false, &p0, nullptr);

  WebMouseEvent web_event = input::WebMouseEventBuilder::Build(
      motion_event, blink::WebInputEvent::Type::kMouseDown, 1,
      ui::MotionEvent::BUTTON_PRIMARY);
  EXPECT_EQ(web_event.PositionInWidget().x(), p0.pos_x_pixels * kPixToDip);
  EXPECT_EQ(web_event.PositionInWidget().y(), p0.pos_y_pixels * kPixToDip);
  EXPECT_EQ(web_event.PositionInScreen().x(),
            (p0.pos_x_pixels + raw_offset_x) * kPixToDip);
  EXPECT_EQ(web_event.PositionInScreen().y(),
            (p0.pos_y_pixels + raw_offset_y) * kPixToDip);
  EXPECT_EQ(web_event.button, blink::WebPointerProperties::Button::kLeft);
  EXPECT_EQ(web_event.TimeStamp(), event_time);
}

// TODO(crbug.com/41353469): Add more tests for WebMouseEventBuilder
