// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
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
    for (int i = 0; i < tab_strip()->GetTabCount(); ++i) {
      contentses.push_back(tab_strip_model()->GetWebContentsAt(i));
    }
    return contentses;
  }

  std::vector<content::WebContents*> GetWebContentsesInOrder(
      const std::vector<int>& order) {
    std::vector<content::WebContents*> contentses;
    for (int i = 0; i < tab_strip()->GetTabCount(); ++i) {
      contentses.push_back(tab_strip_model()->GetWebContentsAt(order[i]));
    }
    return contentses;
  }

  std::u16string GetCollapsedState(tab_groups::TabGroupId group) {
    std::u16string collapsed_state = std::u16string();

#if !BUILDFLAG(IS_WIN)
    collapsed_state =
        tab_strip()->IsGroupCollapsed(group)
            ? l10n_util::GetStringUTF16(IDS_GROUP_AX_LABEL_COLLAPSED)
            : l10n_util::GetStringUTF16(IDS_GROUP_AX_LABEL_EXPANDED);
#endif

    return collapsed_state;
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

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, DetachAndReInsertGroup) {
  ASSERT_TRUE(tab_strip_model()->SupportsTabGroups());

  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = tab_strip_model()->AddToNewGroup({0, 1});

  std::unique_ptr<DetachedTabCollection> detached_group =
      tab_strip_model()->DetachTabGroupForInsertion(group);

  EXPECT_EQ(tab_strip()->GetTabCount(), 1);
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), std::nullopt);

  tab_strip_model()->InsertDetachedTabGroupAt(std::move(detached_group), 1);

  EXPECT_EQ(tab_strip()->GetTabCount(), 3);
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), std::nullopt);
  EXPECT_EQ(tab_strip()->tab_at(1)->group(), group);
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), group);
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

// Regression test for crbug.com/394381780. When active tab is the tab right
// after the collapsed group and a new foreground tab is added to the end of the
// group, the group should expand.
IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       AddForegroundTabToCollapsedGroupExpandsGroup) {
  AppendTab();
  AppendTab();
  ASSERT_EQ(3, tab_strip_model()->count());

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  tab_strip_model()->ActivateTabAt(2);

  tab_strip()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group));

  // Add a tab to the group.
  chrome::AddTabAt(browser(), GURL(), 2, true, group);

  ASSERT_FALSE(tab_strip()->IsGroupCollapsed(group));
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

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, AccessibleName) {
  AppendTab();
  AppendTab();

  ui::AXNodeData data;
  tab_strip()->tab_at(1)->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(u"New Tab",
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  // AccessibleName should update when tab group is changed
  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  std::u16string tab_title = browser()->GetTitleForTab(1);
  std::u16string group_title = tab_strip()->GetGroupTitle(group);
  std::u16string title =
      group_title.empty()
          ? l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_UNNAMED_GROUP_FORMAT,
                                       tab_title)
          : l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_UNNAMED_GROUP_FORMAT,
                                       tab_title, group_title);
  data = ui::AXNodeData();
  tab_strip()->tab_at(1)->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(title,
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  // AccessibleName should update with crashedstatus
  TabRendererData tab_renderer_data = tab_strip()->tab_at(1)->data();
  tab_renderer_data.crashed_status =
      base::TERMINATION_STATUS_PROCESS_WAS_KILLED;
  tab_strip()->tab_at(1)->SetData(tab_renderer_data);
  data = ui::AXNodeData();
  tab_strip()->tab_at(1)->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_CRASHED_FORMAT, title),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  // AccessibleName update with pinned status and network status change
  int new_index = tab_strip_model()->SetTabPinned(1, true);
  title = l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_PINNED_FORMAT, tab_title);
  tab_renderer_data = tab_strip()->tab_at(new_index)->data();
  tab_renderer_data.network_state = TabNetworkState::kError;
  tab_strip()->tab_at(new_index)->SetData(tab_renderer_data);
  data = ui::AXNodeData();
  tab_strip()->tab_at(new_index)->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_NETWORK_ERROR_FORMAT, title),
      data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  // AccessibleName update with alert on tab
  tab_renderer_data = tab_strip()->tab_at(new_index)->data();
  tab_renderer_data.network_state = TabNetworkState::kLoading;
  tab_renderer_data.alert_state.push_back(tabs::TabAlert::AUDIO_PLAYING);
  tab_strip()->tab_at(new_index)->SetData(tab_renderer_data);
  data = ui::AXNodeData();
  tab_strip()->tab_at(new_index)->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_AUDIO_PLAYING_FORMAT, title),
      data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  // AccessibleName update with tab resource usage update
  tab_renderer_data = tab_strip()->tab_at(new_index)->data();
  auto tab_resource_usage = base::MakeRefCounted<TabResourceUsage>();
  tab_resource_usage->SetMemoryUsageInBytes(100);
  tab_renderer_data.tab_resource_usage = std::move(tab_resource_usage);
  tab_strip()->tab_at(new_index)->SetData(tab_renderer_data);
  data = ui::AXNodeData();
  tab_strip()->tab_at(new_index)->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_TAB_AX_MEMORY_USAGE,
                l10n_util::GetStringFUTF16(
                    IDS_TAB_AX_LABEL_AUDIO_PLAYING_FORMAT, title),
                ui::FormatBytes(100)),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       DISABLED_TabGroupHeaderAccessibleProperties) {
  browser()->set_update_ui_immediately_for_testing();
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  tab_strip()->tab_at(1)->SetGroup(group);
  tab_strip_model()->ChangeTabGroupVisuals(
      group, tab_groups::TabGroupVisualData(
                 u"Test title", tab_groups::TabGroupColorId::kBlue));

  auto* group_header = tab_strip()->group_header(group);
  std::u16string group_title = tab_strip_model()
                                   ->group_model()
                                   ->GetTabGroup(group)
                                   ->visual_data()
                                   ->title();

  EXPECT_FALSE(tab_strip()->IsGroupCollapsed(group));
  std::u16string collapsed_state = GetCollapsedState(group);
  EXPECT_FALSE(group_title.empty());
  ui::AXNodeData data;
  group_header->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(IDS_GROUP_AX_LABEL_NAMED_GROUP_FORMAT,
                                       u"Test title", u"\"New Tab\"",
                                       collapsed_state));

  // Validating tab_group name update & collapsed state change should update the
  // accessible name.
  tab_strip_model()->ChangeTabGroupVisuals(
      group,
      tab_groups::TabGroupVisualData(u"", tab_groups::TabGroupColorId::kBlue));
  group_title = tab_strip_model()
                    ->group_model()
                    ->GetTabGroup(group)
                    ->visual_data()
                    ->title();
  EXPECT_TRUE(group_title.empty());
  tab_strip()->ToggleTabGroupCollapsedState(group);
  EXPECT_TRUE(tab_strip()->IsGroupCollapsed(group));
  collapsed_state = GetCollapsedState(group);
  data = ui::AXNodeData();
  group_header->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(IDS_GROUP_AX_LABEL_UNNAMED_GROUP_FORMAT,
                                       u"\"New Tab\"", collapsed_state));

  tab_strip_model()->ChangeTabGroupVisuals(
      group, tab_groups::TabGroupVisualData(
                 u"New test title", tab_groups::TabGroupColorId::kBlue));
  collapsed_state = GetCollapsedState(group);
  EXPECT_FALSE(tab_strip()->IsGroupCollapsed(group));
  group_title = tab_strip_model()
                    ->group_model()
                    ->GetTabGroup(group)
                    ->visual_data()
                    ->title();
  EXPECT_FALSE(group_title.empty());
  data = ui::AXNodeData();
  group_header->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(IDS_GROUP_AX_LABEL_NAMED_GROUP_FORMAT,
                                       u"New test title", u"\"New Tab\"",
                                       collapsed_state));

  // Validating tab's title update in a tab_group should update accessible name.
  AppendTab();
  AppendTab();
  AppendTab();
  group = AddTabToNewGroup(3);
  AddTabToExistingGroup(4, group);
  AddTabToExistingGroup(5, group);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  auto* tab_group = tab_strip_model()->group_model()->GetTabGroup(group);

  gfx::Range tabs_in_group = tab_group->ListTabs();
  auto* web_contents =
      tab_strip_model()->GetWebContentsAt(tabs_in_group.start());
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  std::u16string new_title = u"New Tab Title For Test";
  web_contents->UpdateTitleForEntry(entry, new_title);
  run_loop.RunUntilIdle();
  EXPECT_EQ(web_contents->GetTitle(), new_title);

  collapsed_state = GetCollapsedState(group);
  group_header = tab_strip()->group_header(group);
  data = ui::AXNodeData();
  group_header->GetViewAccessibility().GetAccessibleNodeData(&data);
  std::u16string group_header_contents = base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_PLACEHOLDER_GROUP_TITLE,
                                       2),
      std::vector<std::u16string>{u"New Tab Title For Test"}, nullptr);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(IDS_GROUP_AX_LABEL_UNNAMED_GROUP_FORMAT,
                                       group_header_contents, collapsed_state));

  // Other than first tab in a group, if any tab's title is updated, it should
  // not update the accessible name.
  web_contents = tab_strip_model()->GetWebContentsAt(tabs_in_group.start() + 1);
  entry = web_contents->GetController().GetVisibleEntry();
  new_title = u"New Tab Title For Test 2";
  web_contents->UpdateTitleForEntry(entry, new_title);
  run_loop.RunUntilIdle();
  EXPECT_EQ(web_contents->GetTitle(), new_title);
  data = ui::AXNodeData();
  group_header->GetViewAccessibility().GetAccessibleNodeData(&data);
  group_header_contents = base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_PLACEHOLDER_GROUP_TITLE,
                                       2),
      std::vector<std::u16string>{u"New Tab Title For Test"}, nullptr);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(IDS_GROUP_AX_LABEL_UNNAMED_GROUP_FORMAT,
                                       group_header_contents, collapsed_state));

  // Validate accessible name update with tab move.
  gfx::Range initial_tabs_in_group(3, 6);
  EXPECT_EQ(initial_tabs_in_group, tab_group->ListTabs());
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(3));
  gfx::Range updated_tabs_in_group(4, 6);
  EXPECT_EQ(updated_tabs_in_group, tab_group->ListTabs());
  EXPECT_EQ(new_title, tab_strip_model()->GetWebContentsAt(4)->GetTitle());

  data = ui::AXNodeData();
  group_header->GetViewAccessibility().GetAccessibleNodeData(&data);
  group_header_contents = base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_PLACEHOLDER_GROUP_TITLE,
                                       1),
      std::vector<std::u16string>{u"New Tab Title For Test 2"}, nullptr);

  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(IDS_GROUP_AX_LABEL_UNNAMED_GROUP_FORMAT,
                                       group_header_contents, collapsed_state));

  // Rearrange tabs in a group should update the accessible name.
  initial_tabs_in_group = gfx::Range(4, 6);
  EXPECT_EQ(initial_tabs_in_group, tab_group->ListTabs());

  tabs_in_group = tab_group->ListTabs();
  web_contents = tab_strip_model()->GetWebContentsAt(tabs_in_group.start() + 1);
  entry = web_contents->GetController().GetVisibleEntry();
  new_title = u"Middle Tab Title";
  web_contents->UpdateTitleForEntry(entry, new_title);
  run_loop.RunUntilIdle();
  EXPECT_EQ(web_contents->GetTitle(), new_title);

  // tab_strip_model()->MoveTabToIndexImpl(6, 4, group, false, false);
  tab_strip()->SelectTab(tab_strip()->tab_at(5), GetDummyEvent());
  tab_strip_model()->MoveTabPrevious();
  updated_tabs_in_group = gfx::Range(4, 6);
  EXPECT_EQ(updated_tabs_in_group, tab_group->ListTabs());
  EXPECT_EQ(new_title, tab_strip_model()->GetWebContentsAt(4)->GetTitle());

  data = ui::AXNodeData();
  group_header->GetViewAccessibility().GetAccessibleNodeData(&data);
  group_header_contents = base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_PLACEHOLDER_GROUP_TITLE,
                                       1),
      std::vector<std::u16string>{new_title}, nullptr);

  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(IDS_GROUP_AX_LABEL_UNNAMED_GROUP_FORMAT,
                                       group_header_contents, collapsed_state));

  // Moving out all tabs in a tab group to another group
  tab_strip()->SelectTab(tab_strip()->tab_at(4), GetDummyEvent());
  tab_strip()->ExtendSelectionTo(tab_strip()->tab_at(6));
  tab_groups::TabGroupId new_group = tab_strip_model()->AddToNewGroup({0});
  tab_strip_model()->MoveSelectedTabsTo(0, new_group);
  collapsed_state = GetCollapsedState(new_group);
  group_header = tab_strip()->group_header(new_group);
  data = ui::AXNodeData();
  group_header->GetViewAccessibility().GetAccessibleNodeData(&data);
  group_header_contents = base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_PLACEHOLDER_GROUP_TITLE,
                                       3),
      std::vector<std::u16string>{new_title}, nullptr);

  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(IDS_GROUP_AX_LABEL_UNNAMED_GROUP_FORMAT,
                                       group_header_contents, collapsed_state));
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

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, TabGroupHeaderTooltipText) {
  browser()->set_update_ui_immediately_for_testing();
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  tab_strip()->tab_at(1)->SetGroup(group);
  tab_strip_model()->ChangeTabGroupVisuals(
      group, tab_groups::TabGroupVisualData(
                 u"Non empty title text", tab_groups::TabGroupColorId::kBlue));

  auto* group_header = tab_strip()->group_header(group);
  std::u16string group_title = tab_strip_model()
                                   ->group_model()
                                   ->GetTabGroup(group)
                                   ->visual_data()
                                   ->title();

  EXPECT_EQ(group_title, group_header->GetTitleTextForTesting());
  EXPECT_EQ(
      group_header->GetRenderedTooltipText(gfx::Point()),
      l10n_util::GetStringFUTF16(
          IDS_TAB_GROUPS_NAMED_GROUP_TOOLTIP,
          std::u16string(group_header->GetTitleTextForTesting()),
          tab_strip()->GetGroupContentString(group_header->group().value())));

  tab_strip_model()->ChangeTabGroupVisuals(
      group, tab_groups::TabGroupVisualData(
                 std::u16string(), tab_groups::TabGroupColorId::kBlue));

  EXPECT_EQ(group_header->GetRenderedTooltipText(gfx::Point()),
            l10n_util::GetStringFUTF16(IDS_TAB_GROUPS_UNNAMED_GROUP_TOOLTIP,
                                       tab_strip()->GetGroupContentString(
                                           group_header->group().value())));
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       TabGroupHeaderTooltipTextAccessibility) {
  browser()->set_update_ui_immediately_for_testing();
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  tab_strip()->tab_at(1)->SetGroup(group);
  tab_strip_model()->ChangeTabGroupVisuals(
      group, tab_groups::TabGroupVisualData(
                 u"Non empty title text", tab_groups::TabGroupColorId::kBlue));

  auto* group_header = tab_strip()->group_header(group);
  std::u16string group_title = tab_strip_model()
                                   ->group_model()
                                   ->GetTabGroup(group)
                                   ->visual_data()
                                   ->title();

  EXPECT_EQ(group_title, group_header->GetTitleTextForTesting());

  EXPECT_EQ(
      group_header->GetRenderedTooltipText(gfx::Point()),
      l10n_util::GetStringFUTF16(
          IDS_TAB_GROUPS_NAMED_GROUP_TOOLTIP,
          std::u16string(group_header->GetTitleTextForTesting()),
          tab_strip()->GetGroupContentString(group_header->group().value())));

  ui::AXNodeData data;

  group_header->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_NE(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            group_header->GetRenderedTooltipText(gfx::Point()));
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            group_header->GetRenderedTooltipText(gfx::Point()));
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
  for (int i = 0; i < 4; i++) {
    AppendTab();
  }

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
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());

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

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, TabGroupHeaderAccessibleState) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  auto* group_header = tab_strip()->group_header(group);

  ui::AXNodeData data;
  group_header->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kEditable));
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ToggleTabSelection) {
  AppendTab();
  AppendTab();
  tab_strip()->SelectTab(tab_strip()->tab_at(0), GetDummyEvent());

  const ui::EventFlags modifier =
#if BUILDFLAG(IS_MAC)
      ui::EF_COMMAND_DOWN;
#else
      ui::EF_CONTROL_DOWN;
#endif
  ui::MouseEvent click(ui::EventType::kMousePressed, gfx::Point(0, 0),
                       gfx::Point(0, 0), ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON | modifier,
                       ui::EF_LEFT_MOUSE_BUTTON);
  tab_strip()->tab_at(1)->OnMousePressed(click);
  EXPECT_TRUE(tab_strip()->IsTabSelected(tab_strip()->tab_at(0)));
  EXPECT_TRUE(tab_strip()->IsTabSelected(tab_strip()->tab_at(1)));
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ExtendTabSelection) {
  AppendTab();
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip()->SelectTab(tab_strip()->tab_at(1), GetDummyEvent());

  ui::MouseEvent click(ui::EventType::kMousePressed, gfx::Point(0, 0),
                       gfx::Point(0, 0), ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_SHIFT_DOWN,
                       ui::EF_LEFT_MOUSE_BUTTON);
  tab_strip()->tab_at(3)->OnMousePressed(click);
  EXPECT_FALSE(tab_strip()->IsTabSelected(tab_strip()->tab_at(0)));
  EXPECT_TRUE(tab_strip()->IsTabSelected(tab_strip()->tab_at(1)));
  EXPECT_TRUE(tab_strip()->IsTabSelected(tab_strip()->tab_at(2)));
  EXPECT_TRUE(tab_strip()->IsTabSelected(tab_strip()->tab_at(3)));
}

class TabStripSaveBrowsertest : public TabStripBrowsertest {
 public:
  TabStripSaveBrowsertest() {
    scoped_feature_list_.InitWithFeatures(
        {data_sharing::features::kDataSharingFeature}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripSaveBrowsertest, AttentionIndicatorIsShown) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  tab_strip()->ToggleTabGroupCollapsedState(group);
  ASSERT_TRUE(tab_strip()->IsGroupCollapsed(group));

  auto* group_header = tab_strip()->group_header(group);

  group_header->SetTabGroupNeedsAttention(true);
  EXPECT_TRUE(group_header->attention_indicator_->GetVisible());

  group_header->SetTabGroupNeedsAttention(false);
  EXPECT_FALSE(group_header->attention_indicator_->GetVisible());
}
