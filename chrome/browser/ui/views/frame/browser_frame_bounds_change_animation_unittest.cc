// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_bounds_change_animation.h"

#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/gfx/geometry/rect.h"

namespace {

constexpr gfx::Rect kInitialBounds{50, 50, 300, 300};
constexpr gfx::Rect kNewBounds{100, 100, 400, 400};

void ExpectRectBetween(const gfx::Rect& actual_rect,
                       const gfx::Rect& lower_rect,
                       const gfx::Rect& higher_rect) {
  EXPECT_GT(actual_rect.x(), lower_rect.x());
  EXPECT_LT(actual_rect.x(), higher_rect.x());
  EXPECT_GT(actual_rect.y(), lower_rect.y());
  EXPECT_LT(actual_rect.y(), higher_rect.y());
  EXPECT_GT(actual_rect.width(), lower_rect.width());
  EXPECT_LT(actual_rect.width(), higher_rect.width());
  EXPECT_GT(actual_rect.height(), lower_rect.height());
  EXPECT_LT(actual_rect.height(), higher_rect.height());
}

using BrowserFrameBoundsChangeAnimationTest = ChromeViewsTestBase;

TEST_F(BrowserFrameBoundsChangeAnimationTest, AnimatesWidgetMove) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(kInitialBounds);
  widget->Show();
  BrowserFrameBoundsChangeAnimation animation(*widget, kNewBounds);

  ASSERT_EQ(kInitialBounds, widget->GetWindowBoundsInScreen());
  animation.Start();
  task_environment()->FastForwardBy(base::Milliseconds(100));
  ExpectRectBetween(widget->GetWindowBoundsInScreen(), kInitialBounds,
                    kNewBounds);
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(kNewBounds, widget->GetWindowBoundsInScreen());
}

}  // namespace
