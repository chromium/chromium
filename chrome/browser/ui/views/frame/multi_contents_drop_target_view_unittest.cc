// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"

#include "base/time/time.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace {

constexpr int kDelayedAnimationDuration = 60;

class DropTargetViewTest : public ChromeViewsTestBase {
 protected:
  DropTargetViewTest() {
    drop_target_view_.animation_for_testing().SetSlideDuration(
        base::Seconds(0));
  }

  MultiContentsDropTargetView* drop_target_view() { return &drop_target_view_; }

 private:
  MultiContentsDropTargetView drop_target_view_;
};

TEST_F(DropTargetViewTest, ViewIsOpened) {
  MultiContentsDropTargetView* view = drop_target_view();

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() == 0);

  view->Show();

  EXPECT_TRUE(view->GetVisible());
  EXPECT_TRUE(view->icon_view_for_testing()->GetVisible());
}

TEST_F(DropTargetViewTest, ViewIsClosed) {
  MultiContentsDropTargetView* view = drop_target_view();
  view->Show();

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() == 1);

  view->Hide();

  EXPECT_FALSE(view->GetVisible());
}

TEST_F(DropTargetViewTest, ViewIsClosedAfterDelay) {
  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));

  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));

  view->Show();

  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(15));

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() > 0);
  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() < 1);
  EXPECT_TRUE(view->GetVisible());

  view->Hide();

  animation.Step(now + base::Seconds(kDelayedAnimationDuration + 1));

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() == 0);
  EXPECT_FALSE(view->GetVisible());
}

TEST_F(DropTargetViewTest, ViewIsOpenedAfterDelay) {
  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));

  view->Show();

  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));

  view->Hide();

  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(15));

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() > 0);
  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() < 1);
  EXPECT_TRUE(view->GetVisible());

  view->Show();

  animation.Step(now + base::Seconds(kDelayedAnimationDuration + 1));

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() == 1);
  EXPECT_TRUE(view->GetVisible());
}

}  // namespace
