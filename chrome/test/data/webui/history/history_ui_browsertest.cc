// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class HistoryUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  HistoryUIBrowserTest() { set_test_loader_host(chrome::kChromeUIHistoryHost); }
};

typedef HistoryUIBrowserTest HistoryDrawerTest;
IN_PROC_BROWSER_TEST_F(HistoryDrawerTest, All) {
  RunTest("history/history_drawer_test.js", "mocha.run()");
}

typedef HistoryUIBrowserTest HistoryItemTest;
IN_PROC_BROWSER_TEST_F(HistoryItemTest, All) {
  RunTest("history/history_item_test.js", "mocha.run()");
}

typedef HistoryUIBrowserTest HistoryLinkClickTest;
IN_PROC_BROWSER_TEST_F(HistoryLinkClickTest, All) {
  RunTest("history/link_click_test.js", "mocha.run()");
}

class HistoryListTest : public HistoryUIBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    HistoryUIBrowserTest::RunTest(
        "history/history_list_test.js",
        base::StringPrintf("runMochaTest('HistoryListTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(HistoryListTest, DeletingSingleItem) {
  RunTestCase("DeletingSingleItem");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest, CancellingSelectionOfMultipleItems) {
  RunTestCase("CancellingSelectionOfMultipleItems");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest,
                       SelectionOfMultipleItemsUsingShiftClick) {
  RunTestCase("SelectionOfMultipleItemsUsingShiftClick");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest, DisablingCtrlAOnSyncedTabsPage) {
  RunTestCase("DisablingCtrlAOnSyncedTabsPage");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest, SettingFirstAndLastItems) {
  RunTestCase("SettingFirstAndLastItems");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest, UpdatingHistoryResults) {
  RunTestCase("UpdatingHistoryResults");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest, DeletingMultipleItemsFromView) {
  RunTestCase("DeletingMultipleItemsFromView");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest,
                       SearchResultsDisplayWithCorrectItemTitle) {
  RunTestCase("SearchResultsDisplayWithCorrectItemTitle");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest,
                       CorrectDisplayMessageWhenNoHistoryAvailable) {
  RunTestCase("CorrectDisplayMessageWhenNoHistoryAvailable");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest,
                       MoreFromThisSiteSendsAndSetsCorrectData) {
  RunTestCase("MoreFromThisSiteSendsAndSetsCorrectData");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest, ChangingSearchDeselectsItems) {
  RunTestCase("ChangingSearchDeselectsItems");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest, DeleteItemsEndToEnd) {
  RunTestCase("DeleteItemsEndToEnd");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest, DeleteViaMenuButton) {
  RunTestCase("DeleteViaMenuButton");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest, DeleteDisabledWhilePending) {
  RunTestCase("DeleteDisabledWhilePending");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest, DeletingItemsUsingShortcuts) {
  RunTestCase("DeletingItemsUsingShortcuts");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest, DeleteDialogClosedOnBackNavigation) {
  RunTestCase("DeleteDialogClosedOnBackNavigation");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest, ClickingFileUrlSendsMessageToChrome) {
  RunTestCase("ClickingFileUrlSendsMessageToChrome");
}

IN_PROC_BROWSER_TEST_F(HistoryListTest,
                       DeleteHistoryResultsInQueryHistoryEvent) {
  RunTestCase("DeleteHistoryResultsInQueryHistoryEvent");
}

typedef HistoryUIBrowserTest HistoryMetricsTest;
IN_PROC_BROWSER_TEST_F(HistoryMetricsTest, All) {
  RunTest("history/history_metrics_test.js", "mocha.run()");
}

typedef HistoryUIBrowserTest HistoryOverflowMenuTest;
IN_PROC_BROWSER_TEST_F(HistoryOverflowMenuTest, All) {
  RunTest("history/history_overflow_menu_test.js", "mocha.run()");
}

typedef HistoryUIBrowserTest HistoryRoutingTest;
IN_PROC_BROWSER_TEST_F(HistoryRoutingTest, All) {
  RunTest("history/history_routing_test.js", "mocha.run()");
}

typedef HistoryUIBrowserTest HistoryRoutingWithQueryParamTest;
IN_PROC_BROWSER_TEST_F(HistoryRoutingWithQueryParamTest, All) {
  RunTest("history/history_routing_with_query_param_test.js", "mocha.run()");
}

typedef HistoryUIBrowserTest HistorySyncedTabsTest;
IN_PROC_BROWSER_TEST_F(HistorySyncedTabsTest, All) {
  RunTest("history/history_synced_tabs_test.js", "mocha.run()");
}

typedef HistoryUIBrowserTest HistoryToolbarTest;
IN_PROC_BROWSER_TEST_F(HistoryToolbarTest, All) {
  RunTest("history/history_toolbar_test.js", "mocha.run()");
}

typedef HistoryUIBrowserTest HistorySearchedLabelTest;
IN_PROC_BROWSER_TEST_F(HistorySearchedLabelTest, All) {
  RunTest("history/searched_label_test.js", "mocha.run()");
}
