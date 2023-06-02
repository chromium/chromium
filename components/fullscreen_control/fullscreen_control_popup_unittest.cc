// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "ui/views/widget/widget.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "components/fullscreen_control/fullscreen_control_popup.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/test/widget_test.h"

class FullscreenControlPopupTest : public views::test::WidgetTest {
 public:
  FullscreenControlPopupTest() {}

  FullscreenControlPopupTest(const FullscreenControlPopupTest&) = delete;
  FullscreenControlPopupTest& operator=(const FullscreenControlPopupTest&) =
      delete;

  ~FullscreenControlPopupTest() override {}

  // views::test::WidgetTest:
  void SetUp() override {
    views::test::WidgetTest::SetUp();
    parent_widget_ = CreateTopLevelNativeWidget();
    parent_widget_->SetBounds(gfx::Rect(100, 100, 640, 480));
    parent_widget_->Show();
    popup_ = std::make_unique<FullscreenControlPopup>(
        parent_widget_->GetNativeView(), base::DoNothing(), base::DoNothing());
    animation_api_ = std::make_unique<gfx::AnimationTestApi>(
        popup_->GetAnimationForTesting());
  }

  void TearDown() override {
    parent_widget_->CloseNow();
    views::test::WidgetTest::TearDown();
  }

 protected:
  void RunAnimationFor(base::TimeDelta duration) {
    base::TimeTicks now = base::TimeTicks::Now();
    animation_api_->SetStartTime(now);
    animation_api_->Step(now + duration);
  }

  void CompleteAnimation() { RunAnimationFor(base::Milliseconds(5000)); }

  gfx::Rect GetParentBounds() const {
    return parent_widget_->GetClientAreaBoundsInScreen();
  }

  gfx::Rect GetPopupBounds() const {
    return popup_->GetPopupWidget()->GetClientAreaBoundsInScreen();
  }

  std::unique_ptr<FullscreenControlPopup> popup_;

 private:
  std::unique_ptr<gfx::AnimationTestApi> animation_api_;
  raw_ptr<views::Widget, DanglingUntriaged> parent_widget_ = nullptr;
};

TEST_F(FullscreenControlPopupTest, ShowPopupAnimated) {
  EXPECT_FALSE(popup_->IsAnimating());
  EXPECT_FALSE(popup_->IsVisible());

  popup_->Show(GetParentBounds());
  EXPECT_TRUE(popup_->IsAnimating());
  EXPECT_TRUE(popup_->IsVisible());

  // The popup should be above the parent bounds when the animation is just
  // started.
  EXPECT_GT(GetParentBounds().y(), GetPopupBounds().y());

  CompleteAnimation();

  EXPECT_FALSE(popup_->IsAnimating());
  EXPECT_TRUE(popup_->IsVisible());
  int final_bottom =
      FullscreenControlPopup::GetButtonBottomOffset() + GetParentBounds().y();
  EXPECT_EQ(final_bottom, GetPopupBounds().bottom());
}

TEST_F(FullscreenControlPopupTest, HidePopupWhileStillShowing) {
  popup_->Show(GetParentBounds());

  RunAnimationFor(base::Milliseconds(50));

  EXPECT_TRUE(popup_->IsAnimating());
  EXPECT_TRUE(popup_->IsVisible());

  // The popup is partially shown.
  EXPECT_LT(GetParentBounds().y(), GetPopupBounds().bottom());

  popup_->Hide(true);
  EXPECT_TRUE(popup_->IsAnimating());
  EXPECT_TRUE(popup_->IsVisible());
  EXPECT_LT(GetParentBounds().y(), GetPopupBounds().bottom());

  CompleteAnimation();

  EXPECT_FALSE(popup_->IsAnimating());
  EXPECT_FALSE(popup_->IsVisible());
  EXPECT_GT(GetParentBounds().y(), GetPopupBounds().y());
}

TEST_F(FullscreenControlPopupTest, HidePopupWithoutAnimation) {
  popup_->Show(GetParentBounds());

  CompleteAnimation();

  popup_->Hide(false);
  EXPECT_FALSE(popup_->IsAnimating());
  EXPECT_FALSE(popup_->IsVisible());
}
