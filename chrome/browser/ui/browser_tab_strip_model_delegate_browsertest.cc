// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"

#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace chrome {

using BrowserTabStripModelDelegateTest = InProcessBrowserTest;

class BrowserTabStripModelDelegateWithEmbeddedServerTest
    : public BrowserTabStripModelDelegateTest {
 public:
  void SetUpOnMainThread() override {
    BrowserTabStripModelDelegateTest::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromDirectory(
        base::FilePath{base::FilePath::kCurrentDirectory});
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("example.com", "127.0.0.1");
  }

  void ToggleMute(Browser* browser) {
    int active_index = browser->tab_strip_model()->active_index();
    browser->tab_strip_model()->ExecuteContextMenuCommand(
        active_index,
        TabStripModel::ContextMenuCommand::CommandToggleSiteMuted);
  }

  void VerifyMute(Browser* browser, bool isMuted) {
    int active_index = browser->tab_strip_model()->active_index();
    EXPECT_EQ(isMuted, IsSiteMuted(*browser->tab_strip_model(), active_index));
  }
};

// Tests the "Move Tab to New Window" tab context menu command.
IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateTest, MoveTabsToNewWindow) {
  std::unique_ptr<TabStripModelDelegate> delegate =
      std::make_unique<BrowserTabStripModelDelegate>(browser());

  GURL url1("chrome://version");
  GURL url2("chrome://about");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  // Moving a tab from a single tab window to a new tab window is a no-op.
  // TODO(lgrey): When moving to existing windows is implemented, add a case
  // for this test that asserts we *can* move to an existing window from a
  // single tab window.
  EXPECT_FALSE(delegate->CanMoveTabsToWindow({0}));

  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_LINK));

  EXPECT_TRUE(delegate->CanMoveTabsToWindow({0}));
  EXPECT_TRUE(delegate->CanMoveTabsToWindow({1}));
  // Moving *all* the tabs in a window to a new window is a no-op.
  EXPECT_FALSE(delegate->CanMoveTabsToWindow({0, 1}));

  // Precondition: there's currently one browser with two tabs.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);

  // Execute this on a background tab to ensure that the code path can handle
  // other tabs besides the active one.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  delegate->MoveTabsToNewWindow({0});
  Browser* active_browser = browser_created_observer.Wait();
  ui_test_utils::WaitUntilBrowserBecomeActive(active_browser);

  // Now there are two browsers, each with one tab and the new browser is
  // active.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 2u);
  EXPECT_NE(active_browser, browser());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(active_browser->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);
  EXPECT_EQ(active_browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url1);
}

// Tests the "Move Tab to New Window" tab context menu command with multiple
// tabs selected.
IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateTest,
                       MoveMultipleTabsToNewWindow) {
  std::unique_ptr<TabStripModelDelegate> delegate =
      std::make_unique<BrowserTabStripModelDelegate>(browser());

  GURL url1("chrome://version");
  GURL url2("chrome://about");
  GURL url3("chrome://terms");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  // Moving a tab from a single tab window to a new tab window is a no-op.
  // TODO(jugallag): When moving to existing windows is implemented, add a case
  // for this test that asserts we *can* move to an existing window from a
  // single tab window.
  EXPECT_FALSE(delegate->CanMoveTabsToWindow({0}));

  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_LINK));
  ASSERT_TRUE(AddTabAtIndex(2, url3, ui::PAGE_TRANSITION_LINK));

  EXPECT_TRUE(delegate->CanMoveTabsToWindow({0}));
  EXPECT_TRUE(delegate->CanMoveTabsToWindow({1}));
  EXPECT_TRUE(delegate->CanMoveTabsToWindow({2}));
  EXPECT_TRUE(delegate->CanMoveTabsToWindow({0, 1}));
  EXPECT_TRUE(delegate->CanMoveTabsToWindow({0, 2}));
  EXPECT_TRUE(delegate->CanMoveTabsToWindow({1, 2}));
  // Moving *all* the tabs in a window to a new window is a no-op.
  EXPECT_FALSE(delegate->CanMoveTabsToWindow({0, 1, 2}));

  // Precondition: there's currently one browser with three tabs.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url3);

  // Execute this on a background tab to ensure that the code path can handle
  // other tabs besides the active one.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  delegate->MoveTabsToNewWindow({0, 2});
  BrowserWindowInterface* const active_browser =
      browser_created_observer.Wait();
  ui_test_utils::WaitUntilBrowserBecomeActive(active_browser);

  // Now there are two browsers, with one or two tabs and the new browser is
  // active.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 2u);
  EXPECT_NE(active_browser, browser());
  EXPECT_EQ(browser()->GetTabStripModel()->count(), 1);
  EXPECT_EQ(active_browser->GetTabStripModel()->count(), 2);
  EXPECT_EQ(browser()->GetTabStripModel()->GetActiveWebContents()->GetURL(),
            url2);
  EXPECT_EQ(
      active_browser->GetTabStripModel()->GetActiveWebContents()->GetURL(),
      url3);
}

// Test muting tab in regular window is resettable in Incognito window.
IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateWithEmbeddedServerTest,
                       ToggleMuteInRegularAndThenToggleMuteInIncognito) {
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Mute the site in regular tab.
  ToggleMute(browser());
  VerifyMute(browser(), /*isMuted=*/true);

  // Open Incognito tab and check the site is muted there.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url));
  VerifyMute(incognito_browser, /*isMuted=*/true);

  // Unmute in Incognito tab.
  ToggleMute(incognito_browser);
  VerifyMute(incognito_browser, /*isMuted=*/false);

  // In regular tab the site should still be muted.
  VerifyMute(browser(), /*isMuted=*/true);
}

// Test muting/unmuting tab from regular window is inherited properly in
// Incognito window.
IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateWithEmbeddedServerTest,
                       ToggleMuteInRegularWindowAndCheckInIncognito) {
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Mute the site in regular tab.
  ToggleMute(browser());
  VerifyMute(browser(), /*isMuted=*/true);

  // Open Incognito tab and check the site is muted there.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url));
  VerifyMute(incognito_browser, /*isMuted=*/true);

  // Unmute in Regular tab.
  ToggleMute(browser());
  VerifyMute(browser(), /*isMuted=*/false);

  // Site should also unmute in Incognito tab.
  VerifyMute(incognito_browser, /*isMuted=*/false);
}

IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateWithEmbeddedServerTest,
                       ToggleMuteOnlyInIncognitoWindow) {
  GURL url = embedded_test_server()->GetURL("/title1.html");

  // Open tab in Incognito
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url));

  // Mute the site in Incognito.
  ToggleMute(incognito_browser);

  // The site should be muted in Incognito.
  VerifyMute(incognito_browser, /*isMuted=*/true);

  // Unmute the site in Incognito.
  ToggleMute(incognito_browser);

  // The site should be unmuted in Incognito.
  VerifyMute(incognito_browser, /*isMuted=*/false);
}

// Tests that bulk actions will close tab groups without destruction.
IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateTest,
                       BulkCloseToRightWithTabGroups) {
  std::unique_ptr<TabStripModelDelegate> delegate =
      std::make_unique<BrowserTabStripModelDelegate>(browser());

  GURL url1("chrome://version");
  GURL url2("chrome://about");
  GURL url3("chrome://terms");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_LINK));
  ASSERT_TRUE(AddTabAtIndex(2, url3, ui::PAGE_TRANSITION_LINK));

  auto* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->AddToNewGroup({1});
  tab_strip_model->AddToNewGroup({2});

  // Precondition: there's currently one browser with three tabs in two
  // groups.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url3);
  EXPECT_EQ(browser()->tab_strip_model()->group_model()->ListTabGroups().size(),
            2u);

  auto* sync_service = tab_groups::TabGroupSyncServiceFactory::GetForProfile(
      browser()->profile());
  EXPECT_NE(sync_service, nullptr);

  auto groups = sync_service->GetAllGroups();
  EXPECT_EQ(groups.size(), 2u);
  for (auto group : groups) {
    // Group is open
    EXPECT_TRUE(group.local_group_id().has_value());
  }

  // Execute command on first tab to close all tabs to the right.
  tab_strip_model->ExecuteContextMenuCommand(
      0, TabStripModel::ContextMenuCommand::CommandCloseTabsToRight);

  // Now there are is only one tab and groups are minimized.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url1);
  EXPECT_EQ(browser()->tab_strip_model()->group_model()->ListTabGroups().size(),
            0u);

  groups = sync_service->GetAllGroups();
  EXPECT_EQ(groups.size(), 2u);
  for (auto group : groups) {
    // Group is not open
    EXPECT_FALSE(group.local_group_id().has_value());
  }
}

IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateTest,
                       BulkCloseOtherWithTabGroups) {
  std::unique_ptr<TabStripModelDelegate> delegate =
      std::make_unique<BrowserTabStripModelDelegate>(browser());

  GURL url1("chrome://version");
  GURL url2("chrome://about");
  GURL url3("chrome://terms");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_LINK));
  ASSERT_TRUE(AddTabAtIndex(2, url3, ui::PAGE_TRANSITION_LINK));

  auto* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->AddToNewGroup({1});
  tab_strip_model->AddToNewGroup({2});

  // Precondition: there's currently one browser with three tabs in two
  // groups.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url3);
  EXPECT_EQ(browser()->tab_strip_model()->group_model()->ListTabGroups().size(),
            2u);

  auto* sync_service = tab_groups::TabGroupSyncServiceFactory::GetForProfile(
      browser()->profile());
  EXPECT_NE(sync_service, nullptr);

  auto groups = sync_service->GetAllGroups();
  EXPECT_EQ(groups.size(), 2u);
  for (auto group : groups) {
    // Group is open
    EXPECT_TRUE(group.local_group_id().has_value());
  }

  // Execute command on first tab to close all tabs to the right.
  tab_strip_model->ExecuteContextMenuCommand(
      1, TabStripModel::ContextMenuCommand::CommandCloseOtherTabs);

  // Now there are is only one tab and groups are minimized.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);
  EXPECT_EQ(browser()->tab_strip_model()->group_model()->ListTabGroups().size(),
            1u);

  groups = sync_service->GetAllGroups();
  EXPECT_EQ(groups.size(), 2u);
  // Other group is not open
  EXPECT_FALSE(groups.at(0).local_group_id().has_value());
  // Current group is open
  EXPECT_TRUE(groups.at(1).local_group_id().has_value());
}

class BrowserTabStripModelDelegateWithSideBySide
    : public BrowserTabStripModelDelegateTest {
 public:
  BrowserTabStripModelDelegateWithSideBySide() {
    feature_list_.InitWithFeatures({features::kSideBySide}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateWithSideBySide,
                       NewSplitTabWithActiveTabPinned) {
  std::unique_ptr<TabStripModelDelegate> delegate =
      std::make_unique<BrowserTabStripModelDelegate>(browser());

  GURL url1("chrome://about");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  browser()->tab_strip_model()->SetTabPinned(0, true);
  delegate->NewSplitTab({}, split_tabs::SplitTabCreatedSource::kToolbarButton);

  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabPinned(0));
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabPinned(1));
}

IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateWithSideBySide,
                       NewSplitTabWithActiveTabGroupped) {
  std::unique_ptr<TabStripModelDelegate> delegate =
      std::make_unique<BrowserTabStripModelDelegate>(browser());

  GURL url1("chrome://about");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});
  delegate->NewSplitTab({}, split_tabs::SplitTabCreatedSource::kToolbarButton);

  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->GetTabGroupForTab(0).value(),
            group_id);
  ASSERT_EQ(browser()->tab_strip_model()->GetTabGroupForTab(1).value(),
            group_id);
}

IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateWithSideBySide,
                       NewSplitTabFromIncognito) {
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());

  std::unique_ptr<TabStripModelDelegate> delegate =
      std::make_unique<BrowserTabStripModelDelegate>(incognito_browser);

  GURL url1("chrome://about");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url1));

  delegate->NewSplitTab({}, split_tabs::SplitTabCreatedSource::kToolbarButton);

  ASSERT_EQ(incognito_browser->tab_strip_model()->count(), 2);
  ASSERT_EQ(incognito_browser->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            chrome::kChromeUINewTabURL);
}

IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateWithSideBySide,
                       DuplicateSplitTab) {
  std::unique_ptr<TabStripModelDelegate> delegate =
      std::make_unique<BrowserTabStripModelDelegate>(browser());

  GURL url1("chrome://about");
  GURL url2("chrome://version");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_LINK));

  delegate->NewSplitTab({0}, split_tabs::SplitTabCreatedSource::kToolbarButton);

  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  std::optional<split_tabs::SplitTabId> split_id1 =
      browser()->tab_strip_model()->GetSplitForTab(0);
  ASSERT_TRUE(split_id1.has_value());
  ASSERT_EQ(browser()->tab_strip_model()->GetSplitForTab(1).value(),
            split_id1.value());

  delegate->DuplicateSplit(split_id1.value());

  ASSERT_EQ(browser()->tab_strip_model()->count(), 4);
  std::optional<split_tabs::SplitTabId> split_id2 =
      browser()->tab_strip_model()->GetSplitForTab(2);
  ASSERT_TRUE(split_id2.has_value());
  ASSERT_EQ(browser()->tab_strip_model()->GetSplitForTab(3).value(),
            split_id2.value());
}

}  // namespace chrome
