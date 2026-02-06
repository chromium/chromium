// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/dragging/test/mock_tab_drag_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/common/drop_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr gfx::Size kMultiContentsViewSize(500, 500);
constexpr gfx::Point kDragPointForStartDropTargetShow(1, 250);
constexpr gfx::Point kDragPointForEndDropTargetShow(499, 250);
constexpr gfx::Point kDragPointForHiddenTargets(250, 250);
// Increase to cover the increased delay for subsequent drags.
constexpr base::TimeDelta kHideDropTargetDelay = base::Milliseconds(100);
constexpr base::TimeDelta kHideDropTargetAnimation = base::Milliseconds(450);

constexpr char kNudgeUsedUserActionName[] = "Tabs.SplitView.NudgeUsed";
constexpr char kNudgeShownUserActionName[] = "Tabs.SplitView.NudgeShown";

content::DropData ValidUrlDropData() {
  content::DropData valid_url_data;
  valid_url_data.url_infos = {
      ui::ClipboardUrlInfo(GURL("https://mail.google.com"), u"")};
  return valid_url_data;
}

void SetRTL(bool rtl) {
  // Override the current locale/direction.
  base::i18n::SetICUDefaultLocale(rtl ? "he" : "en");
  ASSERT_EQ(rtl, base::i18n::IsRTL());
}

class MockDropDelegate
    : public MultiContentsViewDropTargetController::DropDelegate {
 public:
  MOCK_METHOD(void,
              HandleLinkDrop,
              (MultiContentsDropTargetView::DropSide,
               const ui::DropTargetEvent&),
              (override));
  MOCK_METHOD(void,
              HandleTabDrop,
              (MultiContentsDropTargetView::DropSide,
               TabDragTarget::DragController&),
              (override));
  MOCK_METHOD(void, OnDragEntered, (const ui::DropTargetEvent& event));
};

class MockTabDragController : public TabDragTarget::DragController {
 public:
  MOCK_METHOD(std::unique_ptr<tabs::TabModel>,
              DetachTabAtForInsertion,
              (int drag_idx),
              (override));
  MOCK_METHOD(const DragSessionData&, GetSessionData, (), (const, override));
  MOCK_METHOD(const TabDragContext*, GetAttachedContext, (), (const, override));
};

class MockTabSlotView : public TabSlotView {
 public:
  // TabSlotView's pure virtual methods:
  MOCK_METHOD(ViewType, GetTabSlotViewType, (), (const, override));
  MOCK_METHOD(TabSizeInfo, GetTabSizeInfo, (), (const, override));
};

class MultiContentsViewDropTargetControllerTest : public ChromeViewsTestBase {
 public:
  MultiContentsViewDropTargetControllerTest() = default;
  ~MultiContentsViewDropTargetControllerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    SetRTL(false);
    multi_contents_view_ = std::make_unique<views::View>();
    drop_target_view_ = multi_contents_view_->AddChildView(
        std::make_unique<MultiContentsDropTargetView>());
    drop_target_view_->SetVisible(false);
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterIntegerPref(
        prefs::kSplitViewDragAndDropNudgeShownCount, 0);
    prefs_->registry()->RegisterIntegerPref(
        prefs::kSplitViewDragAndDropNudgeUsedCount, 0);
    controller_ = std::make_unique<MultiContentsViewDropTargetController>(
        *drop_target_view_, drop_delegate_, prefs());
    multi_contents_view_->SetSize(kMultiContentsViewSize);

    session_data_.tab_drag_data_ = {
        TabDragData(&tab_drag_context_, &tab_slot_view_),
    };
    session_data_.tab_drag_data_[0].attached_view = &tab_slot_view_;
    EXPECT_CALL(tab_slot_view_, GetTabSlotViewType)
        .WillRepeatedly(testing::Return(TabSlotView::ViewType::kTab));
    EXPECT_CALL(mock_tab_drag_controller_, GetSessionData)
        .WillRepeatedly(testing::ReturnRef(session_data_));
  }

  void TearDown() override {
    controller_.reset();
    drop_target_view_ = nullptr;
    multi_contents_view_ = nullptr;
    ChromeViewsTestBase::TearDown();
  }

  void ResetController() {
    controller_.reset();
  }

  MultiContentsViewDropTargetController& controller() { return *controller_; }
  MultiContentsDropTargetView& drop_target_view() { return *drop_target_view_; }
  PrefService* prefs() { return prefs_.get(); }

  // Fast forwards by an arbitrary time to ensure timed events are executed.
  void FastForward(base::TimeDelta amount) {
    task_environment()->FastForwardBy(amount);
  }

  void DragURLTo(const gfx::Point& point) {
    controller().OnWebContentsDragUpdate(ValidUrlDropData(), point, false);
  }

  void DragTabTo(const gfx::Point& point) {
    controller().OnTabDragUpdated(mock_tab_drag_controller_, point);
  }

  void DropLink() {
    ASSERT_TRUE(drop_target_view().GetVisible());
    const GURL url("https://www.google.com");
    ui::OSExchangeData drop_data;
    drop_data.SetURL(url, u"Google");
    const ui::DropTargetEvent drop_event(
        drop_data, gfx::PointF(), gfx::PointF(), ui::DragDropTypes::DRAG_LINK);
    EXPECT_CALL(drop_delegate(),
                HandleLinkDrop(MultiContentsDropTargetView::DropSide::START,
                               testing::_));
    views::View::DropCallback callback =
        controller().GetDropCallback(drop_event);
    ui::mojom::DragOperation output_op = ui::mojom::DragOperation::kNone;
    std::unique_ptr<ui::LayerTreeOwner> drag_image;
    std::move(callback).Run(drop_event, output_op, std::move(drag_image));
  }

  // Tests that the nudge is shown a limited amount of times. If start_new_drag
  // is true, starts a new drag after each nudge, otherwise just drags the link
  // back to the center of the screen.
  void TestNudgeShownLimit(bool start_new_drag) {
    auto reset_nudge = [&]() {
      if (start_new_drag) {
        controller().OnWebContentsDragEnded();
      } else {
        DragURLTo(kDragPointForHiddenTargets);
      }
    };

    base::UserActionTester user_action_tester;
    ASSERT_EQ(0, user_action_tester.GetActionCount(kNudgeShownUserActionName));
    ASSERT_EQ(0,
              prefs()->GetInteger(prefs::kSplitViewDragAndDropNudgeShownCount));

    // Show the nudge the first kNudgeShownLimit times.
    for (int expected_count = 1;
         expected_count <=
         MultiContentsViewDropTargetController::kNudgeShownLimit;
         ++expected_count) {
      DragURLTo(kDragPointForStartDropTargetShow);
      FastForward(MultiContentsViewDropTargetController::
                      kShowDropTargetForLinkAfterHideDelay);
      EXPECT_TRUE(drop_target_view().GetVisible());
      EXPECT_EQ(drop_target_view().state().value(),
                MultiContentsDropTargetView::DropTargetState::kNudge);
      EXPECT_EQ(expected_count,
                user_action_tester.GetActionCount(kNudgeShownUserActionName));
      EXPECT_EQ(
          expected_count,
          prefs()->GetInteger(prefs::kSplitViewDragAndDropNudgeShownCount));

      reset_nudge();
      FastForward(MultiContentsViewDropTargetController::
                      kShowDropTargetForLinkAfterHideDelay);
      EXPECT_FALSE(drop_target_view().GetVisible());
    }

    // Afterwards, the nudge should not be shown.
    DragURLTo(kDragPointForStartDropTargetShow);
    FastForward(MultiContentsViewDropTargetController::
                    kShowDropTargetForLinkAfterHideDelay);
    EXPECT_TRUE(drop_target_view().GetVisible());
    EXPECT_EQ(drop_target_view().state().value(),
              MultiContentsDropTargetView::DropTargetState::kFull);
    EXPECT_EQ(MultiContentsViewDropTargetController::kNudgeShownLimit,
              user_action_tester.GetActionCount(kNudgeShownUserActionName));
    EXPECT_EQ(MultiContentsViewDropTargetController::kNudgeShownLimit,
              prefs()->GetInteger(prefs::kSplitViewDragAndDropNudgeShownCount));

    reset_nudge();
    FastForward(MultiContentsViewDropTargetController::
                    kShowDropTargetForLinkAfterHideDelay);
    EXPECT_FALSE(drop_target_view().GetVisible());
  }

  MockDropDelegate& drop_delegate() { return drop_delegate_; }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  MockDropDelegate drop_delegate_;
  MockTabDragController mock_tab_drag_controller_;
  DragSessionData session_data_;
  MockTabSlotView tab_slot_view_;
  MockTabDragContext tab_drag_context_;
  std::unique_ptr<MultiContentsViewDropTargetController> controller_;
  std::unique_ptr<views::View> multi_contents_view_;
  raw_ptr<MultiContentsDropTargetView> drop_target_view_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

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
  DragTabTo(kDragPointForStartDropTargetShow);
  FastForward(
      MultiContentsViewDropTargetController::kShowDropTargetForTabDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Dragging multiple tabs should immediately hide it.
  EXPECT_CALL(mock_tab_drag_controller, GetSessionData)
      .WillRepeatedly(testing::ReturnRef(session_data));
  controller().OnTabDragUpdated(mock_tab_drag_controller,
                                kDragPointForStartDropTargetShow);
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is hidden when dragging a group.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnTabDragUpdated_HidesTargetWhenDraggingGroup) {
  MockTabDragController mock_tab_drag_controller;
  DragSessionData session_data;
  MockTabSlotView tab1;
  MockTabDragContext tab_drag_context;
  session_data.tab_drag_data_ = {
      TabDragData(&tab_drag_context, &tab1),
  };
  session_data.tab_drag_data_[0].attached_view = &tab1;
  session_data.dragging_groups.insert(tab_groups::TabGroupId::GenerateNew());
  EXPECT_CALL(tab1, GetTabSlotViewType)
      .WillRepeatedly(testing::Return(TabSlotView::ViewType::kTab));

  // Simulate showing the drop target first.
  DragTabTo(kDragPointForStartDropTargetShow);
  FastForward(
      MultiContentsViewDropTargetController::kShowDropTargetForTabDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Dragging the group tabs should immediately hide it.
  EXPECT_CALL(mock_tab_drag_controller, GetSessionData)
      .WillRepeatedly(testing::ReturnRef(session_data));
  controller().OnTabDragUpdated(mock_tab_drag_controller,
                                kDragPointForStartDropTargetShow);
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
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

  controller().OnTabDragUpdated(mock_tab_drag_controller,
                                kDragPointForStartDropTargetShow);
  FastForward(
      MultiContentsViewDropTargetController::kShowDropTargetForTabDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);

  // Move the drag back to the center to hide the drop target.
  controller().OnTabDragUpdated(mock_tab_drag_controller,
                                kDragPointForHiddenTargets);
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is hidden when the drag exits the view.
TEST_F(MultiContentsViewDropTargetControllerTest, OnTabDragExited) {
  // First, show the drop target.
  DragTabTo(kDragPointForStartDropTargetShow);
  FastForward(
      MultiContentsViewDropTargetController::kShowDropTargetForTabDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Exiting the drag should hide it.
  controller().OnTabDragExited(gfx::Point());
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is hidden when the drag ends.
TEST_F(MultiContentsViewDropTargetControllerTest, OnTabDragEnded) {
  // First, show the drop target.
  DragTabTo(kDragPointForStartDropTargetShow);
  FastForward(
      MultiContentsViewDropTargetController::kShowDropTargetForTabDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Ending the drag should hide it.
  controller().OnTabDragEnded();
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
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

  controller().OnTabDragUpdated(mock_tab_drag_controller,
                                kDragPointForStartDropTargetShow);
  FastForward(
      MultiContentsViewDropTargetController::kShowDropTargetForTabDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  controller().OnTabDragUpdated(mock_tab_drag_controller,
                                kDragPointForHiddenTargets);
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that CanDropTab returns true only when the drop target is visible.
TEST_F(MultiContentsViewDropTargetControllerTest, CanDropTab) {
  // Target is initially not visible.
  EXPECT_FALSE(controller().CanDropTab());

  // Show the drop target by simulating a link drag.
  DragURLTo(kDragPointForEndDropTargetShow);
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
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
  ResetController();
  EXPECT_TRUE(callback_fired);
}

TEST_F(MultiContentsViewDropTargetControllerTest, ShowAndHideNudge) {
  // Drag to the start of the screen should show the nudge on the start side.
  DragURLTo(kDragPointForStartDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudge);

  // Dragging within 40% of the edge should not hide the target.
  DragURLTo(gfx::Point(kMultiContentsViewSize.width() * 0.39f,
                       kMultiContentsViewSize.height()));
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudge);

  // Drag to the end of the screen should show the nudge on the end side.
  DragURLTo(kDragPointForEndDropTargetShow);
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::END);
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudge);
}

// Tests that the nudge is shown a limited amount of times.
TEST_F(MultiContentsViewDropTargetControllerTest, NudgeShownLimit) {
  TestNudgeShownLimit(true);
}

// Tests that multiple nudges within the same drag count independently towards
// the limit.
TEST_F(MultiContentsViewDropTargetControllerTest, NudgeShownLimitSingleDrag) {
  TestNudgeShownLimit(false);
}

// Tests that if the nudge is no longer shown after using the drop zone a
// certain number of times.
TEST_F(MultiContentsViewDropTargetControllerTest, NudgeUsedLimit) {
  base::UserActionTester user_action_tester;
  ASSERT_EQ(0, user_action_tester.GetActionCount(kNudgeUsedUserActionName));
  ASSERT_EQ(0, prefs()->GetInteger(prefs::kSplitViewDragAndDropNudgeUsedCount));

  // The first kNudgeUsedLimit drags should show the nudge.
  for (int expected_count = 1;
       expected_count <= MultiContentsViewDropTargetController::kNudgeUsedLimit;
       ++expected_count) {
    DragURLTo(kDragPointForStartDropTargetShow);
    FastForward(MultiContentsViewDropTargetController::
                    kShowDropTargetForLinkAfterHideDelay);
    EXPECT_TRUE(drop_target_view().GetVisible());
    EXPECT_EQ(drop_target_view().state().value(),
              MultiContentsDropTargetView::DropTargetState::kNudge);

    const ui::DropTargetEvent event(ui::OSExchangeData(), gfx::PointF(),
                                    gfx::PointF(),
                                    ui::DragDropTypes::DRAG_LINK);
    controller().OnDragEntered(event);
    EXPECT_EQ(drop_target_view().state().value(),
              MultiContentsDropTargetView::DropTargetState::kNudgeToFull);

    DropLink();
    FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
    EXPECT_FALSE(drop_target_view().GetVisible());
    EXPECT_EQ(expected_count,
              user_action_tester.GetActionCount(kNudgeUsedUserActionName));
    EXPECT_EQ(expected_count,
              prefs()->GetInteger(prefs::kSplitViewDragAndDropNudgeUsedCount));
  }

  // Afterwards, the nudge should not be shown during a drag.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kFull);

  const ui::DropTargetEvent event(ui::OSExchangeData(), gfx::PointF(),
                                  gfx::PointF(), ui::DragDropTypes::DRAG_LINK);
  controller().OnDragEntered(event);
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kFull);

  DropLink();
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
  EXPECT_FALSE(drop_target_view().GetVisible());
  EXPECT_EQ(MultiContentsViewDropTargetController::kNudgeUsedLimit,
            user_action_tester.GetActionCount(kNudgeUsedUserActionName));
  EXPECT_EQ(MultiContentsViewDropTargetController::kNudgeUsedLimit,
            prefs()->GetInteger(prefs::kSplitViewDragAndDropNudgeUsedCount));
}

TEST_F(MultiContentsViewDropTargetControllerTest, ShowAndHideNudgeRTL) {
  SetRTL(true);

  // Drag to the start of the screen should show the nudge on the end side.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::END);
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudge);

  // Dragging within 40% of the edge should not hide the target.
  DragURLTo(gfx::Point(kMultiContentsViewSize.width() * 0.39f,
                       kMultiContentsViewSize.height()));
  EXPECT_FALSE(drop_target_view().IsClosing());
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudge);

  // Drag to the end of the screen should show the nudge on the start side.
  DragURLTo(kDragPointForEndDropTargetShow);
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudge);
}

TEST_F(MultiContentsViewDropTargetControllerTest, NudgeToFull) {
  // Drag to the start of the screen should show the nudge on the start side.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudge);

  // Fire the drag entered event to expand the nudge.
  const ui::DropTargetEvent event(ui::OSExchangeData(), gfx::PointF(),
                                  gfx::PointF(), ui::DragDropTypes::DRAG_LINK);
  controller().OnDragEntered(event);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudgeToFull);
}

TEST_F(MultiContentsViewDropTargetControllerTest, NudgeToFullToHidden) {
  // Drag to the start of the screen should show the nudge on the start side.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Fire the drag entered event to expand the nudge.
  const ui::DropTargetEvent event(ui::OSExchangeData(), gfx::PointF(),
                                  gfx::PointF(), ui::DragDropTypes::DRAG_LINK);
  controller().OnDragEntered(event);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudgeToFull);

  // Exiting the drag should not hide the expanded nudge.
  controller().OnDragExited();
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_FALSE(drop_target_view().IsClosing());

  // Dragging to within 40% of screen should not hide it.
  DragURLTo(gfx::Point(kMultiContentsViewSize.width() * 0.39f,
                       kMultiContentsViewSize.height()));
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_FALSE(drop_target_view().IsClosing());
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

  controller().OnTabDragUpdated(mock_tab_drag_controller,
                                kDragPointForEndDropTargetShow);
  FastForward(
      MultiContentsViewDropTargetController::kShowDropTargetForTabDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::END);

  // Expect the delegate's HandleTabDrop to be called with the END side.
  EXPECT_CALL(
      drop_delegate(),
      HandleTabDrop(MultiContentsDropTargetView::DropSide::END, testing::_));
  controller().HandleTabDrop(mock_tab_drag_controller);
}

TEST_F(MultiContentsViewDropTargetControllerTest, DragDelegateMethods) {
  // GetDropFormats
  int formats = 0;
  std::set<ui::ClipboardFormatType> format_types;
  EXPECT_TRUE(controller().GetDropFormats(&formats, &format_types));
  EXPECT_EQ(ui::OSExchangeData::URL, formats);

  // CanDrop
  ui::OSExchangeData data;
  data.SetURL(GURL("https://www.google.com"), u"Google");
  EXPECT_TRUE(controller().CanDrop(data));
  ui::OSExchangeData non_url_data;
  non_url_data.SetString(u"Some random string");
  EXPECT_FALSE(controller().CanDrop(non_url_data));
  ui::OSExchangeData empty_url_data;
  EXPECT_FALSE(controller().CanDrop(empty_url_data));

  // OnDragUpdated
  const ui::DropTargetEvent event(ui::OSExchangeData(), gfx::PointF(),
                                  gfx::PointF(), ui::DragDropTypes::DRAG_LINK);
  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK, controller().OnDragUpdated(event));

  // OnDragExited
  drop_target_view().animation_for_testing().SetSlideDuration(base::Seconds(0));
  drop_target_view().Show(MultiContentsDropTargetView::DropSide::START,
                          MultiContentsDropTargetView::DropTargetState::kFull,
                          MultiContentsDropTargetView::DragType::kLink);
  ASSERT_TRUE(drop_target_view().GetVisible());
  controller().OnDragExited();
  EXPECT_FALSE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().animation_for_testing().GetCurrentValue(), 0);

  // OnDragDone
  drop_target_view().Show(MultiContentsDropTargetView::DropSide::START,
                          MultiContentsDropTargetView::DropTargetState::kFull,
                          MultiContentsDropTargetView::DragType::kLink);
  ASSERT_TRUE(drop_target_view().GetVisible());
  controller().OnDragDone();
  EXPECT_FALSE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().animation_for_testing().GetCurrentValue(), 0);

  // GetDropCallback and DoDrop
  drop_target_view().Show(MultiContentsDropTargetView::DropSide::START,
                          MultiContentsDropTargetView::DropTargetState::kFull,
                          MultiContentsDropTargetView::DragType::kLink);
  DropLink();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

TEST_F(MultiContentsViewDropTargetControllerTest,
       ShowsFullDropTargetWhenAnimationsDisabled) {
  auto animation_mode_reset = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
  ASSERT_FALSE(drop_target_view().ShouldShowAnimation());
  ASSERT_FALSE(drop_target_view().GetVisible());

  // Drag to the start of the screen.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);

  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kFull);
}

TEST_F(MultiContentsViewDropTargetControllerTest,
       FullToNudgeTransitionNotAllowed) {
  // Drag to the start of the screen should show the nudge on the start side.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudge);

  // Fire the drag entered event to expand the nudge.
  const ui::DropTargetEvent event(ui::OSExchangeData(), gfx::PointF(),
                                  gfx::PointF(), ui::DragDropTypes::DRAG_LINK);
  controller().OnDragEntered(event);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudgeToFull);

  // Dragging to the nudge area should not transition back to nudge.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudgeToFull);
}

// Tests that the drag type is correctly set to `DragType::kLink` when
// dragging a link.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_SetsDragTypeToLink) {
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().drag_type().has_value());
  EXPECT_EQ(drop_target_view().drag_type().value(),
            MultiContentsDropTargetView::DragType::kLink);
}

// Tests that the drag type is correctly set to `DragType::kTab` when
// dragging a tab.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnTabDragUpdated_SetsDragTypeToTab) {
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

  controller().OnTabDragUpdated(mock_tab_drag_controller,
                                kDragPointForStartDropTargetShow);
  FastForward(MultiContentsViewDropTargetController::
                  kShowDropTargetForLinkAfterHideDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().drag_type().has_value());
  EXPECT_EQ(drop_target_view().drag_type().value(),
            MultiContentsDropTargetView::DragType::kTab);
}

}  // namespace
