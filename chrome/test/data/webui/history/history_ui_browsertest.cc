// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/history_clusters/core/features.h"
#include "content/public/test/browser_test.h"

class HistoryUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  HistoryUIBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        history_clusters::kRenameJourneys);
    set_test_loader_host(chrome::kChromeUIHistoryHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using HistoryTest = HistoryUIBrowserTest;

IN_PROC_BROWSER_TEST_F(HistoryTest, Drawer) {
  RunTest("history/history_drawer_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(HistoryTest, Item) {
  RunTest("history/history_item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(HistoryTest, LinkClick) {
  RunTest("history/link_click_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(HistoryTest, Metrics) {
  RunTest("history/history_metrics_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(HistoryTest, OverflowMenu) {
  RunTest("history/history_overflow_menu_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(HistoryTest, Routing) {
  RunTest("history/history_routing_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(HistoryTest, RoutingWithQueryParam) {
  RunTest("history/history_routing_with_query_param_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(HistoryTest, SyncedTabs) {
  RunTest("history/history_synced_tabs_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(HistoryTest, Toolbar) {
  RunTest("history/history_toolbar_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(HistoryTest, SearchedLabel) {
  RunTest("history/searched_label_test.js", "mocha.run()");
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
