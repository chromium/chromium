// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/input_event_stream_validator.h"

#include "base/check.h"
#include "base/command_line.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "components/input/switches.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;
using blink::WebTouchEvent;

namespace input {

InputEventStreamValidator::InputEventStreamValidator()
    : enabled_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kValidateInputEventStream)) {
}

InputEventStreamValidator::~InputEventStreamValidator() {
}

void InputEventStreamValidator::Validate(const WebInputEvent& event) {
  if (!enabled_)
    return;

  DCHECK(ValidateImpl(event, &error_msg_))
      << error_msg_
      << "\nInvalid Event: " << ui::WebInputEventTraits::ToString(event);
}

bool InputEventStreamValidator::ValidateImpl(
    const blink::WebInputEvent& event,
    std::string* error_msg) {
  DCHECK(error_msg);
  if (WebInputEvent::IsGestureEventType(event.GetType())) {
    const WebGestureEvent& gesture = static_cast<const WebGestureEvent&>(event);
    // TODO(jdduke): Validate touchpad gesture streams.
    if (gesture.SourceDevice() == blink::WebGestureDevice::kTouchscreen)
      return gesture_validator_.Validate(gesture, error_msg);
  } else if (WebInputEvent::IsTouchEventType(event.GetType())) {
    const WebTouchEvent& touch = static_cast<const WebTouchEvent&>(event);
    return touch_validator_.Validate(touch, error_msg);
  }
  return true;
}

}  // namespace input
