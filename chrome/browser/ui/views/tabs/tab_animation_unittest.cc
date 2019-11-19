// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_animation.h"

#include <utility>

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr base::TimeDelta kZeroDuration = base::TimeDelta::FromMilliseconds(0);

}  // namespace

class TabAnimationTest : public testing::Test {
 public:
  TabAnimationTest()
      : env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  float CurrentPinnedness(const TabAnimation& animation) {
    return animation.GetCurrentState().pinnedness();
  }

  base::test::TaskEnvironment env_;
};

TEST_F(TabAnimationTest, StaticAnimationDoesNotChange) {
  TabAnimationState static_state = TabAnimationState::ForIdealTabState(
      TabOpen::kOpen, TabPinned::kUnpinned, TabActive::kInactive, 0);
  TabAnimation static_animation(static_state, base::BindOnce([]() {}));

  EXPECT_EQ(kZeroDuration, static_animation.GetTimeRemaining());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0),
            static_animation.GetTimeRemaining());
  EXPECT_EQ(static_state.pinnedness(), CurrentPinnedness(static_animation));

  env_.FastForwardBy(TabAnimation::kAnimationDuration);
  EXPECT_EQ(static_state.pinnedness(), CurrentPinnedness(static_animation));
}

TEST_F(TabAnimationTest, AnimationAnimates) {
  TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
      TabOpen::kOpen, TabPinned::kUnpinned, TabActive::kInactive, 0);
  TabAnimationState target_state = initial_state.WithPinned(TabPinned::kPinned);
  TabAnimation animation(initial_state, base::BindOnce([]() {}));
  animation.AnimateTo(target_state);

  EXPECT_LT(kZeroDuration, animation.GetTimeRemaining());
  EXPECT_EQ(initial_state.pinnedness(), CurrentPinnedness(animation));

  env_.FastForwardBy(TabAnimation::kAnimationDuration / 2.0);

  EXPECT_LT(kZeroDuration, animation.GetTimeRemaining());
  EXPECT_LT(initial_state.pinnedness(), CurrentPinnedness(animation));
  EXPECT_LT(CurrentPinnedness(animation), target_state.pinnedness());

  env_.FastForwardBy(TabAnimation::kAnimationDuration / 2.0);

  EXPECT_EQ(target_state.pinnedness(), CurrentPinnedness(animation));
}

TEST_F(TabAnimationTest, CompletedAnimationSnapsToTarget) {
  TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
      TabOpen::kOpen, TabPinned::kUnpinned, TabActive::kInactive, 0);
  TabAnimationState target_state = initial_state.WithPinned(TabPinned::kPinned);
  TabAnimation animation(initial_state, base::BindOnce([]() {}));
  animation.AnimateTo(target_state);

  animation.CompleteAnimation();

  EXPECT_EQ(kZeroDuration, animation.GetTimeRemaining());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0), animation.GetTimeRemaining());
  EXPECT_EQ(target_state.pinnedness(), CurrentPinnedness(animation));
}

TEST_F(TabAnimationTest, ReplacedAnimationRestartsDuration) {
  TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
      TabOpen::kOpen, TabPinned::kUnpinned, TabActive::kInactive, 0);
  TabAnimationState target_state = initial_state.WithPinned(TabPinned::kPinned);
  TabAnimation animation(initial_state, base::BindOnce([]() {}));
  animation.AnimateTo(target_state);

  env_.FastForwardBy(TabAnimation::kAnimationDuration / 2.0);
  TabAnimationState reversal_state = animation.GetCurrentState();
  animation.AnimateTo(initial_state);

  EXPECT_EQ(reversal_state.pinnedness(), CurrentPinnedness(animation));

  EXPECT_EQ(TabAnimation::kAnimationDuration, animation.GetTimeRemaining());
}

TEST_F(TabAnimationTest, RetargetedAnimationKeepsDuration) {
  TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
      TabOpen::kOpen, TabPinned::kUnpinned, TabActive::kInactive, 0);
  TabAnimationState target_state = initial_state.WithPinned(TabPinned::kPinned);
  TabAnimation animation(initial_state, base::BindOnce([]() {}));
  animation.AnimateTo(target_state);

  env_.FastForwardBy(TabAnimation::kAnimationDuration / 2.0);
  EXPECT_EQ(TabAnimation::kAnimationDuration / 2.0,
            animation.GetTimeRemaining());
  animation.RetargetTo(initial_state);

  EXPECT_EQ(TabAnimation::kAnimationDuration / 2.0,
            animation.GetTimeRemaining());

  env_.FastForwardBy(TabAnimation::kAnimationDuration);
  EXPECT_EQ(initial_state.pinnedness(), CurrentPinnedness(animation));
}

TEST_F(TabAnimationTest, TestNotifyCloseCompleted) {
  class TabClosedDetector {
   public:
    void NotifyTabClosed() { was_closed_ = true; }

    bool was_closed_ = false;
  };
  TabAnimationState static_state = TabAnimationState::ForIdealTabState(
      TabOpen::kOpen, TabPinned::kUnpinned, TabActive::kInactive, 0);
  TabClosedDetector tab_closed_detector;
  TabAnimation animation(
      static_state, base::BindOnce(&TabClosedDetector::NotifyTabClosed,
                                   base::Unretained(&tab_closed_detector)));
  EXPECT_FALSE(tab_closed_detector.was_closed_);

  animation.NotifyCloseCompleted();

  EXPECT_TRUE(tab_closed_detector.was_closed_);
}
