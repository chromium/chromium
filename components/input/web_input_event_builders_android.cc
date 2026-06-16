// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/web_input_event_builders_android.h"

#include <android/input.h>

#include "base/check.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/android/key_event_utils.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/features.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/scroll_types.h"

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebGestureEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPointerProperties;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace input {

namespace {

const char* InputSourceToString(int source) {
  switch (source) {
    case AINPUT_SOURCE_UNKNOWN:
      return "UNKNOWN";
    case AINPUT_SOURCE_KEYBOARD:
      return "KEYBOARD";
    case AINPUT_SOURCE_DPAD:
      return "DPAD";
    case AINPUT_SOURCE_GAMEPAD:
      return "GAMEPAD";
    case AINPUT_SOURCE_TOUCHSCREEN:
      return "TOUCHSCREEN";
    case AINPUT_SOURCE_MOUSE:
      return "MOUSE";
    case AINPUT_SOURCE_STYLUS:
      return "STYLUS";
    case AINPUT_SOURCE_TRACKBALL:
      return "TRACKBALL";
    case AINPUT_SOURCE_MOUSE_RELATIVE:
      return "MOUSE_RELATIVE";
    case AINPUT_SOURCE_TOUCHPAD:
      return "TOUCHPAD";
    case AINPUT_SOURCE_TOUCH_NAVIGATION:
      return "TOUCH_NAVIGATION";
    case AINPUT_SOURCE_ROTARY_ENCODER:
      return "ROTARY_ENCODER";
    case AINPUT_SOURCE_JOYSTICK:
      return "JOYSTICK";
    default:
      return "OTHER";
  }
}

int WebInputEventToAndroidModifier(int web_modifier) {
  int android_modifier = 0;
  // Currently only Shift, CapsLock are used, add other modifiers if required.
  if (web_modifier & WebInputEvent::kShiftKey)
    android_modifier |= AMETA_SHIFT_ON;
  if (web_modifier & WebInputEvent::kCapsLockOn)
    android_modifier |= AMETA_CAPS_LOCK_ON;
  return android_modifier;
}

ui::DomKey GetDomKeyFromEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& android_key_event,
    int keycode,
    int modifiers,
    int unicode_character) {
  // Synthetic key event, not enough information to get DomKey.
  if (android_key_event.is_null() && !unicode_character)
    return ui::DomKey::UNIDENTIFIED;

  if (!unicode_character && env) {
    // According to spec |kAllowedModifiers| should be Shift and AltGr, however
    // Android doesn't have AltGr key and ImeAdapter::getModifiers won't pass it
    // either.
    // According to discussion we want to honor CapsLock and possibly NumLock as
    // well. https://github.com/w3c/uievents/issues/70
    const int kAllowedModifiers =
        WebInputEvent::kShiftKey | WebInputEvent::kCapsLockOn;
    int fallback_modifiers =
        WebInputEventToAndroidModifier(modifiers & kAllowedModifiers);

    unicode_character = ui::events::android::GetKeyEventUnicodeChar(
        env, android_key_event, fallback_modifiers);
  }

  ui::DomKey key = ui::GetDomKeyFromAndroidEvent(keycode, unicode_character);
  if (key != ui::DomKey::NONE)
    return key;
  return ui::DomKey::UNIDENTIFIED;
}

bool IsConfirmedPhysicalKeyboardEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& android_key_event) {
  if (android_key_event.is_null()) {
    // Synthetic key event, not enough information to detect source.
    return false;
  }
  return !ui::events::android::IsVirtualKeyboardEvent(env, android_key_event);
}

}  // namespace

WebKeyboardEvent WebKeyboardEventBuilder::Build(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& android_key_event,
    WebInputEvent::Type type,
    int modifiers,
    base::TimeTicks time,
    int keycode,
    int scancode,
    int unicode_character,
    bool is_system_key) {
  DCHECK(WebInputEvent::IsKeyboardEventType(type));

  ui::DomCode dom_code = ui::DomCode::NONE;
  if (scancode)
    dom_code = ui::KeycodeConverter::NativeKeycodeToDomCode(scancode);

  WebKeyboardEvent result(
      type, modifiers | ui::DomCodeToWebInputEventModifiers(dom_code), time);
  result.windows_key_code = ui::LocatedToNonLocatedKeyboardCode(
      ui::KeyboardCodeFromAndroidKeyCode(keycode));
  result.native_key_code = keycode;
  result.dom_code = static_cast<int>(dom_code);
  result.dom_key = GetDomKeyFromEvent(env, android_key_event, keycode,
                                      modifiers, unicode_character);
  result.unmodified_text[0] = unicode_character;
  if (result.windows_key_code == ui::VKEY_RETURN) {
    // This is the same behavior as GTK:
    // We need to treat the enter key as a key press of character \r. This
    // is apparently just how webkit handles it and what it expects.
    result.unmodified_text[0] = '\r';
  }
  result.text[0] = result.unmodified_text[0];
  result.is_system_key = is_system_key;
  result.is_confirmed_physical_keyboard_input =
      IsConfirmedPhysicalKeyboardEvent(env, android_key_event);

  return result;
}

WebMouseEvent WebMouseEventBuilder::Build(
    const ui::MotionEventAndroid& motion_event,
    WebInputEvent::Type type,
    int click_count,
    int action_button) {
  DCHECK(WebInputEvent::IsMouseEventType(type));
  int modifiers = motion_event.GetFlags();
  WebMouseEvent result(type, ui::EventFlagsToWebEventModifiers(modifiers),
                       motion_event.GetEventTime());

  result.SetPositionInWidget(motion_event.GetX(0), motion_event.GetY(0));
  result.SetPositionInScreen(motion_event.GetRawX(0), motion_event.GetRawY(0));

  result.click_count = click_count;

  int button = action_button;
  // For events other than MouseDown/Up, action_button is not defined. So we are
  // determining |button| value from |modifiers| as is done in other platforms.
  if ((type != WebInputEvent::Type::kMouseDown &&
       type != WebInputEvent::Type::kMouseUp) ||
      // TODO(crbug.com/409639106): Also on MouseDown/Up events with undefined button since
      // MotionEvent.obtain used to simlulate inputs events in tests does not set the button
      // correctly.
      button == 0) {
    if (modifiers & ui::EF_LEFT_MOUSE_BUTTON)
      button = ui::MotionEvent::BUTTON_PRIMARY;
    else if (modifiers & ui::EF_MIDDLE_MOUSE_BUTTON)
      button = ui::MotionEvent::BUTTON_TERTIARY;
    else if (modifiers & ui::EF_RIGHT_MOUSE_BUTTON)
      button = ui::MotionEvent::BUTTON_SECONDARY;
    else
      button = 0;
  }

  ui::SetWebPointerPropertiesFromMotionEventData(
      result, motion_event.GetPointerId(0), motion_event.GetPressure(0),
      motion_event.GetOrientation(0), motion_event.GetTiltX(0),
      motion_event.GetTiltY(0), motion_event.GetTwist(0),
      motion_event.GetTangentialPressure(0), button,
      motion_event.GetToolType(0));

  return result;
}

WebMouseWheelEvent WebMouseWheelEventBuilder::Build(
    const ui::MotionEventAndroid& motion_event) {
  TRACE_EVENT("input", "WebMouseWheelEventBuilder::Build", "source",
              InputSourceToString(motion_event.GetSource()), "tool_type",
              motion_event.GetToolType(0));
  WebMouseWheelEvent result(WebInputEvent::Type::kMouseWheel,
                            WebInputEvent::kNoModifiers,
                            motion_event.GetEventTime());
  result.SetPositionInWidget(motion_event.GetX(0), motion_event.GetY(0));
  result.SetPositionInScreen(motion_event.GetRawX(0), motion_event.GetRawY(0));
  result.button = WebMouseEvent::Button::kNoButton;

  bool is_touchpad = motion_event.GetSource() == AINPUT_SOURCE_TOUCHPAD;

  // Touchpads can be represented in several ways on Android. It is important to
  // mark those as precise, otherwise with smooth scrolling, we get
  // acceleration, which translates to very high latency for the user.
  //
  // Note that in practice, the tool has been seen to be set to UNKNOWN locally,
  // so this may overtrigger, but there isn't an easy clean way to make sure
  // that touchpads are properly handled. However, it is properly set to MOUSE
  // locally when using an external USB mouse, so we still get smooth scrolling.
  if (base::FeatureList::IsEnabled(ui::kAndroidTouchpadDetection)) {
    is_touchpad =
        (motion_event.GetSource() & AINPUT_SOURCE_TOUCHPAD) ==
            AINPUT_SOURCE_TOUCHPAD ||
        ((motion_event.GetSource() & AINPUT_SOURCE_MOUSE) ==
             AINPUT_SOURCE_MOUSE &&
         motion_event.GetToolType(0) != ui::MotionEvent::ToolType::MOUSE);
  }
  result.delta_units = is_touchpad
                           ? ui::ScrollGranularity::kScrollByPrecisePixel
                           : ui::ScrollGranularity::kScrollByPixel;
  // For vertical scrolling (delta_y, wheel_ticks_y), Android's MotionEvent
  // provides values that align with "natural" scrolling (content moves in the
  // direction of the scroll).
  // For horizontal scrolling (delta_x, wheel_ticks_x), the MotionEvent
  // historically provided values that matched traditional desktop behavior
  // (content moves opposite to scroll direction). To align with Android's
  // "natural" scrolling expectation for horizontal input, we negate the x-axis
  // values.
  // However, this negation should ONLY apply to Physical Mouse inputs.
  // Other sources like Touchpads (Source Mouse + Tool Finger or Source
  // Touchpad), and Joysticks already provide "natural" direction values (or
  // standard
  // Cartesian direction) and should not be negated.
  // Note: AINPUT_SOURCE_TRACKBALL and AINPUT_SOURCE_MOUSE_RELATIVE are
  // AINPUT_SOURCE_CLASS_NAVIGATION (similar to Joysticks), so they are also
  // excluded from this negation.
  bool is_physical_mouse_scroll =
      (motion_event.GetSource() & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE &&
      motion_event.GetToolType(0) == ui::MotionEvent::ToolType::MOUSE;
  if (is_physical_mouse_scroll) {
    result.delta_x = -motion_event.ticks_x() * motion_event.GetTickMultiplier();
    result.wheel_ticks_x = -motion_event.ticks_x();
  } else {
    result.delta_x = motion_event.ticks_x() * motion_event.GetTickMultiplier();
    result.wheel_ticks_x = motion_event.ticks_x();
  }
  result.delta_y = motion_event.ticks_y() * motion_event.GetTickMultiplier();

  result.wheel_ticks_y = motion_event.ticks_y();
  result.SetModifiers(
      ui::EventFlagsToWebEventModifiers(motion_event.GetFlags()));

  switch (motion_event.GetAction()) {
    case ui::MotionEvent::Action::DOWN:
      result.phase = blink::WebMouseWheelEvent::kPhaseBegan;
      break;
    case ui::MotionEvent::Action::MOVE:
      result.phase = result.delta_x || result.delta_y
                         ? blink::WebMouseWheelEvent::kPhaseChanged
                         : blink::WebMouseWheelEvent::kPhaseStationary;
      break;
    case ui::MotionEvent::Action::UP:
    case ui::MotionEvent::Action::CANCEL:
      result.phase = blink::WebMouseWheelEvent::kPhaseEnded;
      break;
    default:
      result.phase = blink::WebMouseWheelEvent::kPhaseNone;
      break;
  }

  return result;
}

WebGestureEvent WebGestureEventBuilder::Build(WebInputEvent::Type type,
                                              base::TimeTicks time,
                                              float x,
                                              float y) {
  DCHECK(WebInputEvent::IsGestureEventType(type));
  WebGestureEvent result(type, WebInputEvent::kNoModifiers, time,
                         blink::WebGestureDevice::kTouchscreen);
  result.SetPositionInWidget(gfx::PointF(x, y));

  return result;
}

}  // namespace input
