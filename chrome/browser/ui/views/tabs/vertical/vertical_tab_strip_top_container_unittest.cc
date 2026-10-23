// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller_impl.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_segmented_control.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kSessionIDValue = 123;
constexpr int kTopContainerWidth = 240;

class TestVerticalTabStripDelegate
    : public tabs::VerticalTabStripStateController::Delegate {
 public:
  void SetCollapsedStateUpdatedCallback(
      base::RepeatingCallback<void(bool)> callback) override {
    callback_ = std::move(callback);
  }
  bool IsCollapsing() override { return is_collapsing_; }
  void SetIsCollapsing(bool is_collapsing) { is_collapsing_ = is_collapsing; }
  void RequestCollapse(bool collapse) override {
    if (!is_collapsing_ && callback_) {
      callback_.Run(collapse);
    }
  }

 private:
  bool is_collapsing_ = false;
  base::RepeatingCallback<void(bool)> callback_;
};
}  // namespace

class VerticalTabStripTopContainerTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    SessionID test_session_id = SessionID::FromSerializedValue(kSessionIDValue);
    EXPECT_CALL(mock_browser_window_interface_, GetUnownedUserDataHost)
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .WillRepeatedly(testing::Return(&profile_));
    EXPECT_CALL(std::as_const(mock_browser_window_interface_), GetProfile())
        .WillRepeatedly(testing::Return(&profile_));
    profile_.GetTestingPrefService()->SetBoolean(prefs::kVerticalTabsEnabled,
                                                 true);
    controller_ = std::make_unique<tabs::VerticalTabStripStateControllerImpl>(
        mock_browser_window_interface_, profile_.GetTestingPrefService(),
        /*root_action_item=*/nullptr,
        /*session_service=*/nullptr, test_session_id,
        /*restored_state_collapsed=*/std::nullopt,
        /*restored_state_uncollapsed_width=*/std::nullopt);
    controller_->SetDelegate(&test_delegate_);

    action_item_ = actions::ActionItem::Builder().Build();
    action_item_->AddChild(
        actions::ActionItem::Builder()
            .SetActionId(kActionTabSearch)
            .SetText(l10n_util::GetStringUTF16(IDS_TAB_SEARCH_MENU))
            .Build());
    action_item_->AddChild(
        actions::ActionItem::Builder()
            .SetActionId(kActionToggleCollapseVertical)
            .SetText(l10n_util::GetStringUTF16(IDS_COLLAPSE_VERTICAL_TABS))
            .Build());

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    ResetTopContainer();
    widget_->Show();
  }

  void TearDown() override {
    top_container_ = nullptr;
    widget_.reset();
    organizer_panel_controller_.reset();
    animation_controller_.reset();
    if (controller_) {
      controller_->SetDelegate(nullptr);
    }
    controller_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void ResetTopContainer() {
    top_container_ = nullptr;
    if (organizer_panel::IsOrganizerPanelFeatureEnabled()) {
      if (!organizer_panel_controller_) {
        animation_controller_ = std::make_unique<BrowserAnimationController>(
            mock_browser_window_interface_);
        animation_controller_->AddAnimationProvider(
            std::make_unique<OrganizerPanelAnimations>());
        organizer_panel_controller_ =
            std::make_unique<OrganizerPanelController>(
                mock_browser_window_interface_, /*root_action_item=*/nullptr);
      }
    } else {
      organizer_panel_controller_.reset();
      animation_controller_.reset();
    }
    top_container_ =
        widget_->SetContentsView(std::make_unique<VerticalTabStripTopContainer>(
            controller_.get(), action_item_.get(),
            &mock_browser_window_interface_));
  }

 protected:
  VerticalTabStripTopContainer* top_container() { return top_container_; }

  TabStripComboButton* combo_button() {
    return top_container_->GetComboButton();
  }

  views::LabelButton* collapse_button() {
    return top_container_->GetCollapseButton();
  }

  VerticalTabStripSegmentedControl* segmented_control() {
    return top_container_->GetSegmentedControl();
  }

  tabs::VerticalTabStripStateControllerImpl* controller() {
    return static_cast<tabs::VerticalTabStripStateControllerImpl*>(
        controller_.get());
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return profile_.GetTestingPrefService();
  }

  TestVerticalTabStripDelegate& test_delegate() { return test_delegate_; }

  void LayoutView(int width = kTopContainerWidth) {
    // Pass an arbitrarily large height for the available size bounds so the
    // widget can adjust as needed.
    widget_->SetSize(
        top_container_->GetPreferredSize(views::SizeBounds(width, 400)));
    widget_->LayoutRootViewIfNecessary();
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<VerticalTabStripTopContainer> top_container_;
  TestVerticalTabStripDelegate test_delegate_;
  std::unique_ptr<tabs::VerticalTabStripStateController> controller_;
  std::unique_ptr<BrowserAnimationController> animation_controller_;
  std::unique_ptr<OrganizerPanelController> organizer_panel_controller_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  TestingProfile profile_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  std::unique_ptr<actions::ActionItem> action_item_;
};

TEST_F(VerticalTabStripTopContainerTest, LayoutWithoutExclusionZone) {
  top_container()->SetCaptionButtonWidthForLayout(1);
  top_container()->SetToolbarHeightForLayout(1);
  LayoutView();

  const gfx::Rect container_bounds = top_container()->bounds();
  const gfx::Rect combo_bounds = combo_button()->bounds();
  const gfx::Rect collapse_bounds = collapse_button()->bounds();

  // The combo button should be right aligned to the container and
  // vertically centered. Due to rounding, there is an off-by-one error
  // with the vertical centering of the button.
  EXPECT_EQ(combo_bounds.top_right().x(), container_bounds.top_right().x());
  EXPECT_NEAR(combo_bounds.right_center().y(),
              container_bounds.right_center().y(), 1);

  // The collapse button should be to the left of the combo button.
  EXPECT_LT(collapse_bounds.CenterPoint().x(), combo_bounds.CenterPoint().x());
  EXPECT_EQ(collapse_bounds.CenterPoint().y(), combo_bounds.CenterPoint().y());
}

TEST_F(VerticalTabStripTopContainerTest, LayoutWithFullWidthExclusionZone) {
  top_container()->SetCaptionButtonWidthForLayout(1);
  top_container()->SetToolbarHeightForLayout(1);
  LayoutView();

  const gfx::Rect initial_combo_bounds = combo_button()->bounds();

  top_container()->SetCaptionButtonWidthForLayout(kTopContainerWidth);
  constexpr int kExclusionHeight = 50;
  top_container()->SetToolbarHeightForLayout(kExclusionHeight);
  LayoutView();

  const gfx::Rect combo_bounds = combo_button()->bounds();
  const gfx::Rect collapse_bounds = collapse_button()->bounds();

  // Both buttons are shifted down to match the height of the bookmarks bar.
  const int expected_y_center =
      kExclusionHeight +
      (GetLayoutConstant(LayoutConstant::kBookmarkBarHeight) -
       GetLayoutConstant(LayoutConstant::kBookmarkBarButtonImageLabelPadding)) /
          2;

  EXPECT_EQ(combo_bounds.top_right().x(), initial_combo_bounds.top_right().x());
  EXPECT_EQ(combo_bounds.right_center().y(), expected_y_center);

  EXPECT_EQ(collapse_bounds.x(), 0);
  EXPECT_EQ(collapse_bounds.right_center().y(), expected_y_center);
}

TEST_F(VerticalTabStripTopContainerTest, LayoutWithPartialWidthExclusionZone) {
  top_container()->SetCaptionButtonWidthForLayout(50);
  top_container()->SetToolbarHeightForLayout(50);
  LayoutView();

  const gfx::Rect container_bounds = top_container()->bounds();
  const gfx::Rect combo_bounds = combo_button()->bounds();
  const gfx::Rect collapse_bounds = collapse_button()->bounds();

  // The combo button should be right aligned to the container and
  // vertically centered. Due to rounding, there is an off-by-one error
  // with the vertical centering of the button.
  EXPECT_EQ(combo_bounds.top_right().x(), container_bounds.top_right().x());
  EXPECT_NEAR(combo_bounds.right_center().y(),
              container_bounds.right_center().y(), 1);

  // The collapse button should be to the left of the combo button.
  EXPECT_LT(collapse_bounds.CenterPoint().x(), combo_bounds.CenterPoint().x());
  EXPECT_EQ(collapse_bounds.CenterPoint().y(), combo_bounds.CenterPoint().y());
}

TEST_F(VerticalTabStripTopContainerTest, DefaultShowsComboButton) {
  LayoutView();
  EXPECT_TRUE(combo_button()->GetVisible());
  EXPECT_EQ(segmented_control(), nullptr);
}

TEST_F(VerticalTabStripTopContainerTest,
       OrganizerPanelDefaultParamShowsComboButton) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(organizer_panel::kOrganizerPanel);
  ResetTopContainer();

  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  controller()->SetUncollapsedWidth(300);
  LayoutView();

  EXPECT_EQ(segmented_control(), nullptr);
  EXPECT_TRUE(combo_button()->GetVisible());
}

TEST_F(VerticalTabStripTopContainerTest,
       OrganizerPanelParamDisabledShowsComboButton) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      organizer_panel::kOrganizerPanel,
      {{organizer_panel::kOrganizerPanelUseSegmentationControl.name, "false"}});
  ResetTopContainer();

  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  controller()->SetUncollapsedWidth(300);
  LayoutView();

  EXPECT_EQ(segmented_control(), nullptr);
  EXPECT_TRUE(combo_button()->GetVisible());
}

TEST_F(VerticalTabStripTopContainerTest, ShowsSegmentedControlWhenEligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      organizer_panel::kOrganizerPanel,
      {{organizer_panel::kOrganizerPanelUseSegmentationControl.name, "true"}});
  ResetTopContainer();

  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  controller()->SetUncollapsedWidth(300);
  LayoutView();

  ASSERT_NE(segmented_control(), nullptr);
  EXPECT_TRUE(segmented_control()->GetVisible());
  const gfx::Point segmented_control_center =
      segmented_control()->bounds().CenterPoint();
  EXPECT_FALSE(
      top_container()->IsPositionInWindowCaption(segmented_control_center));

  // Unpin tab search: segmented control should hide completely.
  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, false);
  LayoutView();
  EXPECT_FALSE(segmented_control()->GetVisible());
  EXPECT_TRUE(combo_button()->GetVisible());
  EXPECT_TRUE(
      top_container()->IsPositionInWindowCaption(segmented_control_center));
}

TEST_F(VerticalTabStripTopContainerTest,
       CollapsedExpandOnHoverRendersSegmentedControlAtExpandedWidth) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      organizer_panel::kOrganizerPanel,
      {{organizer_panel::kOrganizerPanelUseSegmentationControl.name, "true"}});
  ResetTopContainer();

  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  controller()->SetUncollapsedWidth(300);
  controller()->SetExpandOnHoverEnabled(true);

  // During the collapse animation (kCollapsing), both segmented control and
  // combo button are hidden on a single row so the container height does not
  // jump when the animation ends.
  test_delegate().SetIsCollapsing(true);
  EXPECT_EQ(controller()->GetCollapseState(),
            tabs::VerticalTabStripCollapseState::kCollapsing);
  LayoutView(tabs::kVerticalTabStripCollapseSnapWidth - 10);
  ASSERT_NE(segmented_control(), nullptr);
  EXPECT_FALSE(segmented_control()->GetVisible());
  EXPECT_FALSE(combo_button()->GetVisible());
  const int collapsing_height = top_container()->height();

  test_delegate().SetIsCollapsing(false);
  controller()->RequestCollapse(true);
  EXPECT_TRUE(controller()->IsCollapsed());

  // When collapsed (unhovered), segmented control and combo button are hidden.
  const int collapsed_container_width =
      tabs::kVerticalTabStripCollapsedWidth -
      2 * GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);
  LayoutView(collapsed_container_width);
  EXPECT_FALSE(segmented_control()->GetVisible());
  EXPECT_FALSE(combo_button()->GetVisible());
  EXPECT_EQ(top_container()->height(), collapsing_height);

  // As soon as the expand-on-hover animation starts (even below the snap
  // width), the segmented control is visible and rendered at the expanded
  // width.
  LayoutView(tabs::kVerticalTabStripCollapseSnapWidth - 10);
  EXPECT_TRUE(segmented_control()->GetVisible());
  EXPECT_FALSE(combo_button()->GetVisible());

  // When fully expanded on hover, the segmented control is visible and fits
  // within the container width.
  top_container()->SetSize(gfx::Size(300, 40));
  views::ProposedLayout layout = top_container()->CalculateProposedLayout(
      views::SizeBounds(top_container()->size()));
  auto it = std::ranges::find(layout.child_layouts, segmented_control(),
                              &views::ChildLayout::child_view);
  ASSERT_NE(it, layout.child_layouts.end());
  EXPECT_TRUE(it->visible);
  EXPECT_LE(it->bounds.right(), top_container()->width());

  // When expand on hover is disabled while collapsed, segmented control is
  // hidden and combo button is shown.
  controller()->SetExpandOnHoverEnabled(false);
  LayoutView(tabs::kVerticalTabStripCollapseSnapWidth - 10);
  EXPECT_FALSE(segmented_control()->GetVisible());
  EXPECT_TRUE(combo_button()->GetVisible());
}

TEST_F(VerticalTabStripTopContainerTest,
       CollapsedExpandOnHoverRendersComboButtonAtExpandedWidth) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(organizer_panel::kOrganizerPanel);
  ResetTopContainer();

  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  controller()->SetUncollapsedWidth(300);
  controller()->SetExpandOnHoverEnabled(true);

  // During the collapse animation (kCollapsing), the combo button is already
  // hidden on a single row so the container height does not jump when the
  // animation ends.
  test_delegate().SetIsCollapsing(true);
  EXPECT_EQ(controller()->GetCollapseState(),
            tabs::VerticalTabStripCollapseState::kCollapsing);
  LayoutView(tabs::kVerticalTabStripCollapseSnapWidth - 10);
  EXPECT_EQ(segmented_control(), nullptr);
  EXPECT_FALSE(combo_button()->GetVisible());
  const int collapsing_height = top_container()->height();

  test_delegate().SetIsCollapsing(false);
  controller()->RequestCollapse(true);
  EXPECT_TRUE(controller()->IsCollapsed());

  // When collapsed (unhovered), combo button is hidden.
  const int collapsed_container_width =
      tabs::kVerticalTabStripCollapsedWidth -
      2 * GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);
  LayoutView(collapsed_container_width);
  EXPECT_EQ(segmented_control(), nullptr);
  EXPECT_FALSE(combo_button()->GetVisible());
  EXPECT_EQ(top_container()->height(), collapsing_height);

  // As soon as the expand-on-hover animation starts (even below the snap
  // width), the combo button is visible and rendered at the expanded width,
  // with its layer clipped to the visible bounds within the top container.
  LayoutView(tabs::kVerticalTabStripCollapseSnapWidth - 10);
  EXPECT_TRUE(combo_button()->GetVisible());
  ASSERT_NE(combo_button()->layer(), nullptr);
  EXPECT_FALSE(combo_button()->layer()->clip_rect().IsEmpty());

  // When fully expanded on hover, the combo button is visible, fits within
  // the container width, and has no layer clip rect so focus rings are not
  // clipped.
  LayoutView(300);
  EXPECT_TRUE(combo_button()->GetVisible());
  EXPECT_LE(combo_button()->bounds().right(), top_container()->width());
  EXPECT_TRUE(combo_button()->layer()->clip_rect().IsEmpty());

  // When expand on hover is disabled while collapsed, combo button is
  // laid out vertically inside the collapsed width with no layer clip rect.
  controller()->SetExpandOnHoverEnabled(false);
  LayoutView(tabs::kVerticalTabStripCollapseSnapWidth - 10);
  EXPECT_TRUE(combo_button()->GetVisible());
  EXPECT_LE(combo_button()->bounds().right(), top_container()->width());
  EXPECT_TRUE(combo_button()->layer()->clip_rect().IsEmpty());
}

TEST_F(VerticalTabStripTopContainerTest,
       CollapsedExpandOnHoverOrganizerDisabledLaysOutComboButtonVertically) {
  ResetTopContainer();

  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  controller()->SetUncollapsedWidth(300);
  controller()->SetExpandOnHoverEnabled(true);
  controller()->RequestCollapse(true);
  EXPECT_TRUE(controller()->IsCollapsed());

  top_container()->SetSize(
      gfx::Size(tabs::kVerticalTabStripCollapseSnapWidth - 10, 40));
  views::ProposedLayout layout = top_container()->CalculateProposedLayout(
      views::SizeBounds(top_container()->size()));
  auto it = std::ranges::find(layout.child_layouts, combo_button(),
                              &views::ChildLayout::child_view);
  ASSERT_NE(it, layout.child_layouts.end());
  EXPECT_TRUE(it->visible);
  EXPECT_LE(it->bounds.right(), top_container()->width());
}

TEST_F(VerticalTabStripTopContainerTest,
       CollapsedExpandOnHoverWithCaptionButtonsExclusionZone) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      organizer_panel::kOrganizerPanel,
      {{organizer_panel::kOrganizerPanelUseSegmentationControl.name, "true"}});
  ResetTopContainer();

  constexpr int kCaptionButtonWidth = 50;
  top_container()->SetCaptionButtonWidthForLayout(kCaptionButtonWidth);
  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  controller()->SetUncollapsedWidth(300);
  controller()->SetExpandOnHoverEnabled(true);

  // During the collapse animation (kCollapsing) below the snap width, the
  // collapse button should already be positioned at 0 rather than shifted over
  // by the caption buttons exclusion zone width.
  test_delegate().SetIsCollapsing(true);
  top_container()->SetSize(
      gfx::Size(tabs::kVerticalTabStripCollapseSnapWidth - 10, 40));
  views::ProposedLayout layout = top_container()->CalculateProposedLayout(
      views::SizeBounds(top_container()->size()));
  auto collapse_it = std::ranges::find(layout.child_layouts, collapse_button(),
                                       &views::ChildLayout::child_view);
  ASSERT_NE(collapse_it, layout.child_layouts.end());
  EXPECT_TRUE(collapse_it->visible);
  EXPECT_EQ(collapse_it->bounds.x(), 0);

  test_delegate().SetIsCollapsing(false);
  controller()->RequestCollapse(true);
  EXPECT_TRUE(controller()->IsCollapsed());

  const int collapsed_container_width =
      tabs::kVerticalTabStripCollapsedWidth -
      2 * GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);
  LayoutView(collapsed_container_width);
  ASSERT_NE(segmented_control(), nullptr);
  EXPECT_FALSE(segmented_control()->GetVisible());
  EXPECT_FALSE(combo_button()->GetVisible());

  // In collapsed mode with expand on hover enabled, the collapse button should
  // not be shifted over by the caption buttons exclusion zone width.
  top_container()->SetSize(
      gfx::Size(tabs::kVerticalTabStripCollapseSnapWidth - 10, 40));
  layout = top_container()->CalculateProposedLayout(
      views::SizeBounds(top_container()->size()));
  collapse_it = std::ranges::find(layout.child_layouts, collapse_button(),
                                  &views::ChildLayout::child_view);
  ASSERT_NE(collapse_it, layout.child_layouts.end());
  EXPECT_TRUE(collapse_it->visible);
  EXPECT_EQ(collapse_it->bounds.x(), 0);

  // When uncollapsed, the collapse button should be shifted over by the
  // caption buttons exclusion width (if it fits on the same line).
  controller()->RequestCollapse(false);
  EXPECT_FALSE(controller()->IsCollapsed());
  top_container()->SetSize(gfx::Size(300, 40));
  layout = top_container()->CalculateProposedLayout(
      views::SizeBounds(top_container()->size()));
  collapse_it = std::ranges::find(layout.child_layouts, collapse_button(),
                                  &views::ChildLayout::child_view);
  ASSERT_NE(collapse_it, layout.child_layouts.end());
  EXPECT_TRUE(collapse_it->visible);
  EXPECT_EQ(collapse_it->bounds.x(), kCaptionButtonWidth);
}

TEST_F(VerticalTabStripTopContainerTest, CollapsedSwitchesControls) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      organizer_panel::kOrganizerPanel,
      {{organizer_panel::kOrganizerPanelUseSegmentationControl.name, "true"}});
  ResetTopContainer();

  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  controller()->SetUncollapsedWidth(300);
  controller()->SetExpandOnHoverEnabled(false);
  LayoutView();

  ASSERT_NE(segmented_control(), nullptr);
  EXPECT_TRUE(segmented_control()->GetVisible());
  EXPECT_FALSE(combo_button()->GetVisible());
  EXPECT_FALSE(controller()->IsCollapsed());

  // Collapsing switches visibility to the combo button during layout.
  controller()->RequestCollapse(true);
  LayoutView(tabs::kVerticalTabStripCollapsedWidth);
  EXPECT_TRUE(controller()->IsCollapsed());
  EXPECT_FALSE(segmented_control()->GetVisible());
  EXPECT_TRUE(combo_button()->GetVisible());

  // Uncollapsing switches visibility back to the segmented control.
  controller()->RequestCollapse(false);
  LayoutView();
  EXPECT_FALSE(controller()->IsCollapsed());
  EXPECT_TRUE(segmented_control()->GetVisible());
  EXPECT_FALSE(combo_button()->GetVisible());
}

TEST_F(VerticalTabStripTopContainerTest, LayoutWidthSwitchesControls) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      organizer_panel::kOrganizerPanel,
      {{organizer_panel::kOrganizerPanelUseSegmentationControl.name, "true"}});
  ResetTopContainer();

  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  controller()->SetUncollapsedWidth(300);
  LayoutView();

  ASSERT_NE(segmented_control(), nullptr);
  EXPECT_TRUE(segmented_control()->GetVisible());
  EXPECT_FALSE(combo_button()->GetVisible());

  // Simulate resizing narrower past the organizer panel threshold mid-drag
  // (where state_controller still has 300px, but layout width is 180px).
  LayoutView(180);
  EXPECT_FALSE(segmented_control()->GetVisible());
  EXPECT_TRUE(combo_button()->GetVisible());

  // Simulate resizing wider past the threshold mid-drag.
  LayoutView(240);
  EXPECT_TRUE(segmented_control()->GetVisible());
  EXPECT_FALSE(combo_button()->GetVisible());
}

TEST_F(VerticalTabStripTopContainerTest, ModeChangedSwitchesControls) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      organizer_panel::kOrganizerPanel,
      {{organizer_panel::kOrganizerPanelUseSegmentationControl.name, "true"}});
  controller()->SetVerticalTabsEnabled(false);
  ResetTopContainer();

  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  controller()->SetUncollapsedWidth(300);
  LayoutView();

  ASSERT_NE(segmented_control(), nullptr);
  EXPECT_FALSE(segmented_control()->GetVisible());
  EXPECT_TRUE(combo_button()->GetVisible());

  controller()->SetVerticalTabsEnabled(true);
  LayoutView();
  EXPECT_TRUE(segmented_control()->GetVisible());
  EXPECT_FALSE(combo_button()->GetVisible());

  controller()->SetVerticalTabsEnabled(false);
  LayoutView();
  EXPECT_FALSE(segmented_control()->GetVisible());
  EXPECT_TRUE(combo_button()->GetVisible());
}
