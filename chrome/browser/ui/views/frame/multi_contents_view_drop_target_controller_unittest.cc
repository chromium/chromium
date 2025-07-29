// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/dragging/test/mock_tab_drag_context.h"
#include "content/public/common/drop_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr gfx::Size kMultiContentsViewSize(500, 500);

constexpr gfx::PointF kDragPointForStartDropTargetShow(1, 250);
constexpr gfx::PointF kDragPointForEndDropTargetShow(499, 250);
constexpr gfx::PointF kDragPointForHiddenTargets(250, 250);

constexpr base::TimeDelta kShowTargetDelay = base::Milliseconds(1000);

content::DropData ValidUrlDropData() {
  content::DropData valid_url_data;
  valid_url_data.url = GURL("https://mail.google.com");
  return valid_url_data;
}

void SetRTL(bool rtl) {
  // Override the current locale/direction.
  base::i18n::SetICUDefaultLocale(rtl ? "he" : "en");
  ASSERT_EQ(rtl, base::i18n::IsRTL());
}

class MockDropDelegate : public MultiContentsDropTargetView::DropDelegate {
 public:
  MOCK_METHOD(void,
              HandleLinkDrop,
              (MultiContentsDropTargetView::DropSide, const std::vector<GURL>&),
              (override));
  MOCK_METHOD(void,
              HandleTabDrop,
              (MultiContentsDropTargetView::DropSide,
               TabDragDelegate::DragController&),
              (override));
};

class MockTabDragController : public TabDragDelegate::DragController {
 public:
  MOCK_METHOD(std::unique_ptr<tabs::TabModel>,
              DetachTabAtForInsertion,
              (int drag_idx),
              (override));
  MOCK_METHOD(const DragSessionData&, GetSessionData, (), (const, override));
};

class MockTabSlotView : public TabSlotView {
 public:
  // TabSlotView's pure virtual methods:
  MOCK_METHOD(ViewType, GetTabSlotViewType, (), (const, override));
  MOCK_METHOD(TabSizeInfo, GetTabSizeInfo, (), (const, override));
};

class MultiContentsViewDropTargetControllerTest : public testing::Test {
 public:
  MultiContentsViewDropTargetControllerTest() = default;
  ~MultiContentsViewDropTargetControllerTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kSideBySide,
          {{features::kSideBySideShowDropTargetDelay.name,
            base::NumberToString(kShowTargetDelay.InMilliseconds()) + "ms"}}}},
        {});
    SetRTL(false);
    multi_contents_view_ = std::make_unique<views::View>();
    drop_target_view_ = multi_contents_view_->AddChildView(
        std::make_unique<MultiContentsDropTargetView>(drop_delegate_));

    drop_target_view_->SetVisible(false);
    controller_ = std::make_unique<MultiContentsViewDropTargetController>(
        *drop_target_view_);

    multi_contents_view_->SetSize(kMultiContentsViewSize);
  }

  void TearDown() override {
    controller_.reset();
    drop_target_view_ = nullptr;
    multi_contents_view_.reset();
  }

  MultiContentsViewDropTargetController& controller() { return *controller_; }

  MultiContentsDropTargetView& drop_target_view() { return *drop_target_view_; }

  // Fast forwards by an arbitrary time to ensure timed events are executed.
  void FastForward(double progress = 1.0) {
    task_environment_.FastForwardBy(kShowTargetDelay * progress);
  }

  void DragURLTo(const gfx::PointF& point) {
    controller().OnWebContentsDragUpdate(ValidUrlDropData(), point, false);
  }

  MockDropDelegate& drop_delegate() { return drop_delegate_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  MockDropDelegate drop_delegate_;
  std::unique_ptr<MultiContentsViewDropTargetController> controller_;
  std::unique_ptr<views::View> multi_contents_view_;
  raw_ptr<MultiContentsDropTargetView> drop_target_view_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that the start drop target is shown when a drag reaches enters the
// "drop area" and a valid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_ShowAndHideStartDropTarget) {
  DragURLTo(kDragPointForStartDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);

  // Move the drag back to the center to hide the drop target.
  DragURLTo(kDragPointForHiddenTargets);
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the end drop target is shown when a drag reaches enters the
// "drop area" and a valid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_ShowEndDropTarget) {
  DragURLTo(kDragPointForEndDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::END);
}

// With RTL enabled, tests that the "end" area's drag coordinates will show
// the "start" drop target.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_ShowStartDropTarget_RTL) {
  SetRTL(true);
  DragURLTo(kDragPointForEndDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);
}

// With RTL enabled, tests that the "start" area's drag coordinates will show
// the "end" drop target.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_ShowEndDropTarget_RTL) {
  SetRTL(true);
  DragURLTo(kDragPointForStartDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::END);
}

// Tests that the drop target is shown even if the timer was started from a drag
// in a different region.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_DragMovedBetweenDropTargets) {
  DragURLTo(kDragPointForEndDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward(0.25);
  EXPECT_FALSE(drop_target_view().GetVisible());

  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(0.25);
  EXPECT_FALSE(drop_target_view().GetVisible());

  // Fast forward to the end of the animtion. The start-side drop target should
  // be shown, even though the timer started with a drag to the end-side.
  FastForward(0.50);

  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);
}

// Tests that the drop target is not shown when an invalid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_HideDropTargetOnInvalidURL) {
  controller().OnWebContentsDragUpdate(content::DropData(),
                                       kDragPointForStartDropTargetShow, false);

  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is not shown when a drag is started from a
// tab that is already in a split view.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_HideDropTargetWhenInSplitView) {
  controller().OnWebContentsDragUpdate(ValidUrlDropData(),
                                       kDragPointForStartDropTargetShow, true);

  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is not shown when a drag is outside of the
// contents view.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_HideDropTargetWhenDragIsOutOfBounds) {
  controller().OnWebContentsDragUpdate(ValidUrlDropData(), gfx::PointF(-1, 250),
                                       false);

  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());

  controller().OnWebContentsDragUpdate(ValidUrlDropData(), gfx::PointF(1000, 250),
                                       false);

  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target timer is cancelled when a drag is not in the
// "drop area".
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_HideDropTargetOnOutOfBounds) {
  DragURLTo(kDragPointForStartDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  controller().OnWebContentsDragUpdate(ValidUrlDropData(),
                                       kDragPointForHiddenTargets, false);
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target timer is cancelled when a drag exits the contents
// view.
TEST_F(MultiContentsViewDropTargetControllerTest, OnWebContentsDragExit) {
  DragURLTo(kDragPointForStartDropTargetShow);

  controller().OnWebContentsDragExit();
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is hidden when the drag ends.
TEST_F(MultiContentsViewDropTargetControllerTest, OnWebContentsDragEnded) {
  // First, show the drop target.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Ending the drag should hide it.
  controller().OnWebContentsDragEnded();
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is hidden when dragging more than one tab.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnTabDragUpdated_HidesTargetWhenDraggingMultipleTabs) {
  MockTabDragController mock_tab_drag_controller;
  DragSessionData session_data;
  MockTabSlotView tab1;
  MockTabSlotView tab2;
  MockTabDragContext tab_drag_context;

  session_data.tab_drag_data_ = {
      TabDragData(&tab_drag_context, &tab1),
      TabDragData(&tab_drag_context, &tab2),
  };
  session_data.tab_drag_data_[0].attached_view = &tab1;
  session_data.tab_drag_data_[1].attached_view = &tab2;

  EXPECT_CALL(tab1, GetTabSlotViewType)
      .WillRepeatedly(testing::Return(TabSlotView::ViewType::kTab));
  EXPECT_CALL(tab2, GetTabSlotViewType)
      .WillRepeatedly(testing::Return(TabSlotView::ViewType::kTab));

  // Simulate showing the drop target first.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Dragging multiple tabs should immediately hide it.
  EXPECT_CALL(mock_tab_drag_controller, GetSessionData)
      .WillRepeatedly(testing::ReturnRef(session_data));
  controller().OnTabDragUpdated(
      mock_tab_drag_controller,
      gfx::ToRoundedPoint(kDragPointForStartDropTargetShow));
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drag updated event is handled correctly for a single tab.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnTabDragUpdated_ShowsAndHidesTargetWhenDraggingSingleTab) {
  MockTabDragController mock_tab_drag_controller;
  DragSessionData session_data;
  MockTabSlotView tab1;
  MockTabDragContext tab_drag_context;
  session_data.tab_drag_data_ = {
      TabDragData(&tab_drag_context, &tab1),
  };
  session_data.tab_drag_data_[0].attached_view = &tab1;

  EXPECT_CALL(tab1, GetTabSlotViewType)
      .WillRepeatedly(testing::Return(TabSlotView::ViewType::kTab));

  EXPECT_CALL(mock_tab_drag_controller, GetSessionData)
      .WillRepeatedly(testing::ReturnRef(session_data));

  controller().OnTabDragUpdated(
      mock_tab_drag_controller,
      gfx::ToRoundedPoint(kDragPointForStartDropTargetShow));
  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);

  // Move the drag back to the center to hide the drop target.
  controller().OnTabDragUpdated(
      mock_tab_drag_controller,
      gfx::ToRoundedPoint(kDragPointForHiddenTargets));
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is hidden when the drag exits the view.
TEST_F(MultiContentsViewDropTargetControllerTest, OnTabDragExited) {
  // First, show the drop target.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Exiting the drag should hide it.
  controller().OnTabDragExited();
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is hidden when the drag ends.
TEST_F(MultiContentsViewDropTargetControllerTest, OnTabDragEnded) {
  // First, show the drop target.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Ending the drag should hide it.
  controller().OnTabDragEnded();
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target timer is cancelled when a tab drag is not in the
// "drop area".
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnTabDragUpdated_HideDropTargetOnOutOfBounds) {
  MockTabDragController mock_tab_drag_controller;
  DragSessionData session_data;
  MockTabSlotView tab1;
  MockTabDragContext tab_drag_context;
  session_data.tab_drag_data_ = {
      TabDragData(&tab_drag_context, &tab1),
  };
  session_data.tab_drag_data_[0].attached_view = &tab1;

  EXPECT_CALL(tab1, GetTabSlotViewType)
      .WillRepeatedly(testing::Return(TabSlotView::ViewType::kTab));

  EXPECT_CALL(mock_tab_drag_controller, GetSessionData)
      .WillRepeatedly(testing::ReturnRef(session_data));

  controller().OnTabDragUpdated(
      mock_tab_drag_controller,
      gfx::ToRoundedPoint(kDragPointForStartDropTargetShow));
  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());

  controller().OnTabDragUpdated(
      mock_tab_drag_controller,
      gfx::ToRoundedPoint(kDragPointForHiddenTargets));
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that CanDropTab returns true only when the drop target is visible.
TEST_F(MultiContentsViewDropTargetControllerTest, CanDropTab) {
  // Target is initially not visible.
  EXPECT_FALSE(controller().CanDropTab());

  // Show the drop target.
  DragURLTo(kDragPointForEndDropTargetShow);
  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Now, CanDropTab should be true.
  EXPECT_TRUE(controller().CanDropTab());
}

// Tests that the destruction callback is fired when the controller is
// destroyed.
TEST_F(MultiContentsViewDropTargetControllerTest, RegisterWillDestroyCallback) {
  bool callback_fired = false;
  auto subscription = controller().RegisterWillDestroyCallback(
      base::BindOnce([](bool* fired) { *fired = true; }, &callback_fired));

  EXPECT_FALSE(callback_fired);

  // Resetting the controller unique_ptr will destroy it.
  TearDown();

  EXPECT_TRUE(callback_fired);
}

TEST_F(MultiContentsViewDropTargetControllerTest, HandleTabDrop) {
  MockTabDragController mock_tab_drag_controller;
  DragSessionData session_data;
  MockTabSlotView tab1;
  MockTabDragContext tab_drag_context;

  // Show the drop target on the END side by simulating a single tab drag.
  session_data.tab_drag_data_ = {
      TabDragData(&tab_drag_context, &tab1),
  };
  session_data.tab_drag_data_[0].attached_view = &tab1;

  EXPECT_CALL(tab1, GetTabSlotViewType)
      .WillRepeatedly(testing::Return(TabSlotView::ViewType::kTab));

  EXPECT_CALL(mock_tab_drag_controller, GetSessionData)
      .WillRepeatedly(testing::ReturnRef(session_data));

  controller().OnTabDragUpdated(
      mock_tab_drag_controller,
      gfx::ToRoundedPoint(kDragPointForEndDropTargetShow));
  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::END);

  // Expect the delegate's HandleTabDrop to be called with the END side.
  EXPECT_CALL(
      drop_delegate(),
      HandleTabDrop(MultiContentsDropTargetView::DropSide::END, testing::_));

  controller().HandleTabDrop(mock_tab_drag_controller);
}

}  // namespace
