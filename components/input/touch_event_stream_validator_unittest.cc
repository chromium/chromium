// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "components/input/touch_event_stream_validator.h"

#include <stddef.h>

#include "components/input/web_touch_event_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace input {

TEST(TouchEventStreamValidator, ValidTouchStream) {
  TouchEventStreamValidator validator;
  blink::SyntheticWebTouchEvent event;
  std::string error_msg;

  event.PressPoint(0, 1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.PressPoint(1, 0);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.MovePoint(1, 1, 1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.ReleasePoint(1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.MovePoint(0, -1, 0);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.CancelPoint(0);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.PressPoint(-1, -1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(TouchEventStreamValidator, ResetOnNewTouchStream) {
  TouchEventStreamValidator validator;
  blink::SyntheticWebTouchEvent event;
  std::string error_msg;

  event.PressPoint(0, 1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.CancelPoint(0);
  event.ResetPoints();
  event.PressPoint(1, 0);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(TouchEventStreamValidator, MissedTouchStart) {
  TouchEventStreamValidator validator;
  blink::SyntheticWebTouchEvent event;
  std::string error_msg;

  event.PressPoint(0, 1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event.PressPoint(1, 0);
  event.ResetPoints();
  event.PressPoint(1, 1);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(TouchEventStreamValidator, MissedTouchEnd) {
  TouchEventStreamValidator validator;
  blink::SyntheticWebTouchEvent event;
  std::string error_msg;

  event.PressPoint(0, 1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.PressPoint(0, 1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.ReleasePoint(1);
  event.ResetPoints();
  event.PressPoint(1, 1);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(TouchEventStreamValidator, EmptyEvent) {
  TouchEventStreamValidator validator;
  WebTouchEvent event;
  std::string error_msg;

  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(TouchEventStreamValidator, InvalidEventType) {
  TouchEventStreamValidator validator;
  WebTouchEvent event(WebInputEvent::Type::kGestureScrollBegin,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  std::string error_msg;

  event.touches_length = 1;
  event.touches[0].state = WebTouchPoint::State::kStatePressed;

  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(TouchEventStreamValidator, InvalidPointStates) {
  TouchEventStreamValidator validator;
  std::string error_msg;

  WebInputEvent::Type kTouchTypes[4] = {
      WebInputEvent::Type::kTouchStart,
      WebInputEvent::Type::kTouchMove,
      WebInputEvent::Type::kTouchEnd,
      WebInputEvent::Type::kTouchCancel,
  };

  WebTouchPoint::State kValidTouchPointStatesForType[4] = {
      WebTouchPoint::State::kStatePressed,
      WebTouchPoint::State::kStateMoved,
      WebTouchPoint::State::kStateReleased,
      WebTouchPoint::State::kStateCancelled,
  };

  blink::SyntheticWebTouchEvent start;
  start.PressPoint(0, 0);
  for (size_t i = 0; i < 4; ++i) {
    // Always start with a touchstart to reset the stream validation.
    EXPECT_TRUE(validator.Validate(start, &error_msg));
    EXPECT_TRUE(error_msg.empty());

    WebTouchEvent event(kTouchTypes[i], WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests());
    event.touches_length = 1;
    for (size_t j = static_cast<size_t>(WebTouchPoint::State::kStateUndefined);
         j <= static_cast<size_t>(WebTouchPoint::State::kStateCancelled); ++j) {
      event.touches[0].state = static_cast<WebTouchPoint::State>(j);
      if (event.touches[0].state == kValidTouchPointStatesForType[i]) {
        EXPECT_TRUE(validator.Validate(event, &error_msg));
        EXPECT_TRUE(error_msg.empty());
      } else {
        EXPECT_FALSE(validator.Validate(event, &error_msg));
        EXPECT_FALSE(error_msg.empty());
      }
    }
  }
}

}  // namespace input
