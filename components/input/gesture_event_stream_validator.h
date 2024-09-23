// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_GESTURE_EVENT_STREAM_VALIDATOR_H_
#define COMPONENTS_INPUT_GESTURE_EVENT_STREAM_VALIDATOR_H_

#include <string>

#include "base/component_export.h"

namespace blink {
class WebGestureEvent;
}

namespace input {

// Utility class for validating a stream of WebGestureEvents.
class COMPONENT_EXPORT(INPUT) GestureEventStreamValidator {
 public:
  GestureEventStreamValidator();

  GestureEventStreamValidator(const GestureEventStreamValidator&) = delete;
  GestureEventStreamValidator& operator=(const GestureEventStreamValidator&) =
      delete;

  ~GestureEventStreamValidator();

  // If |event| is valid for the current stream, returns true.
  // Otherwise, returns false with a corresponding error message.
  bool Validate(const blink::WebGestureEvent& event, std::string* error_msg);

 private:
  bool scrolling_;
  bool pinching_;
  bool waiting_for_tap_end_;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_GESTURE_EVENT_STREAM_VALIDATOR_H_
