// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/tap_suppression_controller.h"

#include <memory>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace input {

class MockTapSuppressionController : public TapSuppressionController {
 public:
  using TapSuppressionController::DISABLED;
  using TapSuppressionController::LAST_CANCEL_STOPPED_FLING;
  using TapSuppressionController::NOTHING;
  using TapSuppressionController::SUPPRESSING_TAPS;

  enum Action {
    NONE = 0,
    TAP_DOWN_FORWARDED = 1 << 0,
    TAP_DOWN_SUPPRESSED = 1 << 1,
    TAP_UP_SUPPRESSED = 1 << 2,
    TAP_UP_FORWARDED = 1 << 3,
  };

  MockTapSuppressionController(const TapSuppressionController::Config& config)
      : TapSuppressionController(config), last_actions_(NONE), time_() {}

  MockTapSuppressionController(const MockTapSuppressionController&) = delete;
  MockTapSuppressionController& operator=(const MockTapSuppressionController&) =
      delete;

  ~MockTapSuppressionController() override {}

  void NotifyGestureFlingCancelStoppedFling() {
    last_actions_ = NONE;
    GestureFlingCancelStoppedFling();
  }

  void SendTapDown() {
    last_actions_ = NONE;
    if (ShouldSuppressTapDown()) {
      last_actions_ |= TAP_DOWN_SUPPRESSED;
    } else {
      last_actions_ |= TAP_DOWN_FORWARDED;
    }
  }

  void SendTapUp() {
    last_actions_ = NONE;
    if (ShouldSuppressTapEnd()) {
      last_actions_ |= TAP_UP_SUPPRESSED;
    } else {
      last_actions_ |= TAP_UP_FORWARDED;
    }
  }

  void AdvanceTime(const base::TimeDelta& delta) {
    last_actions_ = NONE;
    time_ += delta;
  }

  State state() { return state_; }

  int last_actions() { return last_actions_; }

 protected:
  base::TimeTicks Now() override { return time_; }

 private:
  // Hiding some derived public methods
  using TapSuppressionController::GestureFlingCancelStoppedFling;
  using TapSuppressionController::ShouldSuppressTapDown;
  using TapSuppressionController::ShouldSuppressTapEnd;

  int last_actions_;

  base::TimeTicks time_;
};

class TapSuppressionControllerTest : public testing::Test {
 public:
  TapSuppressionControllerTest() {}
  ~TapSuppressionControllerTest() override {}

 protected:
  // testing::Test
  void SetUp() override {
    tap_suppression_controller_ =
        std::make_unique<MockTapSuppressionController>(GetConfig());
  }

  void TearDown() override { tap_suppression_controller_.reset(); }

  static TapSuppressionController::Config GetConfig() {
    TapSuppressionController::Config config;
    config.enabled = true;
    config.max_cancel_to_down_time = base::Milliseconds(10);
    return config;
  }

  std::unique_ptr<MockTapSuppressionController> tap_suppression_controller_;
};

// Test TapSuppressionController for when GestureFlingCancel actually stops
// fling and the tap down event arrives without any delay.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapFast) {
  // Notify the controller that the GFC has stooped an active fling.
  tap_suppression_controller_->NotifyGestureFlingCancelStoppedFling();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_SUPPRESSED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::SUPPRESSING_TAPS,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_SUPPRESSED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::SUPPRESSING_TAPS,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel actually stops
// fling but there is a small delay between the Ack and TapDown.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapInsufficientlyLateTapDown) {
  // Notify the controller that the GFC has stooped an active fling.
  tap_suppression_controller_->NotifyGestureFlingCancelStoppedFling();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Wait less than allowed delay between GestureFlingCancel and TapDown, so the
  // TapDown is still considered associated with the GestureFlingCancel.
  tap_suppression_controller_->AdvanceTime(base::Milliseconds(7));
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_SUPPRESSED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::SUPPRESSING_TAPS,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_SUPPRESSED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::SUPPRESSING_TAPS,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel actually stops
// fling but there is a long delay between the Ack and TapDown.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapSufficientlyLateTapDown) {
  // Notify the controller that the GFC has stooped an active fling.
  tap_suppression_controller_->NotifyGestureFlingCancelStoppedFling();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Wait more than allowed delay between GestureFlingCancel and TapDown, so the
  // TapDown is not considered associated with the GestureFlingCancel.
  tap_suppression_controller_->AdvanceTime(base::Milliseconds(13));
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should not be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());

  // Send MouseUp. This MouseUp should not be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test that no suppression occurs if the TapSuppressionController is disabled.
TEST_F(TapSuppressionControllerTest, NoSuppressionIfDisabled) {
  TapSuppressionController::Config disabled_config;
  disabled_config.enabled = false;
  tap_suppression_controller_ =
      std::make_unique<MockTapSuppressionController>(disabled_config);

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->NotifyGestureFlingCancelStoppedFling();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::DISABLED,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should not be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::DISABLED,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should not be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::DISABLED,
            tap_suppression_controller_->state());
}

}  // namespace input
