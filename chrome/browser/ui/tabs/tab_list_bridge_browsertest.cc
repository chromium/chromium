// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_list_bridge.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

// TODO(devlin): Would it make sense to port this to instead be a
// TabListInterface browsertest, and use it on all relevant platforms?
using TabListBridgeBrowserTest = InProcessBrowserTest;

// A helpful matcher for tabs having an expected URL. Since we assume the
// TabInterface works, this is sufficient to meaningfully describe tabs in
// expectations.
MATCHER_P(MatchesTab, expected_url, "") {
  const GURL& actual_url = arg->GetContents()->GetLastCommittedURL();
  bool match = testing::ExplainMatchResult(testing::Eq(expected_url),
                                           actual_url, result_listener);
  if (!match) {
    *result_listener << " Actual URL: " << actual_url;
  }
  return match;
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetTab) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  tabs::TabInterface* tab1 = tab_list_interface->GetTab(0);
  ASSERT_TRUE(tab1);
  EXPECT_THAT(tab1, MatchesTab(url1));

  tabs::TabInterface* tab2 = tab_list_interface->GetTab(1);
  ASSERT_TRUE(tab2);
  EXPECT_THAT(tab2, MatchesTab(url2));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetActiveIndex) {
  const GURL url("http://one.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  EXPECT_EQ(0, tab_list_interface->GetActiveIndex());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ(1, tab_list_interface->GetActiveIndex());
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetTabCount) {
  const GURL url("http://one.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  EXPECT_EQ(1, tab_list_interface->GetTabCount());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ(2, tab_list_interface->GetTabCount());
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetAllTabs) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");
  const GURL url3("http://three.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Navigate to one.example. This should be the only tab, initially.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_THAT(tab_list_interface->GetAllTabs(),
              testing::ElementsAre(MatchesTab(url1)));

  // Open two more tabs, for a total of three. All should be returned (in
  // order).
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  EXPECT_THAT(tab_list_interface->GetAllTabs(),
              testing::ElementsAre(MatchesTab(url1), MatchesTab(url2),
                                   MatchesTab(url3)));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetActiveTab) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");
  const GURL url3("http://three.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Navigate to one.example. This should be the only tab, initially.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_THAT(tab_list_interface->GetActiveTab(), MatchesTab(url1));

  // Open a new tab in the background. The active tab should be unchanged.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_THAT(tab_list_interface->GetActiveTab(), MatchesTab(url1));

  // Open a new tab in the foreground. Now, the active tab should be the new
  // tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_THAT(tab_list_interface->GetActiveTab(), MatchesTab(url3));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, PinAndUnpin) {
  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  tabs::TabInterface* tab = tab_list_interface->GetActiveTab();
  ASSERT_TRUE(tab);

  EXPECT_FALSE(tab->IsPinned());

  tab_list_interface->PinTab(tab->GetHandle());
  EXPECT_TRUE(tab->IsPinned());

  tab_list_interface->UnpinTab(tab->GetHandle());
  EXPECT_FALSE(tab->IsPinned());
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetIndexOfTab) {
  const GURL url("http://example.com");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  tabs::TabInterface* tab0 = tab_list_interface->GetActiveTab();

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* tab1 = tab_list_interface->GetActiveTab();

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* tab2 = tab_list_interface->GetActiveTab();

  EXPECT_EQ(0, tab_list_interface->GetIndexOfTab(tab0->GetHandle()));
  EXPECT_EQ(1, tab_list_interface->GetIndexOfTab(tab1->GetHandle()));
  EXPECT_EQ(2, tab_list_interface->GetIndexOfTab(tab2->GetHandle()));

  Browser* new_browser = CreateBrowser(browser()->profile());
  TabListInterface* new_tab_list_interface = TabListBridge::From(new_browser);
  ASSERT_TRUE(new_tab_list_interface);

  tabs::TabInterface* new_tab = new_tab_list_interface->GetActiveTab();

  EXPECT_EQ(-1, tab_list_interface->GetIndexOfTab(new_tab->GetHandle()));
  EXPECT_EQ(-1, new_tab_list_interface->GetIndexOfTab(tab0->GetHandle()));
}
