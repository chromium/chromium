// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/tabs/hovercard/fade_label_view.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/aura/window.h"
#endif

namespace {

class VerticalTabStripControllerBrowserTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
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

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       ClickTabInImmersiveMode) {
  if (base::FeatureList::IsEnabled(features::kInitialWebUI)) {
    GTEST_SKIP() << "Skipping test because it fails with InitialWebUI enabled. "
                    "See crbug.com/477426026.";
  }

  // Add another tab to switch to.
  AppendTab();

  // Enter immersive fullscreen.
  chrome::ToggleFullscreenMode(browser());
  RunScheduledLayouts();

  TabCollectionNode* last_tab_node =
      unpinned_collection_node()->children()[1].get();
  views::View* last_tab_view = last_tab_node->view();

#if BUILDFLAG(IS_CHROMEOS)
  ui::test::EventGenerator event_generator(
      browser()->window()->GetNativeWindow()->GetRootWindow());
#else
  ui::test::EventGenerator event_generator(
      browser()->window()->GetNativeWindow());
#endif

  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);

  event_generator.MoveMouseTo(last_tab_view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ToolbarView* toolbar = browser_view->toolbar();
  ToolbarButtonProvider* button_provider =
      browser_view->toolbar_button_provider();
  EXPECT_TRUE(toolbar->IsDrawn());
  EXPECT_TRUE(button_provider->GetBackButton()->IsDrawn());
  EXPECT_TRUE(toolbar->forward_button()->IsDrawn());
  EXPECT_TRUE(toolbar->reload_button()->IsDrawn());
  EXPECT_TRUE(toolbar->location_bar()->IsDrawn());
  EXPECT_TRUE(button_provider->GetAppMenuControl()->IsDrawn());
}

#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

class VerticalTabGroupHoverCardTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  VerticalTabGroupHoverCardTest() {
    TabHoverCardController::set_disable_animations_for_testing(true);
  }

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{features::kTabGroupsCollapseFreezing, {}},
            {tabs::kVerticalTabs, {}},
            {features::kTabGroupHoverCards, {}}};
  }
};

IN_PROC_BROWSER_TEST_F(VerticalTabGroupHoverCardTest,
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

IN_PROC_BROWSER_TEST_F(VerticalTabGroupHoverCardTest,
                       TabGroupHeaderHoverCardNamed) {
  // Create a group with some tabs and a name.
  AppendTab();
  AppendTab();
  TabStripModel* model = browser()->tab_strip_model();
  tab_groups::TabGroupId group_id = model->AddToNewGroup({1, 2});
  std::u16string group_title = u"My Group";
  model->ChangeTabGroupVisuals(
      group_id,
      tab_groups::TabGroupVisualData(group_title,
                                     tab_groups::TabGroupColorId::kBlue),
      false);

  // Get the VerticalTabGroupHeaderView.
  TabCollectionNode* group_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::GROUP);
  ASSERT_TRUE(group_node);
  VerticalTabGroupView* group_view =
      views::AsViewClass<VerticalTabGroupView>(group_node->view());
  VerticalTabGroupHeaderView* header_view = group_view->group_header();

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

IN_PROC_BROWSER_TEST_F(VerticalTabGroupHoverCardTest,
                       TabGroupHeaderHoverCardWithExcessTabs) {
  // Create a group with more than kMaxTabs tabs.
  const size_t n_tabs = tabs::TabGroupData::kMaxTabs + 1;
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

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       MoveTabFirstAndLastUnpinned) {
  AppendTab();
  AppendTab();

  auto* tab_model = browser()->tab_strip_model();
  ASSERT_EQ(3, tab_model->count());

  auto* tab0 = tab_model->GetTabAtIndex(0);
  auto* tab1 = tab_model->GetTabAtIndex(1);
  auto* tab2 = tab_model->GetTabAtIndex(2);

  vertical_tab_strip_controller()->MoveTabLast(tab0);

  // Verify the order of the tabs after tab0 is moved to the end.
  EXPECT_EQ(tab_model->GetTabAtIndex(2), tab0);
  EXPECT_EQ(tab_model->GetTabAtIndex(0), tab1);
  EXPECT_EQ(tab_model->GetTabAtIndex(1), tab2);

  vertical_tab_strip_controller()->MoveTabFirst(tab0);

  // Verify the order of the tabs after tab0 is moved to the front.
  EXPECT_EQ(tab_model->GetTabAtIndex(0), tab0);
  EXPECT_EQ(tab_model->GetTabAtIndex(1), tab1);
  EXPECT_EQ(tab_model->GetTabAtIndex(2), tab2);
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       MoveTabFirstAndLastPinned) {
  AppendTab();
  AppendTab();

  auto* tab_model = browser()->tab_strip_model();
  ASSERT_EQ(3, tab_model->count());

  tab_model->SetTabPinned(0, true);
  tab_model->SetTabPinned(1, true);

  EXPECT_TRUE(tab_model->IsTabPinned(0));
  EXPECT_TRUE(tab_model->IsTabPinned(1));
  EXPECT_FALSE(tab_model->IsTabPinned(2));

  auto* tab0 = tab_model->GetTabAtIndex(0);
  auto* tab1 = tab_model->GetTabAtIndex(1);
  auto* tab2 = tab_model->GetTabAtIndex(2);

  vertical_tab_strip_controller()->MoveTabLast(tab0);

  // Verify the order of the tabs after tab0 is moved to the end of the
  // pinned tabs.
  EXPECT_EQ(tab_model->GetTabAtIndex(1), tab0);
  EXPECT_EQ(tab_model->GetTabAtIndex(0), tab1);
  EXPECT_EQ(tab_model->GetTabAtIndex(2), tab2);

  vertical_tab_strip_controller()->MoveTabFirst(tab0);

  // Verify the order of the tabs after tab0 is moved to the front of the
  // pinned tabs.
  EXPECT_EQ(tab_model->GetTabAtIndex(0), tab0);
  EXPECT_EQ(tab_model->GetTabAtIndex(1), tab1);
  EXPECT_EQ(tab_model->GetTabAtIndex(2), tab2);
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       ShiftTabIntoGroup) {
  AppendTab();
  AppendTab();

  auto* tab_model = browser()->tab_strip_model();
  ASSERT_EQ(3, tab_model->count());

  // Create a group containing the last tab.
  tab_groups::TabGroupId group_id = tab_model->AddToNewGroup({2});

  // Tab 1 is ungrouped and adjacent to Tab 2 (which is in the group).
  auto* tab1 = tab_model->GetTabAtIndex(1);
  EXPECT_FALSE(tab1->GetGroup().has_value());

  // Shift tab 1 next.
  vertical_tab_strip_controller()->ShiftTabNext(tab1);

  // Verify that tab 1 is now in the group.
  EXPECT_EQ(group_id, tab1->GetGroup());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       ShiftTabOutOfGroup) {
  AppendTab();

  auto* tab_model = browser()->tab_strip_model();
  ASSERT_EQ(2, tab_model->count());

  // Create a group containing the first tab.
  tab_groups::TabGroupId group_id = tab_model->AddToNewGroup({0});

  // Tab 0 is in the group. Tab 1 is ungrouped.
  auto* tab0 = tab_model->GetTabAtIndex(0);
  EXPECT_EQ(group_id, tab0->GetGroup());

  // Shift tab 0 next.
  vertical_tab_strip_controller()->ShiftTabNext(tab0);

  // Verify that tab 0 is no longer in the group.
  EXPECT_FALSE(tab0->GetGroup().has_value());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       ShiftTabPastCollapsedGroup) {
  AppendTab();
  AppendTab();
  AppendTab();

  auto* tab_model = browser()->tab_strip_model();
  ASSERT_EQ(4, tab_model->count());

  // Ungrouped (Tab 0), Grouped (Tab 1, Tab 2), Ungrouped (Tab 3)
  tab_groups::TabGroupId group_id = tab_model->AddToNewGroup({1, 2});
  TabGroup* group = tab_model->group_model()->GetTabGroup(group_id);

  // Collapse the group.
  vertical_tab_strip_controller()->ToggleTabGroupCollapsedState(
      group, ToggleTabGroupCollapsedStateOrigin::kMouse);

  auto* tab0 = tab_model->GetTabAtIndex(0);
  auto* tab1 = tab_model->GetTabAtIndex(1);
  auto* tab2 = tab_model->GetTabAtIndex(2);
  auto* tab3 = tab_model->GetTabAtIndex(3);

  // Shift Tab 0 next.
  vertical_tab_strip_controller()->ShiftTabNext(tab0);

  // Verify the order.
  EXPECT_EQ(tab_model->GetTabAtIndex(0), tab1);
  EXPECT_EQ(tab_model->GetTabAtIndex(1), tab2);
  EXPECT_EQ(tab_model->GetTabAtIndex(2), tab0);
  EXPECT_EQ(tab_model->GetTabAtIndex(3), tab3);

  // Verify that tab 0 is not in any group.
  EXPECT_FALSE(tab0->GetGroup().has_value());
}

}  // namespace
