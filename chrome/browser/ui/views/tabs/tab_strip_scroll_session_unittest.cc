// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_scroll_session.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

// Mock class for scroll_manager
class MockTabDragWithScrollManager : public TabDragWithScrollManager {
 public:
  MockTabDragWithScrollManager() = default;
  ~MockTabDragWithScrollManager() override = default;
  MockTabDragWithScrollManager(const MockTabDragWithScrollManager&) = delete;
  MockTabDragWithScrollManager(MockTabDragWithScrollManager&&) = delete;
  MockTabDragWithScrollManager& operator=(const MockTabDragWithScrollManager&) =
      delete;
  MockTabDragWithScrollManager& operator=(MockTabDragWithScrollManager&&) =
      delete;

  MOCK_METHOD(bool, IsDraggingTabState, (), ());
  MOCK_METHOD(void,
              MoveAttached,
              (const gfx::Point& point_in_screen, bool just_attached),
              ());
  MOCK_METHOD(views::ScrollView*, GetScrollView, (), ());
  MOCK_METHOD(gfx::Point, GetLastPointInScreen, (), ());
  MOCK_METHOD(views::View*, GetAttachedContext, (), ());
  MOCK_METHOD(gfx::Rect, GetEnclosingRectForDraggedTabs, (), ());
};

class TabStripScrollSessionWithTimerTestBase : public ChromeViewsTestBase {
 public:
  explicit TabStripScrollSessionWithTimerTestBase(
      TabDragController::ScrollWithDragStrategy strategy)
      : strategy_(strategy) {
    scoped_feature_list_.InitWithFeatures({tabs::kScrollableTabStrip}, {});
  }

  ~TabStripScrollSessionWithTimerTestBase() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    mock_timer_ = new base::MockRepeatingTimer();
    drag_controller_ = std::make_unique<MockTabDragWithScrollManager>();

    if (strategy_ ==
        TabDragController::ScrollWithDragStrategy::kVariableSpeed) {
      scroll_session_ = std::make_unique<TabStripScrollSessionWithTimer>(
          *(drag_controller_.get()),
          TabStripScrollSessionWithTimer::ScrollSessionTimerType::
              kVariableTimer);
    } else {
      scroll_session_ = std::make_unique<TabStripScrollSessionWithTimer>(
          *(drag_controller_.get()),
          TabStripScrollSessionWithTimer::ScrollSessionTimerType::
              kConstantTimer);
    }
    scroll_session_->SetTimerForTesting(mock_timer_);

    scroll_view_ = std::make_unique<views::ScrollView>();
    scroll_view_->SetBounds(0, 0,
                            5 * TabStyle::Get()->GetMinimumInactiveWidth(), 5);

    attached_context_ =
        scroll_view_->SetContents(std::make_unique<views::View>());
    attached_context_->SetBounds(
        0, 0, 10 * TabStyle::Get()->GetMinimumInactiveWidth(), 5);
  }

  void TearDown() override { ChromeViewsTestBase::TearDown(); }

 protected:
  TabDragController::ScrollWithDragStrategy strategy_ =
      TabDragController::ScrollWithDragStrategy::kDisabled;
  std::unique_ptr<MockTabDragWithScrollManager> drag_controller_;
  std::unique_ptr<views::ScrollView> scroll_view_;
  std::unique_ptr<TabStripScrollSessionWithTimer> scroll_session_;
  raw_ptr<views::View, DanglingUntriaged> attached_context_;
  raw_ptr<base::MockRepeatingTimer> mock_timer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class TabStripScrollSessionTestWithConstantSpeed
    : public TabStripScrollSessionWithTimerTestBase {
 public:
  TabStripScrollSessionTestWithConstantSpeed()
      : TabStripScrollSessionWithTimerTestBase(
            TabDragController::ScrollWithDragStrategy::kConstantSpeed) {}
  TabStripScrollSessionTestWithConstantSpeed(
      const TabStripScrollSessionTestWithConstantSpeed&) = delete;
  TabStripScrollSessionTestWithConstantSpeed& operator=(
      const TabStripScrollSessionTestWithConstantSpeed&) = delete;
  ~TabStripScrollSessionTestWithConstantSpeed() override = default;

 private:
};

// When the tab scroll direction is `kNoScroll` then do not start the scroll
// session
TEST_F(TabStripScrollSessionTestWithConstantSpeed,
       GivenNoScrollWhenScrollSessionMaybeStartThenTimerDoesNotRun) {
  // create a dragged_tab_rect outside of the right and left scrollable regions
  const gfx::Rect dragged_tabs_rect = gfx::Rect(
      scroll_view_->GetVisibleRect().top_right().x() -
          2 * scroll_session_->GetScrollableOffsetFromScrollViewForTesting(),
      0, (scroll_session_->GetScrollableOffsetFromScrollViewForTesting() / 2),
      1);

  EXPECT_CALL(*drag_controller_, GetAttachedContext())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(attached_context_));
  EXPECT_CALL(*drag_controller_, GetEnclosingRectForDraggedTabs())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(dragged_tabs_rect));
  EXPECT_CALL(*drag_controller_, GetScrollView())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(scroll_view_.get()));

  // assert
  scroll_session_->MaybeStart();
  EXPECT_FALSE(mock_timer_->IsRunning());
}

// If there is no attached context to the `drag_controller_`, then do not run
// the timer.
TEST_F(TabStripScrollSessionTestWithConstantSpeed,
       GivenNoAttachedContextWhenScrollSessionMaybeStartThenTimerDoesNotRun) {
  // arrange
  EXPECT_CALL(*drag_controller_, GetAttachedContext())
      .Times(1)
      .WillOnce(Return(nullptr));

  // act
  scroll_session_->MaybeStart();

  // assert
  EXPECT_FALSE(mock_timer_->IsRunning());
}

// When scroll session starts with correct arguments, timer callback is invoked
TEST_F(TabStripScrollSessionTestWithConstantSpeed,
       GivenScrollSessionWhenMaybeStartThenTimerCallback) {
  // create a rect that is in the right scrollable region.
  const gfx::Rect dragged_tabs_rect =
      gfx::Rect(scroll_view_->GetVisibleRect().top_right().x(), 0, 1, 1);

  EXPECT_CALL(*drag_controller_, GetAttachedContext())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(attached_context_));

  EXPECT_CALL(*drag_controller_, GetScrollView())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(scroll_view_.get()));

  EXPECT_CALL(*drag_controller_, IsDraggingTabState())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*drag_controller_, GetLastPointInScreen())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(gfx::Point()));

  EXPECT_CALL(*drag_controller_, GetEnclosingRectForDraggedTabs())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(dragged_tabs_rect));

  EXPECT_CALL(*drag_controller_, MoveAttached(_, false)).Times(1);

  // act
  scroll_session_->MaybeStart();

  // make sure timer is running
  EXPECT_TRUE(mock_timer_->IsRunning());

  if (mock_timer_->IsRunning()) {
    mock_timer_->Fire();
  }

  // assert
  EXPECT_TRUE(scroll_session_->IsRunning());
  EXPECT_EQ(ceil(scroll_session_->CalculateBaseScrollOffset()),
            scroll_view_->GetVisibleRect().x());
}

// When scroll is started with one direction but in the callback check,
// the direction calculation is different, stop the timer.
TEST_F(TabStripScrollSessionTestWithConstantSpeed,
       GivenScrollingTowardsRightWhenShouldScrollToLeftThenStopTimer) {
  // arrange
  const gfx::Rect dragged_tabs_rect_for_scrolling_left =
      gfx::Rect(scroll_view_->GetVisibleRect().origin().x(), 0, 1, 1);
  const gfx::Rect dragged_tabs_rect_for_scrolling_right =
      gfx::Rect(scroll_view_->GetVisibleRect().top_right().x(), 0, 1, 1);

  EXPECT_CALL(*drag_controller_, GetAttachedContext())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(attached_context_));
  EXPECT_CALL(*drag_controller_, GetScrollView())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(scroll_view_.get()));
  EXPECT_CALL(*drag_controller_, GetEnclosingRectForDraggedTabs())
      .Times(AtLeast(1))
      .WillOnce(Return(dragged_tabs_rect_for_scrolling_right))
      .WillRepeatedly(Return(dragged_tabs_rect_for_scrolling_left));
  EXPECT_CALL(*drag_controller_, MoveAttached(_, false)).Times(0);

  // act
  scroll_session_->MaybeStart();
  // check if timer is running
  EXPECT_TRUE(mock_timer_->IsRunning());
  mock_timer_->Fire();
  // assert
  EXPECT_FALSE(scroll_session_->IsRunning());
  EXPECT_EQ(scroll_view_->GetVisibleRect().x(), 0);
}

class TabStripScrollSessionTestWithVariableSpeed
    : public TabStripScrollSessionWithTimerTestBase {
 public:
  TabStripScrollSessionTestWithVariableSpeed()
      : TabStripScrollSessionWithTimerTestBase(
            TabDragController::ScrollWithDragStrategy::kVariableSpeed) {}
  TabStripScrollSessionTestWithVariableSpeed(
      const TabStripScrollSessionTestWithVariableSpeed&) = delete;
  TabStripScrollSessionTestWithVariableSpeed& operator=(
      const TabStripScrollSessionTestWithVariableSpeed&) = delete;
  ~TabStripScrollSessionTestWithVariableSpeed() override = default;

 private:
};

// When scroll session starts with correct arguments, timer callback is invoked
TEST_F(TabStripScrollSessionTestWithVariableSpeed,
       GivenScrollSessionWhenMaybeStartThenTimerCallback) {
  // create a rect starting from the scrollable region to half of the end of the
  // attached_context
  const gfx::Rect dragged_tabs_rect = gfx::Rect(
      scroll_view_->GetVisibleRect().top_right().x() -
          scroll_session_->GetScrollableOffsetFromScrollViewForTesting(),
      0, (scroll_session_->GetScrollableOffsetFromScrollViewForTesting() / 2),
      1);

  EXPECT_CALL(*drag_controller_, GetAttachedContext())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(attached_context_));
  EXPECT_CALL(*drag_controller_, GetScrollView())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(scroll_view_.get()));

  EXPECT_CALL(*drag_controller_, IsDraggingTabState())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*drag_controller_, GetEnclosingRectForDraggedTabs())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(dragged_tabs_rect));

  EXPECT_CALL(*drag_controller_, GetLastPointInScreen())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(gfx::Point()));

  EXPECT_CALL(*drag_controller_, MoveAttached(_, false)).Times(1);

  scroll_session_->MaybeStart();
  EXPECT_TRUE(mock_timer_->IsRunning());

  if (mock_timer_->IsRunning()) {
    mock_timer_->Fire();
  }

  EXPECT_TRUE(scroll_session_->IsRunning());
  EXPECT_GE(scroll_view_->GetVisibleRect().x(), 0);
  EXPECT_LE(scroll_view_->GetVisibleRect().x(),
            ceil(scroll_session_->CalculateBaseScrollOffset()));
}
