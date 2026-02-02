// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_view.h"

#include <memory>
#include <vector>

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_no_tab_groups_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

namespace {

tab_groups::SavedTabGroup CreateGroup(const std::u16string& title) {
  return tab_groups::SavedTabGroup(
      title, tab_groups::TabGroupColorId::kGrey, /*urls=*/{},
      /*position=*/std::nullopt, /*saved_guid=*/std::nullopt,
      /*local_group_id=*/std::nullopt, /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*created_before_syncing_tab_groups=*/false);
}

MATCHER_P(IsForTabGroup, expected_tab_group, "") {
  auto* label =
      static_cast<ProjectsPanelTabGroupsItemView*>(arg)->title_for_testing();
  return expected_tab_group.title() == label->GetText();
}

}  // namespace

class ProjectsPanelTabGroupsViewTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    root_action_item_ = actions::ActionItem::Builder().Build();
    action_view_controller_ = std::make_unique<views::ActionViewController>();

    tab_groups_view_ = std::make_unique<ProjectsPanelTabGroupsView>(
        root_action_item_.get(), action_view_controller_.get());
  }

 protected:
  testing::NiceMock<tab_groups::MockTabGroupSyncService>
      mock_tab_group_sync_service_;
  std::unique_ptr<actions::ActionItem> root_action_item_;
  std::unique_ptr<views::ActionViewController> action_view_controller_;
  std::unique_ptr<ProjectsPanelTabGroupsView> tab_groups_view_;

  void VerifyNoTabGroupsView() {
    // Should contain only the no tab groups message.
    EXPECT_EQ(1u, tab_groups_view_->children().size());
    views::Label* no_tabs_label = static_cast<views::Label*>(
        tab_groups_view_->no_tab_groups_view_for_testing()->children()[0]);
    EXPECT_EQ(
        u"Organize your tabs by grouping them together and label them with a "
        u"custom name and color",
        no_tabs_label->GetText());
  }
};

TEST_F(ProjectsPanelTabGroupsViewTest, EmptyTabGroups) {
  tab_groups_view_->SetTabGroups({});
  VerifyNoTabGroupsView();
}

TEST_F(ProjectsPanelTabGroupsViewTest, EnsureNoTabGroupsRemovedCorrectly) {
  tab_groups_view_->SetTabGroups({});
  VerifyNoTabGroupsView();
  tab_groups_view_->SetTabGroups(
      {CreateGroup(u"Group 1"), CreateGroup(u"Group 2")});
  EXPECT_EQ(nullptr, tab_groups_view_->no_tab_groups_view_for_testing());
  tab_groups_view_->SetTabGroups({});
  VerifyNoTabGroupsView();
}

TEST_F(ProjectsPanelTabGroupsViewTest, PopulatesTabGroups) {
  std::vector<tab_groups::SavedTabGroup> groups = {CreateGroup(u"Group 1"),
                                                   CreateGroup(u"Group 2")};

  tab_groups_view_->SetTabGroups(groups);

  ASSERT_EQ(2u, tab_groups_view_->children().size());

  for (size_t i = 0; i < groups.size(); ++i) {
    EXPECT_THAT(tab_groups_view_->children()[i], IsForTabGroup(groups[i]));
  }
}
