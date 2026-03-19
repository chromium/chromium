// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/fade_label_view.h"
#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/view_utils.h"

namespace {

class VerticalTabStripControllerBrowserTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  VerticalTabStripControllerBrowserTest() {
    TabHoverCardController::set_disable_animations_for_testing(true);
  }

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{features::kTabGroupsCollapseFreezing, {}},
            {tabs::kVerticalTabs, {}}};
  }

  bool CheckMenuHasStringId(int message_id) {
    ui::SimpleMenuModel* menu_model = vertical_tab_strip_controller()
                                          ->GetTabContextMenuController()
                                          ->GetMenuModel();
    for (size_t i = 0; i < menu_model->GetItemCount(); i++) {
      if (l10n_util::GetStringUTF16(message_id) == menu_model->GetLabelAt(i)) {
        return true;
      }
    }
    return false;
  }
};

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       VerifyTabContextMenuText) {
  // Get the first tab's node.
  TabCollectionNode* first_tab_node =
      unpinned_collection_node()->children()[0].get();
  ASSERT_TRUE(first_tab_node);

  // Open Tab Context Menu manually.
  vertical_tab_strip_controller()->ShowContextMenuForNode(
      first_tab_node, first_tab_node->view(), gfx::Point(),
      ui::mojom::MenuSourceType::kMouse);

  // Verify "New Tab Below" text is present.
  EXPECT_TRUE(CheckMenuHasStringId(IDS_TAB_CXMENU_NEWTABBELOW));
  // Verify "Close Tabs Below" text is present.
  EXPECT_TRUE(CheckMenuHasStringId(IDS_TAB_CXMENU_CLOSETABSBELOW));

  // Close menu to avoid the test hanging.
  vertical_tab_strip_controller()->GetTabContextMenuController()->CloseMenu();
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       TabGroupCollapseFreezing) {
  // Create two tabs and add them to a group.
  AppendTab();
  AppendTab();
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  TabGroup* group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);

  TabCollectionNode* group_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::GROUP);
  VerticalTabView* tab_view0 = views::AsViewClass<VerticalTabView>(
      group_node->children()[0].get()->view());
  VerticalTabView* tab_view1 = views::AsViewClass<VerticalTabView>(
      group_node->children()[1].get()->view());

  // Collapse the group.
  vertical_tab_strip_controller()->ToggleTabGroupCollapsedState(
      group, ToggleTabGroupCollapsedStateOrigin::kMouse);

  // Verify freezing votes.
  EXPECT_TRUE(tab_view0->HasFreezingVote());
  EXPECT_TRUE(tab_view1->HasFreezingVote());

  // Expand the group.
  vertical_tab_strip_controller()->ToggleTabGroupCollapsedState(
      group, ToggleTabGroupCollapsedStateOrigin::kMouse);

  // Verify freezing votes are released.
  EXPECT_FALSE(tab_view0->HasFreezingVote());
  EXPECT_FALSE(tab_view1->HasFreezingVote());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest, ShiftTabNext) {
  AppendTab();

  TabStripModel* model = browser()->tab_strip_model();
  ASSERT_EQ(2, model->count());

  // Tab Starts at Index 0.
  tabs::TabInterface* tab_to_move = model->GetTabAtIndex(0);
  EXPECT_EQ(0, model->GetIndexOfTab(tab_to_move));

  // Shift the tab next (0 -> 1).
  vertical_tab_strip_controller()->ShiftTabNext(tab_to_move);

  // Verify Tab is at Index 1.
  EXPECT_EQ(1, model->GetIndexOfTab(tab_to_move));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       ShiftTabPrevious) {
  AppendTab();

  TabStripModel* model = browser()->tab_strip_model();
  ASSERT_EQ(2, model->count());

  // Tab Starts at Index 1.
  tabs::TabInterface* tab_to_move = model->GetTabAtIndex(1);
  EXPECT_EQ(1, model->GetIndexOfTab(tab_to_move));

  // Shift the tab previous (1 -> 0).
  vertical_tab_strip_controller()->ShiftTabPrevious(tab_to_move);

  // Verify Tab is at Index 0.
  EXPECT_EQ(0, model->GetIndexOfTab(tab_to_move));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       TabGroupHeaderHoverCardUnnamed) {
  AppendTab();
  AppendTab();
  TabStripModel* model = browser()->tab_strip_model();

  model->AddToNewGroup({1, 2});
  RunScheduledLayouts();

  // Get the VerticalTabGroupHeaderView.
  TabCollectionNode* group_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::GROUP);
  ASSERT_TRUE(group_node);
  VerticalTabGroupView* group_view =
      views::AsViewClass<VerticalTabGroupView>(group_node->view());

  VerticalTabGroupHeaderView* header_view = group_view->group_header();
  header_view->SetHoverCardDataForTesting();

  // Trigger the hover card.
  hover_card_controller()->UpdateHoverCard(
      header_view, TabSlotController::HoverCardUpdateType::kHover);
  TabHoverCardBubbleView* bubble =
      hover_card_controller()->hover_card_for_testing();
  ASSERT_TRUE(bubble);

  // Check header text.
  std::u16string expected_header = l10n_util::GetPluralStringFUTF16(
      IDS_TAB_GROUPS_UNNAMED_GROUP_HOVER_CARD_HEADER, 2);
  EXPECT_EQ(bubble->GetGroupTitleViewForTesting()->GetText(), expected_header);

  // Check tab titles start with bullets.
  const std::vector<raw_ptr<FadeLabelView>>& tab_title_views =
      bubble->GetGroupTabTitleViewsForTesting();

  std::u16string expected_tab_title = l10n_util::GetStringFUTF16(
      IDS_LIST_BULLET, model->GetTabAtIndex(1)->GetContents()->GetTitle());
  EXPECT_EQ(tab_title_views[0]->GetText(), expected_tab_title);
  EXPECT_TRUE(tab_title_views[0]->GetVisible());
  EXPECT_EQ(tab_title_views[1]->GetText(), expected_tab_title);
  EXPECT_TRUE(tab_title_views[1]->GetVisible());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       TabGroupHeaderHoverCardNamed) {
  // Create a group with some tabs and a name.
  AppendTab();
  AppendTab();
  TabStripModel* model = browser()->tab_strip_model();
  tab_groups::TabGroupId group_id = model->AddToNewGroup({1, 2});
  TabGroup* group = model->group_model()->GetTabGroup(group_id);
  std::u16string group_title = u"My Group";
  group->SetVisualData(tab_groups::TabGroupVisualData(
      group_title, tab_groups::TabGroupColorId::kBlue));

  // Get the VerticalTabGroupHeaderView.
  TabCollectionNode* group_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::GROUP);
  ASSERT_TRUE(group_node);
  VerticalTabGroupView* group_view =
      views::AsViewClass<VerticalTabGroupView>(group_node->view());
  VerticalTabGroupHeaderView* header_view = group_view->group_header();
  header_view->SetHoverCardDataForTesting();

  // Trigger the hover card.
  hover_card_controller()->UpdateHoverCard(
      header_view, TabSlotController::HoverCardUpdateType::kHover);

  // Verify the hover card exists and has correct content.
  TabHoverCardBubbleView* bubble =
      hover_card_controller()->hover_card_for_testing();
  ASSERT_TRUE(bubble);

  // Check header text.
  std::u16string expected_header = l10n_util::FormatString(
      l10n_util::GetPluralStringFUTF16(IDS_TAB_GROUPS_HOVER_CARD_HEADER, 2),
      {group_title}, nullptr);
  EXPECT_EQ(bubble->GetGroupTitleViewForTesting()->GetText(), expected_header);
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       TabGroupHeaderHoverCardWithExcessTabs) {
  // Create a group with more than kMaxTabs tabs.
  const size_t n_tabs = GroupCardData::kMaxTabs + 1;
  for (size_t i = 0; i < n_tabs; ++i) {
    AppendTab();
  }
  chrome::GroupAllUngroupedTabs(browser());

  // Get the VerticalTabGroupHeaderView.
  TabCollectionNode* group_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::GROUP);
  ASSERT_TRUE(group_node);
  VerticalTabGroupView* group_view =
      views::AsViewClass<VerticalTabGroupView>(group_node->view());
  VerticalTabGroupHeaderView* header_view = group_view->group_header();
  header_view->SetHoverCardDataForTesting();

  // Trigger the hover card.
  hover_card_controller()->UpdateHoverCard(
      header_view, TabSlotController::HoverCardUpdateType::kHover);
  TabHoverCardBubbleView* bubble =
      hover_card_controller()->hover_card_for_testing();
  ASSERT_TRUE(bubble);

  // There should be 2 excess tabs. Note that the group has |n_tabs+1| tabs
  // because of the initial tab made in the browser, and the call to
  // |GroupAllUngroupedTabs|.
  std::u16string expected_footer = l10n_util::GetStringFUTF16(
      IDS_TAB_GROUPS_HOVER_CARD_FOOTER, base::NumberToString16(2));
  EXPECT_EQ(bubble->GetGroupFooterViewForTesting()->GetText(), expected_footer);
  EXPECT_TRUE(bubble->GetGroupFooterViewForTesting()->GetVisible());
}

// TODO(crbug.com/490428062): Expand Test Coverage for Keyboard Commands.

}  // namespace
