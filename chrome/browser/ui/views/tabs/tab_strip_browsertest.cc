// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include <vector>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/ax_event_counter.h"
#include "url/gurl.h"

namespace {
ui::MouseEvent GetDummyEvent() {
  return ui::MouseEvent(ui::EventType::kMousePressed, gfx::PointF(),
                        gfx::PointF(), base::TimeTicks::Now(), 0, 0);
}
}  // namespace

// Integration tests for interactions between TabStripModel and TabStrip.
class TabStripBrowsertest : public InProcessBrowserTest {
 public:
  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  TabStrip* tab_strip() {
    return BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  }

  void AppendTab() { chrome::AddTabAt(browser(), GURL(), -1, true); }

  tab_groups::TabGroupId AddTabToNewGroup(int tab_index) {
    return tab_strip_model()->AddToNewGroup({tab_index});
  }

  void AddTabToExistingGroup(int tab_index, tab_groups::TabGroupId group) {
    ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

    tab_strip_model()->AddToExistingGroup({tab_index}, group);
  }

  std::vector<content::WebContents*> GetWebContentses() {
    std::vector<content::WebContents*> contentses;
    for (int i = 0; i < tab_strip()->GetTabCount(); ++i)
      contentses.push_back(tab_strip_model()->GetWebContentsAt(i));
    return contentses;
  }

  std::vector<content::WebContents*> GetWebContentsesInOrder(
      const std::vector<int>& order) {
    std::vector<content::WebContents*> contentses;
    for (int i = 0; i < tab_strip()->GetTabCount(); ++i)
      contentses.push_back(tab_strip_model()->GetWebContentsAt(order[i]));
    return contentses;
  }
};

// Regression test for crbug.com/983961.
IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabAndDeleteGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToNewGroup(2);

  Tab* tab0 = tab_strip()->tab_at(0);
  Tab* tab1 = tab_strip()->tab_at(1);
  Tab* tab2 = tab_strip()->tab_at(2);

  tab_strip_model()->AddToExistingGroup({2}, group);

  EXPECT_EQ(tab0, tab_strip()->tab_at(0));
  EXPECT_EQ(tab2, tab_strip()->tab_at(1));
  EXPECT_EQ(tab1, tab_strip()->tab_at(2));

  EXPECT_EQ(group, tab_strip_model()->GetTabGroupForTab(1));

  std::vector<tab_groups::TabGroupId> groups =
      tab_strip_model()->group_model()->ListTabGroups();
  EXPECT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0], group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabPrevious_Success) {
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({1, 0, 2});
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabPrevious_AddsToGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);

  // Instead of moving, the tab should be added to the group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(2)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabPrevious_PastCollapsedGroup_Success) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group));

  const auto expected = GetWebContentsesInOrder({2, 0, 1});
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group));
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), std::nullopt);
  EXPECT_EQ(tab_strip()->tab_at(1)->group().value(), group);
  EXPECT_EQ(tab_strip()->tab_at(2)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabPrevious_BetweenTwoCollapsedGroups_Success) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());
  AppendTab();
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group1);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group1));
  tab_strip()->ToggleTabGroupCollapsedState(group1);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group1));

  tab_groups::TabGroupId group2 = AddTabToNewGroup(2);
  AddTabToExistingGroup(3, group2);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group2));
  tab_strip()->ToggleTabGroupCollapsedState(group2);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group2));

  const auto expected = GetWebContentsesInOrder({0, 1, 4, 2, 3});
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(4));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group1));
  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group2));
  EXPECT_EQ(tab_strip()->tab_at(0)->group().value(), group1);
  EXPECT_EQ(tab_strip()->tab_at(1)->group().value(), group1);
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), std::nullopt);
  EXPECT_EQ(tab_strip()->tab_at(3)->group().value(), group2);
  EXPECT_EQ(tab_strip()->tab_at(4)->group().value(), group2);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabPrevious_RemovesFromGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  AddTabToNewGroup(1);

  // Instead of moving, the tab should be removed from the group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(1)->group(), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabPrevious_ShiftsBetweenGroups) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToNewGroup(1);

  // Instead of moving, the tab should be removed from its old group, then added
  // to the new group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(1)->group(), std::nullopt);
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(1)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabPrevious_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(0));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabPrevious_Failure_Pinned) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftTabPrevious(tab_strip()->tab_at(1));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabNext_Success) {
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({1, 0, 2});
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabNext_AddsToGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);

  // Instead of moving, the tab should be added to the group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(0)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabNext_PastCollapsedGroup_Success) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group));

  const auto expected = GetWebContentsesInOrder({1, 2, 0});
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group));
  EXPECT_EQ(tab_strip()->tab_at(0)->group().value(), group);
  EXPECT_EQ(tab_strip()->tab_at(1)->group().value(), group);
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabNext_BetweenTwoCollapsedGroups_Success) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group1);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group1));
  tab_strip()->ToggleTabGroupCollapsedState(group1);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group1));

  tab_groups::TabGroupId group2 = AddTabToNewGroup(3);
  AddTabToExistingGroup(4, group2);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group2));
  tab_strip()->ToggleTabGroupCollapsedState(group2);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group2));

  const auto expected = GetWebContentsesInOrder({1, 2, 0, 3, 4});
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group1));
  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group2));
  EXPECT_EQ(tab_strip()->tab_at(0)->group().value(), group1);
  EXPECT_EQ(tab_strip()->tab_at(1)->group().value(), group1);
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), std::nullopt);
  EXPECT_EQ(tab_strip()->tab_at(3)->group().value(), group2);
  EXPECT_EQ(tab_strip()->tab_at(4)->group().value(), group2);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabNext_RemovesFromGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  AddTabToNewGroup(1);

  // Instead of moving, the tab should be removed from the group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(1)->group(), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabNext_ShiftsBetweenGroups) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  AddTabToNewGroup(0);
  tab_groups::TabGroupId group = AddTabToNewGroup(1);

  // Instead of moving, the tab should be removed from its old group, then added
  // to the new group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), std::nullopt);
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(0)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabNext_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(2));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabNext_Failure_Pinned) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftTabNext(tab_strip()->tab_at(0));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_NoPinnedTabs_Success) {
  AppendTab();
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({2, 0, 1, 3});
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_PinnedTabs_Success) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto expected = GetWebContentsesInOrder({0, 2, 1, 3});
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_DoesNotAddToGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  AddTabToNewGroup(0);

  tab_strip()->MoveTabFirst(tab_strip()->tab_at(1));
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_RemovesFromGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  AddTabToNewGroup(0);
  AddTabToNewGroup(1);

  tab_strip()->MoveTabFirst(tab_strip()->tab_at(0));
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), std::nullopt);

  tab_strip()->MoveTabFirst(tab_strip()->tab_at(1));
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_NoPinnedTabs_Failure) {
  AppendTab();
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(0));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_PinnedTabs_Failure) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(1));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       MoveTabFirst_MovePinnedTab_Success) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);

  const auto expected = GetWebContentsesInOrder({2, 0, 1, 3});
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_NoPinnedTabs_Success) {
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({1, 2, 0});
  tab_strip()->MoveTabLast(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_MovePinnedTab_Success) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);

  const auto expected = GetWebContentsesInOrder({0, 2, 1, 3});
  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_AllPinnedTabs_Success) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);

  const auto expected = GetWebContentsesInOrder({0, 2, 1});
  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_DoesNotAddToGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  AddTabToNewGroup(2);

  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_RemovesFromGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  AddTabToNewGroup(1);
  AddTabToNewGroup(2);

  tab_strip()->MoveTabLast(tab_strip()->tab_at(2));
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), std::nullopt);

  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_NoPinnedTabs_Failure) {
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabLast(tab_strip()->tab_at(2));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_PinnedTabs_Failure) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_AllPinnedTabs_Failure) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabLast(tab_strip()->tab_at(2));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupLeft_Success) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);

  const auto expected = GetWebContentsesInOrder({1, 2, 0});
  tab_strip()->ShiftGroupLeft(group);
  EXPECT_EQ(expected, GetWebContentses());
}

// TODO(crbug.com/353618704): Re-enable this test
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_ShiftGroupLeft_OtherGroup DISABLED_ShiftGroupLeft_OtherGroup
#else
#define MAYBE_ShiftGroupLeft_OtherGroup ShiftGroupLeft_OtherGroup
#endif
IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MAYBE_ShiftGroupLeft_OtherGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(2);
  AddTabToExistingGroup(3, group1);

  tab_groups::TabGroupId group2 = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group2);

  const auto expected = GetWebContentsesInOrder({2, 3, 0, 1});
  tab_strip()->ShiftGroupLeft(group1);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftGroupLeft_Failure_EdgeOfTabstrip) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupLeft(group);
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupLeft_Failure_Pinned) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupLeft(group);
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupRight_Success) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group);

  const auto expected = GetWebContentsesInOrder({2, 0, 1});
  tab_strip()->ShiftGroupRight(group);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupRight_OtherGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group1);

  tab_groups::TabGroupId group2 = AddTabToNewGroup(2);
  AddTabToExistingGroup(3, group2);

  const auto expected = GetWebContentsesInOrder({2, 3, 0, 1});
  tab_strip()->ShiftGroupRight(group1);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftGroupRight_Failure_EdgeOfTabstrip) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupRight(group);
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftCollapsedGroupLeft_Success) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group));

  const auto expected = GetWebContentsesInOrder({1, 2, 0});
  tab_strip()->ShiftGroupLeft(group);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftCollapsedGroupLeft_OtherCollapsedGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(2);
  AddTabToExistingGroup(3, group1);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group1));
  tab_strip()->ToggleTabGroupCollapsedState(group1);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group1));

  tab_groups::TabGroupId group2 = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group2);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group2));
  tab_strip()->ToggleTabGroupCollapsedState(group2);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group2));

  const auto expected = GetWebContentsesInOrder({2, 3, 0, 1, 4});
  tab_strip()->ShiftGroupLeft(group1);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftCollapsedGroupLeft_Failure_EdgeOfTabstrip) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group));

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupLeft(group);

  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftCollapsedGroupLeft_Failure_Pinned) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group));

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupLeft(group);

  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftCollapsedGroupRight_Success) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group));

  const auto expected = GetWebContentsesInOrder({2, 0, 1});
  tab_strip()->ShiftGroupRight(group);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftCollapsedGroupRight_OtherCollapsedGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group1);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group1));
  tab_strip()->ToggleTabGroupCollapsedState(group1);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group1));

  tab_groups::TabGroupId group2 = AddTabToNewGroup(2);
  AddTabToExistingGroup(3, group2);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group2));
  tab_strip()->ToggleTabGroupCollapsedState(group2);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group2));

  const auto expected = GetWebContentsesInOrder({2, 3, 0, 1, 4});
  tab_strip()->ShiftGroupRight(group1);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftCollapsedGroupRight_Failure_EdgeOfTabstrip) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group));

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupRight(group);
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       CollapseGroup_WithActiveTabInGroup_SelectsNext) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  tab_strip()->SelectTab(tab_strip()->tab_at(0), GetDummyEvent());
  ASSERT_EQ(0, tab_strip()->GetActiveIndex());
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);

  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group));
  EXPECT_EQ(1, tab_strip()->GetActiveIndex());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       CollapseGroup_WhenAddingActiveTab_ExpandsGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);

  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group));
  EXPECT_EQ(1, tab_strip()->GetActiveIndex());

  tab_strip_model()->AddToExistingGroup({1}, group);
  EXPECT_FALSE(tab_strip()->IsGroupCollapsed(group));
  EXPECT_EQ(1, tab_strip()->GetActiveIndex());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       CollapseGroup_WhenAddingInactiveTab_StaysCollapsed) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);

  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);

  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group));
  EXPECT_EQ(2, tab_strip()->GetActiveIndex());

  tab_strip_model()->AddToExistingGroup({1}, group);

  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group));
  EXPECT_EQ(2, tab_strip()->GetActiveIndex());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       CollapseGroup_WithActiveTabInGroup_SelectsPrevious) {
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  tab_strip()->SelectTab(tab_strip()->tab_at(1), GetDummyEvent());
  ASSERT_EQ(1, tab_strip()->GetActiveIndex());
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);

  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group));
  EXPECT_EQ(0, tab_strip()->GetActiveIndex());
}

IN_PROC_BROWSER_TEST_F(
    TabStripBrowsertest,
    CollapseGroup_WithActiveTabOutsideGroup_DoesNotChangeActiveTab) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  tab_strip()->SelectTab(tab_strip()->tab_at(1), GetDummyEvent());
  ASSERT_EQ(1, tab_strip()->GetActiveIndex());
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);

  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group));
  EXPECT_EQ(1, tab_strip()->GetActiveIndex());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, CollapseGroup_CreatesNewTab) {
  ASSERT_EQ(1, tab_strip_model()->count());
  AppendTab();
  ASSERT_EQ(2, tab_strip_model()->count());

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  tab_strip_model()->AddToExistingGroup({1}, group);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));

  // Any origin other than kMenuAction will work here. At the time this was
  // written, it was impossible to trigger this specific interaction (collapsing
  // a group) from a context menu.
  tab_strip()->ToggleTabGroupCollapsedState(
      group, ToggleTabGroupCollapsedStateOrigin::kMouse);
  ASSERT_EQ(3, tab_strip_model()->count());

  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group));
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ActivateTabInCollapsedGroup_ExpandsCollapsedGroup) {
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
  tab_strip()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group));
  ASSERT_EQ(1, tab_strip()->GetActiveIndex());

  tab_strip()->SelectTab(tab_strip()->tab_at(0), GetDummyEvent());
  EXPECT_FALSE(tab_strip()->IsGroupCollapsed(group));
}

// Tests IDC_SELECT_TAB_0, IDC_SELECT_NEXT_TAB, IDC_SELECT_PREVIOUS_TAB and
// IDC_SELECT_LAST_TAB. The tab navigation accelerators should ignore tabs in
// collapsed groups.
IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, TabGroupTabNavigationAccelerators) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());
  // Create five tabs.
  for (int i = 0; i < 4; i++)
    AppendTab();

  ASSERT_EQ(5, tab_strip_model()->count());

  // Add the first, second, and last tab into their own collapsed groups.
  tab_groups::TabGroupId group1 = tab_strip_model()->AddToNewGroup({0});
  tab_groups::TabGroupId group2 = tab_strip_model()->AddToNewGroup({1});
  tab_groups::TabGroupId group3 = tab_strip_model()->AddToNewGroup({4});
  tab_strip()->ToggleTabGroupCollapsedState(group1);
  tab_strip()->ToggleTabGroupCollapsedState(group2);
  tab_strip()->ToggleTabGroupCollapsedState(group3);

  // Select the fourth tab.
  tab_strip_model()->ActivateTabAt(3);

  CommandUpdater* updater = browser()->command_controller();

  // Navigate to the first tab using an accelerator.
  updater->ExecuteCommand(IDC_SELECT_TAB_0);
  ASSERT_EQ(2, tab_strip_model()->active_index());

  // Navigate back to the first tab using the previous accelerators.
  updater->ExecuteCommand(IDC_SELECT_PREVIOUS_TAB);
  ASSERT_EQ(3, tab_strip_model()->active_index());

  // Navigate to the second tab using the next accelerators.
  updater->ExecuteCommand(IDC_SELECT_NEXT_TAB);
  ASSERT_EQ(2, tab_strip_model()->active_index());

  // Navigate to the last tab using the select last accelerator.
  updater->ExecuteCommand(IDC_SELECT_LAST_TAB);
  ASSERT_EQ(3, tab_strip_model()->active_index());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       TabHasCorrectAccessibleSelectedState) {
  AppendTab();
  AppendTab();

  Tab* tab0 = tab_strip()->tab_at(0);
  Tab* tab1 = tab_strip()->tab_at(1);
  ui::AXNodeData ax_node_data_0;
  ui::AXNodeData ax_node_data_1;
  views::test::AXEventCounter counter(views::AXEventManager::Get());

  tab_strip()->SelectTab(tab_strip()->tab_at(0), GetDummyEvent());
  tab0->GetViewAccessibility().GetAccessibleNodeData(&ax_node_data_0);
  tab1->GetViewAccessibility().GetAccessibleNodeData(&ax_node_data_1);
  EXPECT_TRUE(tab0->IsSelected());
  EXPECT_TRUE(
      ax_node_data_0.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(tab1->IsSelected());
  EXPECT_FALSE(
      ax_node_data_1.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kSelection), 1);

  tab_strip()->SelectTab(tab_strip()->tab_at(1), GetDummyEvent());
  ax_node_data_0 = ui::AXNodeData();
  ax_node_data_1 = ui::AXNodeData();
  tab0->GetViewAccessibility().GetAccessibleNodeData(&ax_node_data_0);
  tab1->GetViewAccessibility().GetAccessibleNodeData(&ax_node_data_1);
  EXPECT_FALSE(tab0->IsSelected());
  EXPECT_FALSE(
      ax_node_data_0.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_TRUE(tab1->IsSelected());
  EXPECT_TRUE(
      ax_node_data_1.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kSelection), 2);
}
