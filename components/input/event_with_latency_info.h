// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_EVENT_WITH_LATENCY_INFO_H_
#define COMPONENTS_INPUT_EVENT_WITH_LATENCY_INFO_H_

#include "base/check_op.h"
#include "components/input/native_web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/latency/latency_info.h"

namespace input {

template <typename T>
class EventWithLatencyInfo {
 public:
  T event;
  mutable ui::LatencyInfo latency;

  explicit EventWithLatencyInfo(const T& e) : event(e) {}

  EventWithLatencyInfo(const T& e, const ui::LatencyInfo& l)
      : event(e), latency(l) {}

  EventWithLatencyInfo(blink::WebInputEvent::Type type,
                       int modifiers,
                       base::TimeTicks time_stamp,
                       const ui::LatencyInfo& l)
      : event(type, modifiers, time_stamp), latency(l) {}

  EventWithLatencyInfo() {}

  [[nodiscard]] bool CanCoalesceWith(const EventWithLatencyInfo& other) const {
    if (other.event.GetType() != event.GetType())
      return false;

    return event.CanCoalesce(other.event);
  }

  void CoalesceWith(const EventWithLatencyInfo& other) {
    // New events get coalesced into older events, and the newer timestamp
    // should always be preserved.
    const base::TimeTicks time_stamp = other.event.TimeStamp();
    event.Coalesce(other.event);
    event.SetTimeStamp(time_stamp);

    // When coalescing two input events, we keep the oldest LatencyInfo
    // for Telemetry latency tests, since it will represent the longest
    // latency.
    other.latency = latency;
    other.latency.set_coalesced();
  }
};

typedef EventWithLatencyInfo<blink::WebGestureEvent>
    GestureEventWithLatencyInfo;
typedef EventWithLatencyInfo<blink::WebMouseWheelEvent>
    MouseWheelEventWithLatencyInfo;
typedef EventWithLatencyInfo<blink::WebMouseEvent>
    MouseEventWithLatencyInfo;
typedef EventWithLatencyInfo<blink::WebTouchEvent>
    TouchEventWithLatencyInfo;

typedef EventWithLatencyInfo<NativeWebKeyboardEvent>
    NativeWebKeyboardEventWithLatencyInfo;

}  // namespace input

#endif  // COMPONENTS_INPUT_EVENT_WITH_LATENCY_INFO_H_
