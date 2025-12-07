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
constexpr base::TimeDelta kShowDropTargetForLinkDelay =
    base::Milliseconds(1400);
constexpr base::TimeDelta kShowDropTargetForLinkAfterHideDelay =
    base::Milliseconds(3000);
constexpr base::TimeDelta kShowDropTargetForTabDelay = base::Milliseconds(500);
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

content::DropData NonStandardUrlDropData() {
  content::DropData valid_url_data;
  valid_url_data.url_infos = {
      ui::ClipboardUrlInfo(GURL("mailto:me@google.com"), u"")};
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
               TabDragDelegate::DragController&),
              (override));
  MOCK_METHOD(void, OnDragEntered, (const ui::DropTargetEvent& event));
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

class MultiContentsViewDropTargetControllerTestBase
    : public ChromeViewsTestBase {
 public:
  MultiContentsViewDropTargetControllerTestBase() = default;
  ~MultiContentsViewDropTargetControllerTestBase() override = default;

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

    // Show the nudge the first kSideBySideDropTargetNudgeShownLimit times.
    for (int expected_count = 1;
         expected_count <= features::kSideBySideDropTargetNudgeShownLimit.Get();
         ++expected_count) {
      DragURLTo(kDragPointForStartDropTargetShow);
      FastForward(kShowDropTargetForLinkDelay);
      EXPECT_TRUE(drop_target_view().GetVisible());
      EXPECT_EQ(drop_target_view().state().value(),
                MultiContentsDropTargetView::DropTargetState::kNudge);
      EXPECT_EQ(expected_count,
                user_action_tester.GetActionCount(kNudgeShownUserActionName));
      EXPECT_EQ(
          expected_count,
          prefs()->GetInteger(prefs::kSplitViewDragAndDropNudgeShownCount));

      reset_nudge();
      FastForward(kShowDropTargetForLinkDelay);
      EXPECT_FALSE(drop_target_view().GetVisible());
    }

    // Afterwards, the nudge should not be shown.
    DragURLTo(kDragPointForStartDropTargetShow);
    FastForward(kShowDropTargetForLinkDelay);
    EXPECT_TRUE(drop_target_view().GetVisible());
    EXPECT_EQ(drop_target_view().state().value(),
              MultiContentsDropTargetView::DropTargetState::kFull);
    EXPECT_EQ(features::kSideBySideDropTargetNudgeShownLimit.Get(),
              user_action_tester.GetActionCount(kNudgeShownUserActionName));
    EXPECT_EQ(features::kSideBySideDropTargetNudgeShownLimit.Get(),
              prefs()->GetInteger(prefs::kSplitViewDragAndDropNudgeShownCount));

    reset_nudge();
    FastForward(kShowDropTargetForLinkDelay);
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

// Tests link-dragging behaviour while the "nudge" feature is disabled.
class MultiContentsViewDropTargetControllerNudgeDisabledTest
    : public MultiContentsViewDropTargetControllerTestBase {
 public:
  MultiContentsViewDropTargetControllerNudgeDisabledTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kSideBySide,
          {{features::kSideBySideShowDropTargetDelay.name,
            base::NumberToString(kShowDropTargetForTabDelay.InMilliseconds()) +
                "ms"},
           {features::kSideBySideShowDropTargetForLinkDelay.name,
            base::NumberToString(kShowDropTargetForLinkDelay.InMilliseconds()) +
                "ms"},
           {features::kSideBySideShowDropTargetForLinkAfterHideDelay.name,
            base::NumberToString(
                kShowDropTargetForLinkAfterHideDelay.InMilliseconds()) +
                "ms"}}}},
        {features::kSideBySideDropTargetNudge});
  }
  ~MultiContentsViewDropTargetControllerNudgeDisabledTest() override = default;
};

// Tests that the start drop target is shown when a drag reaches enters the
// "drop area" and a valid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragUpdate_ShowAndHideStartDropTarget) {
  DragURLTo(kDragPointForStartDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);

  // Move the drag back to the center to hide the drop target.
  DragURLTo(kDragPointForHiddenTargets);
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target takes longer to show if it has been recently
// hidden.
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragUpdate_ShowAfterHideDropTarget) {
  DragURLTo(kDragPointForStartDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Move the drag back to the center to hide the drop target.
  DragURLTo(kDragPointForHiddenTargets);
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
  EXPECT_FALSE(drop_target_view().GetVisible());

  // Now move back over the drop target.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_FALSE(drop_target_view().GetVisible());

  // After waiting the longer delay, it does show.
  FastForward(kShowDropTargetForLinkAfterHideDelay -
              kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
}

// Tests that the end drop target is shown when a drag reaches enters the
// "drop area" and a valid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragUpdate_ShowEndDropTarget) {
  DragURLTo(kDragPointForEndDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::END);
}

// With RTL enabled, tests that the "end" area's drag coordinates will show
// the "start" drop target.
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragUpdate_ShowStartDropTarget_RTL) {
  SetRTL(true);
  DragURLTo(kDragPointForEndDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);
}

// With RTL enabled, tests that the "start" area's drag coordinates will show
// the "end" drop target.
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragUpdate_ShowEndDropTarget_RTL) {
  SetRTL(true);
  DragURLTo(kDragPointForStartDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::END);
}

// Tests that the drop target is shown even if the timer was started from a drag
// in a different region.
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragUpdate_DragMovedBetweenDropTargets) {
  DragURLTo(kDragPointForEndDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward(0.25 * kShowDropTargetForLinkDelay);
  EXPECT_FALSE(drop_target_view().GetVisible());

  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(0.25 * kShowDropTargetForLinkDelay);
  EXPECT_FALSE(drop_target_view().GetVisible());

  // Fast forward to the end of the animtion. The start-side drop target should
  // be shown, even though the timer started with a drag to the end-side.
  FastForward(0.50 * kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);
}

// Tests that the drop target is not shown when an invalid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragUpdate_HideDropTargetOnNonStandardURL) {
  controller().OnWebContentsDragUpdate(NonStandardUrlDropData(),
                                       kDragPointForStartDropTargetShow, false);
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is not shown when a non-standard url is being
// dragged.
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragUpdate_HideDropTargetOnInvalidURL) {
  controller().OnWebContentsDragUpdate(content::DropData(),
                                       kDragPointForStartDropTargetShow, false);
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is not shown when a drag is started from a
// tab that is already in a split view.
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragUpdate_HideDropTargetWhenInSplitView) {
  controller().OnWebContentsDragUpdate(ValidUrlDropData(),
                                       kDragPointForStartDropTargetShow, true);
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is not shown when a drag is outside of the
// contents view.
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragUpdate_HideDropTargetWhenDragIsOutOfBounds) {
  controller().OnWebContentsDragUpdate(ValidUrlDropData(), gfx::Point(-1, 250),
                                       false);
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_FALSE(drop_target_view().GetVisible());

  controller().OnWebContentsDragUpdate(ValidUrlDropData(),
                                       gfx::Point(1000, 250), false);
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target timer is cancelled when a drag is not in the
// "drop area".
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragUpdate_HideDropTargetOnOutOfBounds) {
  DragURLTo(kDragPointForStartDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  controller().OnWebContentsDragUpdate(ValidUrlDropData(),
                                       kDragPointForHiddenTargets, false);
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target timer is cancelled when a drag exits the contents
// view.
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragExit) {
  DragURLTo(kDragPointForStartDropTargetShow);
  controller().OnWebContentsDragExit();
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is hidden when the drag ends.
TEST_F(MultiContentsViewDropTargetControllerNudgeDisabledTest,
       OnWebContentsDragEnded) {
  // First, show the drop target.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Ending the drag should hide it.
  controller().OnWebContentsDragEnded();
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

class MultiContentsViewDropTargetControllerDragTest
    : public MultiContentsViewDropTargetControllerTestBase {
 public:
  MultiContentsViewDropTargetControllerDragTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kSideBySide,
          {{features::kSideBySideShowDropTargetDelay.name,
            base::NumberToString(kShowDropTargetForTabDelay.InMilliseconds()) +
                "ms"},
           {features::kSideBySideShowDropTargetForLinkDelay.name,
            base::NumberToString(kShowDropTargetForLinkDelay.InMilliseconds()) +
                "ms"},
           {features::kSideBySideHideDropTargetDelay.name,
            base::NumberToString(kHideDropTargetDelay.InMilliseconds()) + "ms"},
           {features::kSideBySideShowDropTargetForLinkAfterHideLookbackWindow
                .name,
            "0ms"}}},
         {features::kSideBySideDropTargetNudge, {}}},
        {});
  }
  ~MultiContentsViewDropTargetControllerDragTest() override = default;
};

// Tests that the drop target is hidden when dragging more than one tab.
TEST_F(MultiContentsViewDropTargetControllerDragTest,
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
  FastForward(kShowDropTargetForTabDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Dragging multiple tabs should immediately hide it.
  EXPECT_CALL(mock_tab_drag_controller, GetSessionData)
      .WillRepeatedly(testing::ReturnRef(session_data));
  controller().OnTabDragUpdated(mock_tab_drag_controller,
                                kDragPointForStartDropTargetShow);
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drag updated event is handled correctly for a single tab.
TEST_F(MultiContentsViewDropTargetControllerDragTest,
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
  FastForward(kShowDropTargetForTabDelay);
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
TEST_F(MultiContentsViewDropTargetControllerDragTest, OnTabDragExited) {
  // First, show the drop target.
  DragTabTo(kDragPointForStartDropTargetShow);
  FastForward(kShowDropTargetForTabDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Exiting the drag should hide it.
  controller().OnTabDragExited();
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is hidden when the drag ends.
TEST_F(MultiContentsViewDropTargetControllerDragTest, OnTabDragEnded) {
  // First, show the drop target.
  DragTabTo(kDragPointForStartDropTargetShow);
  FastForward(kShowDropTargetForTabDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Ending the drag should hide it.
  controller().OnTabDragEnded();
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target timer is cancelled when a tab drag is not in the
// "drop area".
TEST_F(MultiContentsViewDropTargetControllerDragTest,
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
  FastForward(kShowDropTargetForTabDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  controller().OnTabDragUpdated(mock_tab_drag_controller,
                                kDragPointForHiddenTargets);
  FastForward(kHideDropTargetDelay + kHideDropTargetAnimation);
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that CanDropTab returns true only when the drop target is visible.
TEST_F(MultiContentsViewDropTargetControllerDragTest, CanDropTab) {
  // Target is initially not visible.
  EXPECT_FALSE(controller().CanDropTab());

  // Show the drop target by simulating a link drag.
  DragURLTo(kDragPointForEndDropTargetShow);
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());

  // Now, CanDropTab should be true.
  EXPECT_TRUE(controller().CanDropTab());
}

// Tests that the destruction callback is fired when the controller is
// destroyed.
TEST_F(MultiContentsViewDropTargetControllerDragTest,
       RegisterWillDestroyCallback) {
  bool callback_fired = false;
  auto subscription = controller().RegisterWillDestroyCallback(
      base::BindOnce([](bool* fired) { *fired = true; }, &callback_fired));
  EXPECT_FALSE(callback_fired);

  // Resetting the controller unique_ptr will destroy it.
  ResetController();
  EXPECT_TRUE(callback_fired);
}

TEST_F(MultiContentsViewDropTargetControllerDragTest, ShowAndHideNudge) {
  // Drag to the start of the screen should show the nudge on the start side.
  DragURLTo(kDragPointForStartDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudge);

  // Dragging within 40% of the edge should not hide the target.
  DragURLTo(gfx::Point(kMultiContentsViewSize.width() * 0.39f,
                       kMultiContentsViewSize.height()));
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudge);

  // Drag to the end of the screen should show the nudge on the end side.
  DragURLTo(kDragPointForEndDropTargetShow);
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::END);
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudge);
}

// Tests that the nudge is shown a limited amount of times.
TEST_F(MultiContentsViewDropTargetControllerDragTest, NudgeShownLimit) {
  TestNudgeShownLimit(true);
}

// Tests that multiple nudges within the same drag count independently towards
// the limit.
TEST_F(MultiContentsViewDropTargetControllerDragTest,
       NudgeShownLimitSingleDrag) {
  TestNudgeShownLimit(false);
}

// Tests that if the nudge is no longer shown after using the drop zone a
// certain number of times.
TEST_F(MultiContentsViewDropTargetControllerDragTest, NudgeUsedLimit) {
  base::UserActionTester user_action_tester;
  ASSERT_EQ(0, user_action_tester.GetActionCount(kNudgeUsedUserActionName));
  ASSERT_EQ(0, prefs()->GetInteger(prefs::kSplitViewDragAndDropNudgeUsedCount));

  // The first kSideBySideDropTargetNudgeUsedLimit drags should show the nudge.
  for (int expected_count = 1;
       expected_count <= features::kSideBySideDropTargetNudgeUsedLimit.Get();
       ++expected_count) {
    DragURLTo(kDragPointForStartDropTargetShow);
    FastForward(kShowDropTargetForLinkDelay);
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
  FastForward(kShowDropTargetForLinkDelay);
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
  EXPECT_EQ(features::kSideBySideDropTargetNudgeUsedLimit.Get(),
            user_action_tester.GetActionCount(kNudgeUsedUserActionName));
  EXPECT_EQ(features::kSideBySideDropTargetNudgeUsedLimit.Get(),
            prefs()->GetInteger(prefs::kSplitViewDragAndDropNudgeUsedCount));
}

TEST_F(MultiContentsViewDropTargetControllerDragTest, ShowAndHideNudgeRTL) {
  SetRTL(true);

  // Drag to the start of the screen should show the nudge on the end side.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(kShowDropTargetForLinkDelay);
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
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudge);
}

TEST_F(MultiContentsViewDropTargetControllerDragTest, NudgeToFull) {
  // Drag to the start of the screen should show the nudge on the start side.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(kShowDropTargetForLinkDelay);
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

TEST_F(MultiContentsViewDropTargetControllerDragTest, NudgeToFullToHidden) {
  // Drag to the start of the screen should show the nudge on the start side.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(kShowDropTargetForLinkDelay);
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

TEST_F(MultiContentsViewDropTargetControllerDragTest, HandleTabDrop) {
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
  FastForward(kShowDropTargetForTabDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::END);

  // Expect the delegate's HandleTabDrop to be called with the END side.
  EXPECT_CALL(
      drop_delegate(),
      HandleTabDrop(MultiContentsDropTargetView::DropSide::END, testing::_));
  controller().HandleTabDrop(mock_tab_drag_controller);
}

TEST_F(MultiContentsViewDropTargetControllerDragTest, DragDelegateMethods) {
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

TEST_F(MultiContentsViewDropTargetControllerDragTest,
       ShowsFullDropTargetWhenAnimationsDisabled) {
  ASSERT_TRUE(
      base::FeatureList::IsEnabled(features::kSideBySideDropTargetNudge));
  auto animation_mode_reset = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
  ASSERT_FALSE(drop_target_view().ShouldShowAnimation());
  ASSERT_FALSE(drop_target_view().GetVisible());

  // Drag to the start of the screen.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(kShowDropTargetForLinkDelay);

  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kFull);
}

TEST_F(MultiContentsViewDropTargetControllerDragTest,
       FullToNudgeTransitionNotAllowed) {
  // Drag to the start of the screen should show the nudge on the start side.
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(kShowDropTargetForLinkDelay);
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
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  EXPECT_EQ(drop_target_view().state().value(),
            MultiContentsDropTargetView::DropTargetState::kNudgeToFull);
}

// Tests that the drag type is correctly set to `DragType::kLink` when
// dragging a link.
TEST_F(MultiContentsViewDropTargetControllerDragTest,
       OnWebContentsDragUpdate_SetsDragTypeToLink) {
  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().drag_type().has_value());
  EXPECT_EQ(drop_target_view().drag_type().value(),
            MultiContentsDropTargetView::DragType::kLink);
}

// Tests that the drag type is correctly set to `DragType::kTab` when
// dragging a tab.
TEST_F(MultiContentsViewDropTargetControllerDragTest,
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
  FastForward(kShowDropTargetForLinkDelay);
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().drag_type().has_value());
  EXPECT_EQ(drop_target_view().drag_type().value(),
            MultiContentsDropTargetView::DragType::kTab);
}

}  // namespace
