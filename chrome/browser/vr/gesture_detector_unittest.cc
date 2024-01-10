// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/gesture_detector.h"

#include "base/time/time.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/platform_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr float kDelta = 0.001f;
}

namespace vr {

class MockPlatformController : public PlatformController {
 public:
  MockPlatformController() = default;
  MockPlatformController(bool touching_trackpad,
                         base::TimeTicks touch_timestamp)
      : is_touching_trackpad(touching_trackpad),
        last_touch_timestamp(touch_timestamp) {}

  bool IsButtonDown(PlatformController::ButtonType type) const override {
    switch (type) {
      case vr::PlatformController::kButtonSelect:
        return is_select_button_down;
      case vr::PlatformController::kButtonMenu:
        return is_menu_button_down;
      default:
        return false;
    }
  }

  bool ButtonUpHappened(PlatformController::ButtonType type) const override {
    switch (type) {
      case vr::PlatformController::kButtonSelect:
        return was_select_button_down_ && !is_select_button_down;
      case vr::PlatformController::kButtonMenu:
        return was_menu_button_down_ && !is_menu_button_down;
      default:
        return false;
    }
  }

  bool ButtonDownHappened(PlatformController::ButtonType type) const override {
    switch (type) {
      case vr::PlatformController::kButtonSelect:
        return !was_select_button_down_ && is_select_button_down;
      case vr::PlatformController::kButtonMenu:
        return !was_menu_button_down_ && is_menu_button_down;
      default:
        return false;
    }
  }

  bool IsTouchingTrackpad() const override { return is_touching_trackpad; }

  gfx::PointF GetPositionInTrackpad() const override {
    return position_in_trackpad;
  }

  base::TimeTicks GetLastOrientationTimestamp() const override {
    return base::TimeTicks();
  }

  base::TimeTicks GetLastTouchTimestamp() const override {
    return last_touch_timestamp;
  }

  base::TimeTicks GetLastButtonTimestamp() const override {
    return base::TimeTicks();
  }

  vr::ControllerModel::Handedness GetHandedness() const override {
    return vr::ControllerModel::kRightHanded;
  }

  bool GetRecentered() const override { return false; }

  int GetBatteryLevel() const override { return 100; }

  // Call before each frame, if the test needs button up/down events.
  void Update() {
    was_select_button_down_ = is_select_button_down;
    was_menu_button_down_ = is_menu_button_down;
  }

  bool is_touching_trackpad = false;
  bool is_select_button_down = false;
  bool is_menu_button_down = false;
  gfx::PointF position_in_trackpad;
  base::TimeTicks last_touch_timestamp;

 private:
  bool was_select_button_down_ = false;
  bool was_menu_button_down_ = false;
};

TEST(GestureDetector, StartTouchWithoutMoving) {
  GestureDetector detector;

  base::TimeTicks timestamp;

  MockPlatformController controller(true, timestamp);
  auto gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kFlingCancel);

  // A small move doesn't trigger scrolling yet.
  timestamp += base::Milliseconds(1);
  controller.last_touch_timestamp = timestamp;
  controller.position_in_trackpad = {kDelta, kDelta};
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_TRUE(gestures.empty());
}

TEST(GestureDetector, StartTouchMoveAndRelease) {
  GestureDetector detector;
  base::TimeTicks timestamp;

  MockPlatformController controller(true, timestamp);
  detector.DetectGestures(controller, timestamp);

  // Move to the right.
  timestamp += base::Milliseconds(1);
  controller.last_touch_timestamp = timestamp;
  controller.position_in_trackpad = {0.3f, 0.0f};
  auto gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollBegin);
  auto* gesture = static_cast<InputEvent*>(gestures.front().get());
  EXPECT_GT(gesture->scroll_data.delta_x, 0.0f);
  EXPECT_EQ(gesture->scroll_data.delta_y, 0.0f);

  // Move slightly up.
  timestamp += base::Milliseconds(1);
  controller.last_touch_timestamp = timestamp;
  controller.position_in_trackpad = {0.3f, 0.01f};
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollUpdate);
  gesture = static_cast<InputEvent*>(gestures.front().get());
  EXPECT_EQ(gesture->scroll_data.delta_x, 0.0f);
  EXPECT_GT(gesture->scroll_data.delta_y, 0.0f);

  // Release touch. Scroll is extrapolated for 2 frames.
  controller.is_touching_trackpad = false;
  timestamp += base::Milliseconds(1);
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollUpdate);
  gesture = static_cast<InputEvent*>(gestures.front().get());
  EXPECT_GT(gesture->scroll_data.delta_x, 0.0f);
  EXPECT_GT(gesture->scroll_data.delta_y, 0.0f);
  timestamp += base::Milliseconds(1);
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollUpdate);
  timestamp += base::Milliseconds(1);
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollEnd);
}

TEST(GestureDetector, CancelDuringScrolling) {
  GestureDetector detector;
  base::TimeTicks timestamp;

  MockPlatformController controller(true, timestamp);
  detector.DetectGestures(controller, timestamp);

  // Move to the right.
  timestamp += base::Milliseconds(1);
  controller.last_touch_timestamp = timestamp;
  controller.position_in_trackpad = {0.3f, 0.0f};
  auto gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollBegin);

  // Button down.
  timestamp += base::Milliseconds(1);
  controller.last_touch_timestamp = timestamp;
  controller.is_select_button_down = true;
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollEnd);
}

TEST(GestureDetector, CancelDuringPostScrolling) {
  GestureDetector detector;
  base::TimeTicks timestamp;

  MockPlatformController controller(true, timestamp);
  detector.DetectGestures(controller, timestamp);

  // Move to the right.
  timestamp += base::Milliseconds(1);
  controller.last_touch_timestamp = timestamp;
  controller.position_in_trackpad = {0.3f, 0.0f};
  auto gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollBegin);

  // Release touch. We should see extrapolated scrolling.
  timestamp += base::Milliseconds(1);
  controller.is_touching_trackpad = false;
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollUpdate);

  // Button down.
  timestamp += base::Milliseconds(1);
  controller.is_select_button_down = true;
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollEnd);
}

TEST(GestureDetector, CancelAndTouchDuringPostScrolling) {
  GestureDetector detector;
  base::TimeTicks timestamp;

  MockPlatformController controller(true, timestamp);
  detector.DetectGestures(controller, timestamp);

  // Move to the right.
  timestamp += base::Milliseconds(1);
  controller.last_touch_timestamp = timestamp;
  controller.position_in_trackpad = {0.3f, 0.0f};
  auto gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollBegin);

  // Release touch. We should see extrapolated scrolling.
  timestamp += base::Milliseconds(1);
  controller.is_touching_trackpad = false;
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollUpdate);

  // Touch and button down.
  timestamp += base::Milliseconds(1);
  controller.is_select_button_down = true;
  controller.is_touching_trackpad = true;
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kScrollEnd);
}

TEST(GestureDetector, ClickMenuButton) {
  GestureDetector detector;
  base::TimeTicks timestamp;

  // Touch down menu button.
  MockPlatformController controller;
  controller.is_menu_button_down = true;
  auto gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_TRUE(gestures.empty());

  // Release menu button.
  controller.Update();
  controller.is_menu_button_down = false;
  timestamp += base::Milliseconds(1);
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.size(), 1u);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kMenuButtonClicked);
}

TEST(GestureDetector, LongPressMenuButton) {
  GestureDetector detector;
  base::TimeTicks timestamp;

  // Touch down menu button.
  MockPlatformController controller;
  controller.is_menu_button_down = true;
  auto gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_TRUE(gestures.empty());

  // Keep menu button down.
  controller.Update();
  timestamp += base::Seconds(1);
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.size(), 1u);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kMenuButtonLongPressStart);

  // Release menu button.
  controller.Update();
  controller.is_menu_button_down = false;
  timestamp += base::Seconds(1);
  gestures = detector.DetectGestures(controller, timestamp);
  EXPECT_EQ(gestures.size(), 1u);
  EXPECT_EQ(gestures.front()->type(), InputEvent::kMenuButtonLongPressEnd);
}

}  // namespace vr
