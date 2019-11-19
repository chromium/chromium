// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "url/gurl.h"

// Integration tests for interactions between TabStripModel and TabStrip.
class TabStripBrowsertest : public InProcessBrowserTest {
 public:
  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  TabStrip* tab_strip() {
    return BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  }

  void AppendTab() { chrome::AddTabAt(browser(), GURL(), -1, true); }

  TabGroupId AddTabToNewGroup(int tab_index) {
    tab_strip_model()->AddToNewGroup({tab_index});
    return tab_strip_model()->GetTabGroupForTab(tab_index).value();
  }

  std::vector<content::WebContents*> GetWebContentses() {
    std::vector<content::WebContents*> contentses;
    for (int i = 0; i < tab_strip()->tab_count(); ++i)
      contentses.push_back(tab_strip_model()->GetWebContentsAt(i));
    return contentses;
  }

  std::vector<content::WebContents*> GetWebContentsesInOrder(
      const std::vector<int>& order) {
    std::vector<content::WebContents*> contentses;
    for (int i = 0; i < tab_strip()->tab_count(); ++i)
      contentses.push_back(tab_strip_model()->GetWebContentsAt(order[i]));
    return contentses;
  }
};

// Regression test for crbug.com/983961.
IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabAndDeleteGroup) {
  AppendTab();
  AppendTab();

  TabGroupId group = AddTabToNewGroup(0);
  AddTabToNewGroup(2);

  Tab* tab0 = tab_strip()->tab_at(0);
  Tab* tab1 = tab_strip()->tab_at(1);
  Tab* tab2 = tab_strip()->tab_at(2);

  tab_strip_model()->AddToExistingGroup({2}, group);

  EXPECT_EQ(tab0, tab_strip()->tab_at(0));
  EXPECT_EQ(tab2, tab_strip()->tab_at(1));
  EXPECT_EQ(tab1, tab_strip()->tab_at(2));

  EXPECT_EQ(group, tab_strip_model()->GetTabGroupForTab(1));

  std::vector<TabGroupId> groups = tab_strip_model()->ListTabGroups();
  EXPECT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0], group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLeft_Success) {
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({1, 0, 2});
  tab_strip()->MoveTabLeft(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       MoveTabLeft_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabLeft(tab_strip()->tab_at(0));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLeft_Failure_Pinned) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabLeft(tab_strip()->tab_at(1));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabRight_Success) {
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({1, 0, 2});
  tab_strip()->MoveTabRight(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       MoveTabRight_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabRight(tab_strip()->tab_at(2));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabRight_Failure_Pinned) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabRight(tab_strip()->tab_at(0));
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
