// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_TOUCH_EVENT_STREAM_VALIDATOR_H_
#define COMPONENTS_INPUT_TOUCH_EVENT_STREAM_VALIDATOR_H_

#include <string>

#include "base/component_export.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

namespace input {

// Utility class for validating a stream of WebTouchEvents.
class COMPONENT_EXPORT(INPUT) TouchEventStreamValidator {
 public:
  TouchEventStreamValidator();

  TouchEventStreamValidator(const TouchEventStreamValidator&) = delete;
  TouchEventStreamValidator& operator=(const TouchEventStreamValidator&) =
      delete;

  ~TouchEventStreamValidator();

  // If |event| is valid for the current stream, returns true.
  // Otherwise, returns false with a corresponding error message.
  bool Validate(const blink::WebTouchEvent& event, std::string* error_msg);

 private:
  blink::WebTouchEvent previous_event_;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_TOUCH_EVENT_STREAM_VALIDATOR_H_
