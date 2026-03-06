// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_drag_scroll_handler.h"

#include <memory>

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

class ProjectsPanelTabGroupsDragScrollHandlerTest : public ChromeViewsTestBase {
 public:
  ProjectsPanelTabGroupsDragScrollHandlerTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 100, 100);
    widget_ = CreateTestWidget(std::move(params));

    auto scroll_view = std::make_unique<views::ScrollView>();
    scroll_view->SetPreferredSize(gfx::Size(100, 100));
    scroll_view->ClipHeightTo(100, 100);

    // Make the contents view larger than the scroll view.
    auto contents = std::make_unique<views::View>();
    contents->SetPreferredSize(gfx::Size(100, 200));
    contents_ = scroll_view->SetContents(std::move(contents));

    scroll_view_ = widget_->SetContentsView(std::move(scroll_view));
    scroll_view_->SetBounds(0, 0, 100, 100);

    widget_->Show();
  }

  void TearDown() override {
    scroll_view_ = nullptr;
    contents_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  raw_ptr<views::View> contents_ = nullptr;
  ProjectsPanelTabGroupsDragScrollHandler handler_;
};

// TODO(crbug.com/490441528): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NoScrollInMiddle DISABLED_NoScrollInMiddle
#define MAYBE_ScrollsDownNearBottom DISABLED_ScrollsDownNearBottom
#define MAYBE_ScrollsUpNearTop DISABLED_ScrollsUpNearTop
#define MAYBE_StopScrollingAtBottom DISABLED_StopScrollingAtBottom
#define MAYBE_StopScrollingAtTop DISABLED_StopScrollingAtTop
#define MAYBE_StopScrollingExplicitly DISABLED_StopScrollingExplicitly
#define MAYBE_StopScrollingWhenMovingToMiddle \
  DISABLED_StopScrollingWhenMovingToMiddle
#else
#define MAYBE_NoScrollInMiddle NoScrollInMiddle
#define MAYBE_ScrollsDownNearBottom ScrollsDownNearBottom
#define MAYBE_ScrollsUpNearTop ScrollsUpNearTop
#define MAYBE_StopScrollingAtBottom StopScrollingAtBottom
#define MAYBE_StopScrollingAtTop StopScrollingAtTop
#define MAYBE_StopScrollingExplicitly StopScrollingExplicitly
#define MAYBE_StopScrollingWhenMovingToMiddle StopScrollingWhenMovingToMiddle
#endif

TEST_F(ProjectsPanelTabGroupsDragScrollHandlerTest, MAYBE_NoScrollInMiddle) {
  ASSERT_EQ(100, scroll_view_->height());
  ASSERT_EQ(200, contents_->height());

  // Drag in the middle (50, 50), which is outside the 16px boundary.
  handler_.OnDraggedTabGroupPositionUpdated(*scroll_view_, gfx::Point(50, 50));

  // Fast forward and check that no scrolling happened.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(0, scroll_view_->CurrentOffset().y());
}

TEST_F(ProjectsPanelTabGroupsDragScrollHandlerTest,
       MAYBE_ScrollsDownNearBottom) {
  // Drag near the bottom (50, 90), within the 16px boundary (100 - 16 = 84).
  handler_.OnDraggedTabGroupPositionUpdated(*scroll_view_, gfx::Point(50, 90));

  // Fast forward 20ms (should scroll twice, 5px each time).
  task_environment()->FastForwardBy(base::Milliseconds(25));
  EXPECT_EQ(10, scroll_view_->CurrentOffset().y());

  // Fast forward more to ensure scrolling continues.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_GT(scroll_view_->CurrentOffset().y(), 10);
}

TEST_F(ProjectsPanelTabGroupsDragScrollHandlerTest, MAYBE_ScrollsUpNearTop) {
  // First scroll down manually.
  scroll_view_->ScrollToOffset(gfx::PointF(0, 50));
  EXPECT_EQ(50, scroll_view_->CurrentOffset().y());

  // Drag near the top (50, 10), within the 16px boundary.
  handler_.OnDraggedTabGroupPositionUpdated(*scroll_view_, gfx::Point(50, 10));

  // Fast forward 20ms.
  task_environment()->FastForwardBy(base::Milliseconds(25));
  EXPECT_EQ(40, scroll_view_->CurrentOffset().y());
}

TEST_F(ProjectsPanelTabGroupsDragScrollHandlerTest,
       MAYBE_StopScrollingAtBottom) {
  // Drag near the bottom.
  handler_.OnDraggedTabGroupPositionUpdated(*scroll_view_, gfx::Point(50, 95));

  // Fast forward until it hits the bottom (contents height 200, viewport 100,
  // so max scroll 100). 5px every 10ms -> 100px in 200ms.
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(100, scroll_view_->CurrentOffset().y());
}

TEST_F(ProjectsPanelTabGroupsDragScrollHandlerTest, MAYBE_StopScrollingAtTop) {
  // Scroll down first.
  scroll_view_->ScrollToOffset(gfx::PointF(0, 20));

  // Drag near the top.
  handler_.OnDraggedTabGroupPositionUpdated(*scroll_view_, gfx::Point(50, 5));

  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(0, scroll_view_->CurrentOffset().y());
}

TEST_F(ProjectsPanelTabGroupsDragScrollHandlerTest,
       MAYBE_StopScrollingExplicitly) {
  handler_.OnDraggedTabGroupPositionUpdated(*scroll_view_, gfx::Point(50, 90));
  task_environment()->FastForwardBy(base::Milliseconds(15));
  EXPECT_EQ(5, scroll_view_->CurrentOffset().y());

  handler_.StopScrolling();
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(5, scroll_view_->CurrentOffset().y());
}

TEST_F(ProjectsPanelTabGroupsDragScrollHandlerTest,
       MAYBE_StopScrollingWhenMovingToMiddle) {
  handler_.OnDraggedTabGroupPositionUpdated(*scroll_view_, gfx::Point(50, 90));
  task_environment()->FastForwardBy(base::Milliseconds(15));
  EXPECT_EQ(5, scroll_view_->CurrentOffset().y());

  // Move back to middle.
  handler_.OnDraggedTabGroupPositionUpdated(*scroll_view_, gfx::Point(50, 50));
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(5, scroll_view_->CurrentOffset().y());
}
