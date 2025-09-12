// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/input/web_input_event_builders_ios.h"

#import <UIKit/UIKit.h>

#include "base/apple/foundation_util.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/common/input/web_touch_point.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_ios.h"

#if !BUILDFLAG(IS_IOS_TVOS)
#import <BrowserEngineKit/BrowserEngineKit.h>
#endif

namespace input {

namespace {

constexpr size_t MAX_POINTERS = blink::WebTouchEvent::kTouchesLengthCap;
static UITouch* g_active_touches[MAX_POINTERS] = {};

size_t GetTouchPointerId(UITouch* touch) {
  for (size_t i = 0; i < MAX_POINTERS; ++i) {
    if (g_active_touches[i] == touch) {
      return i + 1;
    }
  }
  return 0;
}

void AddUITouch(UITouch* touch) {
  CHECK(GetTouchPointerId(touch) == 0);
  for (size_t i = 0; i < MAX_POINTERS; ++i) {
    if (!g_active_touches[i]) {
      g_active_touches[i] = touch;
      return;
    }
  }
}

void RemoveUITouch(UITouch* touch) {
  for (size_t i = 0; i < MAX_POINTERS; ++i) {
    if (g_active_touches[i] == touch) {
      g_active_touches[i] = nil;
      return;
    }
  }
  NOTREACHED();
}

int ModifiersFromEvent(UIKeyModifierFlags modifier_flags) {
  int modifiers = 0;

  if (modifier_flags & UIKeyModifierControl) {
    modifiers |= blink::WebInputEvent::kControlKey;
  }
  if (modifier_flags & UIKeyModifierShift) {
    modifiers |= blink::WebInputEvent::kShiftKey;
  }
  if (modifier_flags & UIKeyModifierAlternate) {
    modifiers |= blink::WebInputEvent::kAltKey;
  }
  if (modifier_flags & UIKeyModifierCommand) {
    modifiers |= blink::WebInputEvent::kMetaKey;
  }
  if (modifier_flags & UIKeyModifierAlphaShift) {
    modifiers |= blink::WebInputEvent::kCapsLockOn;
  }

  return modifiers;
}

blink::WebTouchPoint::State ToWebTouchPointState(UITouch* event,
                                                 bool was_changed) {
  // We will get an event for each actual changed phase.
  if (!was_changed) {
    return blink::WebTouchPoint::State::kStateStationary;
  }

  switch ([event phase]) {
    case UITouchPhaseBegan:
    case UITouchPhaseRegionEntered:
      return blink::WebTouchPoint::State::kStatePressed;
    case UITouchPhaseMoved:
    case UITouchPhaseRegionMoved:
      return blink::WebTouchPoint::State::kStateMoved;
    case UITouchPhaseEnded:
    case UITouchPhaseRegionExited:
      return blink::WebTouchPoint::State::kStateReleased;
    case UITouchPhaseCancelled:
      return blink::WebTouchPoint::State::kStateCancelled;
    case UITouchPhaseStationary:
      return blink::WebTouchPoint::State::kStateStationary;
  }
  NOTREACHED() << "Invalid MotionEvent::Action.";
}

void SetWebPointerPropertiesFromMotionEventData(
    blink::WebPointerProperties& webPointerProperties,
    int pointer_id,
    float pressure) {
  webPointerProperties.id = pointer_id;
  webPointerProperties.force = pressure;
  webPointerProperties.tilt_x = webPointerProperties.tilt_y = 0;
  webPointerProperties.twist = 0;
  webPointerProperties.tangential_pressure = 0;
  webPointerProperties.button = blink::WebPointerProperties::Button::kNoButton;
  webPointerProperties.pointer_type =
      blink::WebPointerProperties::PointerType::kTouch;
  // TODO(dtapuska): Support stylus.
}

blink::WebTouchPoint CreateWebTouchPoint(
    UIView* view,
    UITouch* event,
    bool was_changed,
    const std::optional<gfx::Vector2dF>& view_offset) {
  blink::WebTouchPoint touch;

  size_t pointer_index = GetTouchPointerId(event);
  CHECK(pointer_index != 0);

  SetWebPointerPropertiesFromMotionEventData(touch, pointer_index,
                                             [event force]);

  touch.state = ToWebTouchPointState(event, was_changed);
  gfx::PointF window_location = gfx::PointF([event locationInView:nil]);
  touch.SetPositionInScreen(window_location);

  gfx::PointF view_location;
  if (view_offset) {
    view_location = gfx::PointF(window_location);
    view_location += view_offset.value();
  } else {
    view_location = gfx::PointF([event locationInView:view]);
  }
  touch.SetPositionInWidget(view_location);

  float major_radius = event.majorRadius;
  float minor_radius = event.majorRadius;
  float orientation_deg = 0;

  DCHECK_GE(major_radius, 0);
  DCHECK_GE(minor_radius, 0);
  DCHECK_GE(major_radius, minor_radius);
  // Orientation lies in [-180, 180] for a stylus, and [-90, 90] for other
  // touchscreen inputs. There are exceptions on Android when a device is
  // rotated, yielding touch orientations in the range of [-180, 180].
  // Regardless, normalise to [-90, 90), allowing a small tolerance to account
  // for floating point conversion.
  // TODO(e_hakkinen): Also pass unaltered stylus orientation, avoiding loss of
  // quadrant information, see crbug.com/493728.
  DCHECK_GT(orientation_deg, -180.01f);
  DCHECK_LT(orientation_deg, 180.01f);
  if (orientation_deg >= 90.f) {
    orientation_deg -= 180.f;
  } else if (orientation_deg < -90.f) {
    orientation_deg += 180.f;
  }
  if (orientation_deg >= 0) {
    // The case orientation_deg == 0 is handled here on purpose: although the
    // 'else' block is equivalent in this case, we want to pass the 0 value
    // unchanged (and 0 is the default value for many devices that don't
    // report elliptical touches).
    touch.radius_x = minor_radius;
    touch.radius_y = major_radius;
    touch.rotation_angle = orientation_deg;
  } else {
    touch.radius_x = major_radius;
    touch.radius_y = minor_radius;
    touch.rotation_angle = orientation_deg + 90;
  }

  return touch;
}

NSString* FilterSpecialCharacter(NSString* str) {
  if ([str length] != 1) {
    return str;
  }
  unichar c = [str characterAtIndex:0];
  NSString* result = str;
  if (c == 0x7F) {
    // Backspace should be 8
    result = @"\x8";
  } else if (c >= 0xF700 && c <= 0xF7FF) {
    // Mac private use characters should be @"\0" (@"" won't work)
    // NSDeleteFunctionKey will also go into here
    // Use the range 0xF700~0xF7FF to match
    // http://www.opensource.apple.com/source/WebCore/WebCore-7601.1.55/platform/mac/KeyEventMac.mm
    result = @"\0";
  }
  return result;
}

bool IsSystemKeyEvent(const blink::WebKeyboardEvent& event) {
  // Windows and Linux set |isSystemKey| if alt is down. Blink looks at this
  // flag to decide if it should handle a key or not. E.g. alt-left/right
  // shouldn't be used by Blink to scroll the current page, because we want
  // to get that key back for it to do history navigation. Hence, the
  // corresponding situation on OS X is to set this for cmd key presses.

  // cmd-b and and cmd-i are system wide key bindings that OS X doesn't
  // handle for us, so the editor handles them.
  int modifiers = event.GetModifiers() & blink::WebInputEvent::kInputModifiers;
  if (modifiers == blink::WebInputEvent::kMetaKey &&
      event.windows_key_code == ui::VKEY_B) {
    return false;
  }
  if (modifiers == blink::WebInputEvent::kMetaKey &&
      event.windows_key_code == ui::VKEY_I) {
    return false;
  }

  return event.GetModifiers() & blink::WebInputEvent::kMetaKey;
}

}  // namespace

blink::WebKeyboardEvent WebKeyboardEventBuilder::Build(gfx::NativeEvent event) {
  ui::DomCode dom_code;
  ui::DomKey dom_key;
  bool is_key_up = false;
  double time_stamp_seconds;
  ui::KeyboardCode key_code;
  NSString* key_characters;
  UIKeyModifierFlags flags;
#if BUILDFLAG(IS_IOS_TVOS)
  UIPress* press = std::get<base::apple::OwnedUIPress>(event).Get();
  CHECK(press);

  // KeyCode from UIPress is UIKeyboardHIDUsage. Convert it to ui::KeyboardCode.
  key_code = ui::KeyboardCodeFromUIKeyCode(press.key.keyCode);
  dom_code = ui::DomCodeFromUIPress(press, key_code);
  is_key_up = press.phase == UIPressPhaseEnded;
  time_stamp_seconds = press.timestamp;
  dom_key = ui::DomKeyFromKeyboardCode(press, key_code);
  key_characters = press.key.characters;
  flags = press.key.modifierFlags;
#else
  BEKeyEntry* entry = std::get<base::apple::OwnedBEKeyEntry>(event).Get();
  CHECK(entry);

  dom_code = ui::DomCodeFromBEKeyEntry(entry);
  is_key_up = entry.state == BEKeyPressStateUp;
  time_stamp_seconds = entry.timestamp;
  // the keyCode is the keyboard code in BEKeyEntry
  key_code = static_cast<ui::KeyboardCode>(entry.key.keyCode);
  dom_key = ui::DomKeyFromBEKeyEntry(entry);
  key_characters = entry.key.characters;
  flags = entry.key.modifierFlags;
#endif
  int modifiers =
      ModifiersFromEvent(flags) | ui::DomCodeToWebInputEventModifiers(dom_code);

  blink::WebKeyboardEvent result(
      is_key_up ? blink::WebInputEvent::Type::kKeyUp
                : blink::WebInputEvent::Type::kKeyDown,
      modifiers, ui::EventTimeStampFromSeconds(time_stamp_seconds));

  bool is_numeric_keypad_keycode =
      key_code >= ui::VKEY_NUMPAD0 && key_code <= ui::VKEY_NUMPAD9;
  result.windows_key_code = is_numeric_keypad_keycode
                                ? key_code
                                : ui::LocatedToNonLocatedKeyboardCode(key_code);

  result.native_key_code = key_code;
  result.dom_code = static_cast<int>(dom_code);
  result.dom_key = dom_key;
  NSString* text_str = FilterSpecialCharacter(key_characters);
  NSString* unmodified_str = FilterSpecialCharacter(key_characters);
  // Always use 13 for Enter/Return -- we don't want to use AppKit's
  // different character for Enter.
  if (result.windows_key_code == '\r') {
    text_str = @"\r";
    unmodified_str = @"\r";
  }

  // Always use 9 for tab -- we don't want to use AppKit's different character
  // for shift-tab.
  if (result.windows_key_code == 9) {
    text_str = @"\x9";
    unmodified_str = @"\x9";
  }

  if ([text_str length] < blink::WebKeyboardEvent::kTextLengthCap &&
      [unmodified_str length] < blink::WebKeyboardEvent::kTextLengthCap) {
    [text_str getCharacters:reinterpret_cast<UniChar*>(&result.text[0])];
    [unmodified_str
        getCharacters:reinterpret_cast<UniChar*>(&result.unmodified_text[0])];
  } else {
    NOTIMPLEMENTED();
  }

  result.is_system_key = IsSystemKeyEvent(result);

  return result;
}

blink::WebGestureEvent WebGestureEventBuilder::Build(UIEvent*, UIView*) {
  return blink::WebGestureEvent();
}

blink::WebTouchEvent WebTouchEventBuilder::Build(
    blink::WebInputEvent::Type type,
    UITouch* touch,
    UIEvent* event,
    UIView* view,
    const std::optional<gfx::Vector2dF>& view_offset) {
  blink::WebTouchEvent result(type, ModifiersFromEvent(event.modifierFlags),
                              ui::EventTimeStampFromSeconds([event timestamp]));
  // TODO(dtapuska): Enable
  //   ui::ComputeEventLatencyOS(event);
  result.dispatch_type =
      result.GetType() == blink::WebInputEvent::Type::kTouchCancel
          ? blink::WebInputEvent::DispatchType::kEventNonBlocking
          : blink::WebInputEvent::DispatchType::kBlocking;
  result.hovering = type == blink::WebInputEvent::Type::kTouchEnd;
  result.unique_touch_event_id = ui::GetNextTouchEventId();

  size_t touch_index = 0;
  if (type == blink::WebInputEvent::Type::kTouchStart) {
    AddUITouch(touch);
  }
  result.touches[touch_index] =
      CreateWebTouchPoint(view, touch, /*was_changed=*/true, view_offset);
  ++touch_index;
  if (type == blink::WebInputEvent::Type::kTouchCancel ||
      type == blink::WebInputEvent::Type::kTouchEnd) {
    RemoveUITouch(touch);
  }

  // We can't use `event.allTouches` here, because we need to generate a
  // WebTouchEvent for each touch point changing. But event.allTouches will have
  // it all already.
  for (size_t i = 0; i < MAX_POINTERS; ++i) {
    if (!g_active_touches[i] || g_active_touches[i] == touch) {
      continue;
    }
    result.touches[touch_index] = CreateWebTouchPoint(
        view, g_active_touches[i], /*was_changed=*/false, view_offset);
    ++touch_index;
  }
  result.touches_length = touch_index;
  DCHECK_GT(result.touches_length, 0U);

  return result;
}

}  // namespace input
