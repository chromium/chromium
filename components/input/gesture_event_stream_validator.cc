// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/gesture_event_stream_validator.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/events/blink/web_input_event_traits.h"

using blink::WebInputEvent;

namespace input {

GestureEventStreamValidator::GestureEventStreamValidator()
    : scrolling_(false), pinching_(false), waiting_for_tap_end_(false) {
}

GestureEventStreamValidator::~GestureEventStreamValidator() {
}

bool GestureEventStreamValidator::Validate(
    const blink::WebGestureEvent& event,
    std::string* error_msg) {
  DCHECK(error_msg);
  error_msg->clear();
  if (!WebInputEvent::IsGestureEventType(event.GetType())) {
    error_msg->append(base::StringPrintf(
        "Invalid gesture type: %s", WebInputEvent::GetName(event.GetType())));
  }
  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureScrollBegin:
      if (scrolling_)
        error_msg->append("Scroll begin during scroll\n");
      if (pinching_)
        error_msg->append("Scroll begin during pinch\n");
      scrolling_ = true;
      break;
    case WebInputEvent::Type::kGestureScrollUpdate:
      if (!scrolling_)
        error_msg->append("Scroll update outside of scroll\n");
      break;
    case WebInputEvent::Type::kGestureFlingStart:
      if (event.SourceDevice() == blink::WebGestureDevice::kTouchscreen &&
          !event.data.fling_start.velocity_x &&
          !event.data.fling_start.velocity_y) {
        error_msg->append("Zero velocity touchscreen fling\n");
      }
      if (!scrolling_)
        error_msg->append("Fling start outside of scroll\n");
      if (pinching_)
        error_msg->append("Flinging while pinching\n");
      // Don't reset scrolling_ since the GSE sent by the fling_controller_ at
      // the end of the fling resets it.
      break;
    case WebInputEvent::Type::kGestureScrollEnd:
      if (!scrolling_)
        error_msg->append("Scroll end outside of scroll\n");
      if (pinching_)
        error_msg->append("Ending scroll while pinching\n");
      scrolling_ = false;
      break;
    case WebInputEvent::Type::kGesturePinchBegin:
      if (pinching_)
        error_msg->append("Pinch begin during pinch\n");
      pinching_ = true;
      break;
    case WebInputEvent::Type::kGesturePinchUpdate:
      if (!pinching_)
        error_msg->append("Pinch update outside of pinch\n");
      break;
    case WebInputEvent::Type::kGesturePinchEnd:
      if (!pinching_)
        error_msg->append("Pinch end outside of pinch\n");
      pinching_ = false;
      break;
    case WebInputEvent::Type::kGestureTapDown:
      if (waiting_for_tap_end_)
        error_msg->append("Missing tap ending event before TapDown\n");
      waiting_for_tap_end_ = true;
      break;
    case WebInputEvent::Type::kGestureTapUnconfirmed:
      if (!waiting_for_tap_end_)
        error_msg->append("Missing TapDown event before TapUnconfirmed\n");
      break;
    case WebInputEvent::Type::kGestureTapCancel:
      if (!waiting_for_tap_end_)
        error_msg->append("Missing TapDown event before TapCancel\n");
      waiting_for_tap_end_ = false;
      break;
    case WebInputEvent::Type::kGestureTap:
      if (!waiting_for_tap_end_)
        error_msg->append("Missing TapDown event before Tap\n");
      waiting_for_tap_end_ = false;
      break;
    case WebInputEvent::Type::kGestureDoubleTap:
      // DoubleTap gestures may be synthetically inserted, and do not require a
      // preceding TapDown.
      waiting_for_tap_end_ = false;
      break;
    default:
      break;
  }
  // TODO(wjmaclean): At some future point we may wish to consider adding a
  // 'continuity check', requiring that all events between an initial tap-down
  // and whatever terminates the sequence to have the same source device type,
  // and that touchpad gestures are only found on ScrollEvents.
  if (event.SourceDevice() == blink::WebGestureDevice::kUninitialized)
    error_msg->append("Gesture event source is uninitialized.\n");

  return error_msg->empty();
}

}  // namespace input
