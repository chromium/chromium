// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kSessionIDValue = 123;
constexpr int kTopContainerWidth = 240;
}  // namespace

class VerticalTabStripTopContainerTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    SessionID test_session_id = SessionID::FromSerializedValue(kSessionIDValue);
    EXPECT_CALL(mock_browser_window_interface_, GetUnownedUserDataHost)
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
    controller_ = std::make_unique<tabs::VerticalTabStripStateController>(
        &mock_browser_window_interface_, &pref_service_,
        /*root_action_item=*/nullptr,
        /*session_service=*/nullptr, test_session_id,
        /*restored_state_collapsed=*/std::nullopt,
        /*restored_state_uncollapsed_width=*/std::nullopt);

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
    top_container_ =
        widget_->SetContentsView(std::make_unique<VerticalTabStripTopContainer>(
            controller_.get(), action_item_.get()));
    widget_->Show();
  }

  void TearDown() override {
    top_container_ = nullptr;
    widget_.reset();
    controller_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  VerticalTabStripTopContainer* top_container() { return top_container_; }

  views::LabelButton* tab_search_button() {
    return top_container_->GetTabSearchButton();
  }

  views::LabelButton* collapse_button() {
    return top_container_->GetCollapseButton();
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
  std::unique_ptr<tabs::VerticalTabStripStateController> controller_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  std::unique_ptr<actions::ActionItem> action_item_;
};

TEST_F(VerticalTabStripTopContainerTest, LayoutWithoutExclusionZone) {
  top_container()->SetExclusionWidthForLayout(0);
  top_container()->SetToolbarHeightForLayout(0);
  LayoutView();

  const gfx::Rect container_bounds = top_container()->bounds();
  const gfx::Rect search_bounds = tab_search_button()->bounds();
  const gfx::Rect collapse_bounds = collapse_button()->bounds();

  // The tab search button should be right aligned to the container and
  // vertically centered. Due to rounding, there is an off-by-one error
  // with the vertical centering of the button.
  EXPECT_EQ(search_bounds.top_right().x(), container_bounds.top_right().x());
  EXPECT_NEAR(search_bounds.right_center().y(),
              container_bounds.right_center().y(), 1);

  // The collapse button should be to the left of the tab search button.
  EXPECT_LT(collapse_bounds.CenterPoint().x(), search_bounds.CenterPoint().x());
  EXPECT_EQ(collapse_bounds.CenterPoint().y(), search_bounds.CenterPoint().y());
}

TEST_F(VerticalTabStripTopContainerTest, LayoutWithFullWidthExclusionZone) {
  top_container()->SetExclusionWidthForLayout(0);
  top_container()->SetToolbarHeightForLayout(0);
  LayoutView();

  const gfx::Rect initial_search_bounds = tab_search_button()->bounds();
  const gfx::Rect initial_collapse_bounds = collapse_button()->bounds();

  top_container()->SetExclusionWidthForLayout(kTopContainerWidth);
  constexpr int kExclusionHeight = 50;
  top_container()->SetToolbarHeightForLayout(kExclusionHeight);
  LayoutView();

  const gfx::Rect search_bounds = tab_search_button()->bounds();
  const gfx::Rect collapse_bounds = collapse_button()->bounds();

  // Both buttons are shifted down
  EXPECT_EQ(search_bounds.top_right().x(),
            initial_search_bounds.top_right().x());
  EXPECT_EQ(search_bounds.right_center().y(),
            initial_search_bounds.right_center().y() + kExclusionHeight);

  EXPECT_EQ(collapse_bounds.top_right().x(),
            initial_collapse_bounds.top_right().x());
  EXPECT_EQ(collapse_bounds.right_center().y(),
            initial_collapse_bounds.right_center().y() + kExclusionHeight);
}

TEST_F(VerticalTabStripTopContainerTest, LayoutWithPartialWidthExclusionZone) {
  top_container()->SetExclusionWidthForLayout(50);
  top_container()->SetToolbarHeightForLayout(50);
  LayoutView();

  const gfx::Rect container_bounds = top_container()->bounds();
  const gfx::Rect search_bounds = tab_search_button()->bounds();
  const gfx::Rect collapse_bounds = collapse_button()->bounds();

  // The tab search button should be right aligned to the container and
  // vertically centered. Due to rounding, there is an off-by-one error
  // with the vertical centering of the button.
  EXPECT_EQ(search_bounds.top_right().x(), container_bounds.top_right().x());
  EXPECT_NEAR(search_bounds.right_center().y(),
              container_bounds.right_center().y(), 1);

  // The collapse button should be to the left of the tab search button.
  EXPECT_LT(collapse_bounds.CenterPoint().x(), search_bounds.CenterPoint().x());
  EXPECT_EQ(collapse_bounds.CenterPoint().y(), search_bounds.CenterPoint().y());
}
