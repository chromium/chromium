// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/input/web_input_event_builders_ios.h"

#import <UIKit/UIKit.h>

#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/common/input/web_touch_point.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_utils.h"

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
  CHECK(false);
}

int ModifiersFromEvent(UIEvent* event) {
  int modifiers = 0;
  UIKeyModifierFlags modifier_flags = [event modifierFlags];

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
  NOTREACHED_IN_MIGRATION() << "Invalid MotionEvent::Action.";
  return blink::WebTouchPoint::State::kStateUndefined;
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

}  // namespace

blink::WebKeyboardEvent WebKeyboardEventBuilder::Build(UIEvent*) {
  return blink::WebKeyboardEvent();
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
  blink::WebTouchEvent result(type, ModifiersFromEvent(event),
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
