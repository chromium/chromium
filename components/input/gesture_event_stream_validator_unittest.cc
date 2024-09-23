// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/gesture_event_stream_validator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace input {
namespace {

const blink::WebGestureDevice kDefaultGestureDevice =
    blink::WebGestureDevice::kTouchscreen;

blink::WebGestureEvent Build(WebInputEvent::Type type) {
  blink::WebGestureEvent event = blink::SyntheticWebGestureEventBuilder::Build(
      type, kDefaultGestureDevice);
  // Default to providing a (valid) non-zero fling velocity.
  if (type == WebInputEvent::Type::kGestureFlingStart)
    event.data.fling_start.velocity_x = 5;
  return event;
}

}  // namespace

TEST(GestureEventStreamValidator, ValidScroll) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  event = Build(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureScrollUpdate);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureScrollEnd);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(GestureEventStreamValidator, InvalidScroll) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  // No preceding ScrollBegin.
  event = Build(WebInputEvent::Type::kGestureScrollUpdate);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // No preceding ScrollBegin.
  event = Build(WebInputEvent::Type::kGestureScrollEnd);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // Already scrolling.
  event = Build(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureScrollEnd);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // Scroll already ended.
  event = Build(WebInputEvent::Type::kGestureScrollEnd);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(GestureEventStreamValidator, ValidFling) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  event = Build(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureFlingStart);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(GestureEventStreamValidator, InvalidFling) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  // No preceding ScrollBegin.
  event = Build(WebInputEvent::Type::kGestureFlingStart);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // Zero velocity.
  event = Build(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureFlingStart);
  event.data.fling_start.velocity_x = 0;
  event.data.fling_start.velocity_y = 0;
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(GestureEventStreamValidator, ValidPinch) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  event = Build(WebInputEvent::Type::kGesturePinchBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGesturePinchUpdate);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGesturePinchEnd);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(GestureEventStreamValidator, InvalidPinch) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  // No preceding PinchBegin.
  event = Build(WebInputEvent::Type::kGesturePinchUpdate);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGesturePinchBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // ScrollBegin while pinching.
  event = Build(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // ScrollEnd while pinching.
  event = Build(WebInputEvent::Type::kGestureScrollEnd);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // Pinch already begun.
  event = Build(WebInputEvent::Type::kGesturePinchBegin);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGesturePinchEnd);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // Pinch already ended.
  event = Build(WebInputEvent::Type::kGesturePinchEnd);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(GestureEventStreamValidator, ValidTap) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  event = Build(WebInputEvent::Type::kGestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureTapCancel);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureTapUnconfirmed);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureTapCancel);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureTap);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // DoubleTap does not require a TapDown (unlike Tap, TapUnconfirmed and
  // TapCancel).
  event = Build(WebInputEvent::Type::kGestureDoubleTap);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(GestureEventStreamValidator, InvalidTap) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  // No preceding TapDown.
  event = Build(WebInputEvent::Type::kGestureTapUnconfirmed);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureTapCancel);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureTap);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // TapDown already terminated.
  event = Build(WebInputEvent::Type::kGestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureDoubleTap);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureTapCancel);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // TapDown already terminated.
  event = Build(WebInputEvent::Type::kGestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureTap);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::Type::kGestureTapCancel);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

}  // namespace input
