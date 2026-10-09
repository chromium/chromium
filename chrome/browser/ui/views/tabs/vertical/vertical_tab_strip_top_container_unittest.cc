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
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller_impl.h"
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
  bool IsCollapsing() override { return false; }
  void RequestCollapse(bool collapse) override {
    if (callback_) {
      callback_.Run(collapse);
    }
  }

 private:
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
    if (controller_) {
      controller_->SetDelegate(nullptr);
    }
    controller_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void ResetTopContainer() {
    top_container_ = nullptr;
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

  void LayoutView() {
    // Pass an arbitrarily large height for the available size bounds so the
    // widget can adjust as needed.
    widget_->SetSize(top_container_->GetPreferredSize(
        views::SizeBounds(kTopContainerWidth, 400)));
    widget_->LayoutRootViewIfNecessary();
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<VerticalTabStripTopContainer> top_container_;
  TestVerticalTabStripDelegate test_delegate_;
  std::unique_ptr<tabs::VerticalTabStripStateController> controller_;
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

TEST_F(VerticalTabStripTopContainerTest, ShowsSegmentedControlWhenEligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(organizer_panel::kOrganizerPanel);
  ResetTopContainer();

  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  controller()->SetUncollapsedWidth(300);
  top_container()->UpdateControlsVisibility();
  LayoutView();

  ASSERT_NE(segmented_control(), nullptr);
  EXPECT_TRUE(segmented_control()->GetVisible());
  EXPECT_FALSE(combo_button()->GetVisible());

  // Unpin tab search: segmented control should hide completely.
  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, false);
  top_container()->UpdateControlsVisibility();
  EXPECT_FALSE(segmented_control()->GetVisible());
  EXPECT_TRUE(combo_button()->GetVisible());
}

TEST_F(VerticalTabStripTopContainerTest,
       CollapsedExpandOnHoverRendersSegmentedControlAtExpandedWidth) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(organizer_panel::kOrganizerPanel);
  ResetTopContainer();

  pref_service()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  controller()->SetUncollapsedWidth(300);
  controller()->SetExpandOnHoverEnabled(true);
  controller()->RequestCollapse(true);
  EXPECT_TRUE(controller()->IsCollapsed());

  top_container()->UpdateControlsVisibility();
  ASSERT_NE(segmented_control(), nullptr);
  EXPECT_TRUE(segmented_control()->GetVisible());
  EXPECT_FALSE(combo_button()->GetVisible());

  // When narrow (unhovered), segmented control is rendered based on the known
  // uncollapsed width, outside of the narrow container width.
  top_container()->SetSize(
      gfx::Size(tabs::kVerticalTabStripCollapseSnapWidth - 10, 40));
  views::ProposedLayout layout = top_container()->CalculateProposedLayout(
      views::SizeBounds(top_container()->size()));
  auto it = std::ranges::find(layout.child_layouts, segmented_control(),
                              &views::ChildLayout::child_view);
  ASSERT_NE(it, layout.child_layouts.end());
  EXPECT_TRUE(it->visible);
  EXPECT_GT(it->bounds.x(), top_container()->width());

  // When expanded, the segmented control fits within the container width.
  top_container()->SetSize(gfx::Size(300, 40));
  layout = top_container()->CalculateProposedLayout(
      views::SizeBounds(top_container()->size()));
  it = std::ranges::find(layout.child_layouts, segmented_control(),
                         &views::ChildLayout::child_view);
  ASSERT_NE(it, layout.child_layouts.end());
  EXPECT_TRUE(it->visible);
  EXPECT_LE(it->bounds.right(), top_container()->width());

  // When expand on hover is disabled while collapsed, segmented control is
  // hidden and combo button is shown.
  controller()->SetExpandOnHoverEnabled(false);
  top_container()->UpdateControlsVisibility();
  EXPECT_FALSE(segmented_control()->GetVisible());
  EXPECT_TRUE(combo_button()->GetVisible());
}
