// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ttc/pointer_mark.h"

#include <memory>

#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"

namespace ttc {
namespace {

class PointerMarkTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    parent_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                               views::Widget::InitParams::TYPE_WINDOW);
    parent_->SetBounds(gfx::Rect(0, 0, 400, 300));
    parent_->Show();
  }

  void TearDown() override {
    parent_.reset();
    ChromeViewsTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> parent_;
};

TEST_F(PointerMarkTest, ShowsAroundThePointWithoutTakingFocus) {
  gfx::ScopedAnimationDurationScaleMode animate(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  std::unique_ptr<views::Widget> mark =
      ShowPointerMark(parent_->GetNativeView(), gfx::Point(200, 150));
  // Still showing: the fade has only just started.
  EXPECT_TRUE(mark->IsVisible());
  EXPECT_EQ(mark->GetWindowBoundsInScreen().CenterPoint(),
            gfx::Point(200, 150));
  EXPECT_FALSE(mark->CanActivate());
  EXPECT_FALSE(mark->IsActive());
}

TEST_F(PointerMarkTest, HidesOnceFadedOut) {
  gfx::ScopedAnimationDurationScaleMode skip_animations(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  std::unique_ptr<views::Widget> mark =
      ShowPointerMark(parent_->GetNativeView(), gfx::Point(200, 150));
  EXPECT_FALSE(mark->IsVisible());
}

}  // namespace
}  // namespace ttc
