// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"

#include "base/path_service.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/webui/commerce/product_specifications_disclosure_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/commerce/core/mojom/product_specifications.mojom.h"
#include "components/commerce/core/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/ui_base_features.h"

namespace chrome {

class BrowserCommandsTest : public InProcessBrowserTest {
 public:
  BrowserCommandsTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatures(
        {
            features::kTabOrganization,
            features::kTabstripDeclutter,
            toast_features::kReadingListToast,
            toast_features::kLinkCopiedToast,
            features::kSideBySide,
        },
        {
            features::kReloadSelectionModel,
        });
  }

  base::test::ScopedFeatureList feature_list_;
  net::test_server::EmbeddedTestServer https_server_;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  static constexpr char kUrl[] = "chrome://version/";

  void AddTabs(Browser* browser, int num_tabs) {
    for (int i = 0; i < num_tabs; ++i) {
      chrome::NewTab(browser);
    }
  }

  void AddTabs(int num_tabs) { AddTabs(browser(), num_tabs); }

  void AddAndReloadTabs(int tab_count) {
    AddTabs(tab_count);

    // Add tabs to the selection (the last one created remains selected) and
    // trigger a reload command on all of them.
    for (int i = 0; i < tab_count - 1; ++i) {
      browser()->tab_strip_model()->SelectTabAt(i + 1);
    }
    EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
    browser()->tab_strip_model()->CloseSelectedTabs();
  }

  void SetThirdPartyCookieBlocking(bool enabled) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            enabled ? content_settings::CookieControlsMode::kBlockThirdParty
                    : content_settings::CookieControlsMode::kOff));
  }

  void CheckReloadBreakageMetrics(ukm::TestAutoSetUkmRecorder& ukm_recorder,
                                  size_t size,
                                  size_t index,
                                  bool blocked,
                                  bool settings_blocked) {
    auto entries = ukm_recorder.GetEntries(
        "ThirdPartyCookies.BreakageIndicator.UserReload",
        {"TPCBlocked", "TPCBlockedInSettings"});
    EXPECT_EQ(entries.size(), size);
    EXPECT_EQ(entries.at(index).metrics.at("TPCBlocked"), blocked);
    EXPECT_EQ(entries.at(index).metrics.at("TPCBlockedInSettings"),
              settings_blocked);
  }

  void CheckBrowserContainsTabGroupWithSize(
      const BrowserWindowInterface* browser,
      tab_groups::TabGroupId group_id,
      int size) {
    EXPECT_TRUE(
        browser->GetTabStripModel()->group_model()->ContainsTabGroup(group_id));
    EXPECT_EQ(size, browser->GetTabStripModel()
                        ->group_model()
                        ->GetTabGroup(group_id)
                        ->ListTabs()
                        .length());
  }

  class ReloadObserver : public content::WebContentsObserver {
   public:
    ~ReloadObserver() override = default;

    int load_count() const { return load_count_; }
    void SetWebContents(content::WebContents* web_contents) {
      Observe(web_contents);
    }

    // content::WebContentsObserver
    void DidStartLoading() override { load_count_++; }

   private:
    int load_count_ = 0;
  };
};

// Verify that calling BookmarkCurrentTab() just after closing all tabs doesn't
// cause a crash. https://crbug.com/799668
IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, BookmarkCurrentTabAfterCloseTabs) {
  browser()->tab_strip_model()->CloseAllTabs();
  BookmarkCurrentTab(browser());
}

// Verify that all of selected tabs are refreshed after executing a reload
// command. https://crbug.com/862102
IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, ReloadSelectedTabs) {
  constexpr int kTabCount = 3;
  std::vector<ReloadObserver> watcher_vec(kTabCount);
  for (int i = 0; i < kTabCount; i++) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), i + 1, GURL(kUrl),
                                       ui::PAGE_TRANSITION_LINK, false));
    content::WebContents* tab =
        browser()->tab_strip_model()->GetWebContentsAt(i + 1);
    watcher_vec[i].SetWebContents(tab);
  }

  for (ReloadObserver& watcher : watcher_vec) {
    EXPECT_EQ(0, watcher.load_count());
  }

  // Add two tabs to the selection (the last one created remains selected) and
  // trigger a reload command on all of them.
  for (int i = 0; i < kTabCount - 1; i++) {
    browser()->tab_strip_model()->SelectTabAt(i + 1);
  }
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));

  int load_sum = 0;
  for (ReloadObserver& watcher : watcher_vec) {
    load_sum += watcher.load_count();
  }
  EXPECT_EQ(kTabCount, load_sum);
}

class BrowserCommandsWithReloadSelectionModelTest : public BrowserCommandsTest {
 public:
  BrowserCommandsWithReloadSelectionModelTest() {
    feature_list_.InitWithFeatures(
        {
            features::kReloadSelectionModel,
        },
        {});
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserCommandsWithReloadSelectionModelTest,
                       ReloadSelectedTabs) {
  // Add 5 tabs.
  constexpr int kTabCount = 5;
  for (int i = 0; i < kTabCount; i++) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), i + 1, GURL(kUrl),
                                       ui::PAGE_TRANSITION_LINK, false));
  }
  // Helper to stop all tabs. Otherwise, reloading an already-loading tab won't
  // cause a new load.
  auto stop_all_tabs = [&]() {
    for (int i = 0; i < kTabCount; i++) {
      content::WebContents* tab =
          browser()->tab_strip_model()->GetWebContentsAt(i + 1);
      tab->Stop();
    }
  };

  // Create a split tab.
  browser()->tab_strip_model()->AddToNewSplit(
      {3},
      split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kVertical,
                                     1.0f),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  // Track how many times each tab is reloaded.
  std::vector<ReloadObserver> reload_observers(kTabCount);
  for (int i = 0; i < kTabCount; i++) {
    content::WebContents* tab =
        browser()->tab_strip_model()->GetWebContentsAt(i + 1);
    reload_observers[i].SetWebContents(tab);
  }
  // Helper to get reload counts as a vector to use in `EXPECT_THAT`.
  auto get_reloads = [&]() {
    std::vector<int> reloads;
    for (ReloadObserver& watcher : reload_observers) {
      reloads.push_back(watcher.load_count());
    }
    return reloads;
  };

  // The split tab should be active.
  EXPECT_THAT(
      browser()->tab_strip_model()->selection_model().selected_indices(),
      testing::ElementsAre(4, 5));
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 5);
  EXPECT_THAT(get_reloads(), testing::ElementsAre(0, 0, 0, 0, 0));

  // Reload with only the split tab selected. Only the active view should
  // reload.
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  EXPECT_THAT(get_reloads(), testing::ElementsAre(0, 0, 0, 0, 1));
  stop_all_tabs();

  // Select 1st tab.
  browser()->tab_strip_model()->SelectTabAt(1);
  EXPECT_THAT(
      browser()->tab_strip_model()->selection_model().selected_indices(),
      testing::ElementsAre(1, 4, 5));
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);

  // Reload with the split tab selected but not active. All selected tabs should
  // reload.
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  EXPECT_THAT(get_reloads(), testing::ElementsAre(1, 0, 0, 1, 2));
  stop_all_tabs();

  // Activate the split tab.
  browser()->tab_strip_model()->SelectTabAt(5);
  EXPECT_THAT(
      browser()->tab_strip_model()->selection_model().selected_indices(),
      testing::ElementsAre(1, 4, 5));
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 5);

  // Reload with the split 4|5 tab selected and active. All selected tabs should
  // reload.
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  EXPECT_THAT(get_reloads(), testing::ElementsAre(2, 0, 0, 2, 3));
  stop_all_tabs();
}

class BrowserCommandsWithCloseHotkeySplitViewTest : public BrowserCommandsTest {
 public:
  BrowserCommandsWithCloseHotkeySplitViewTest() {
    feature_list_.InitWithFeatures(
        {
            features::kCloseActiveTabInSplitViewViaHotkey,
            features::kSideBySide,
        },
        {});
  }

 protected:
  TabStripModel* GetTabStripModel(Browser* browser) {
    return browser->tab_strip_model();
  }

  base::test::ScopedFeatureList feature_list_;
};

// With kCloseHotkeySplitView enabled, only the active tab in the split view is
// closed.
IN_PROC_BROWSER_TEST_F(BrowserCommandsWithCloseHotkeySplitViewTest,
                       OnlyCloseActiveTabInSplitView) {
  // Add 2 tabs.
  constexpr int kTabCount = 3;
  for (int i = 1; i < kTabCount; i++) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), i, GURL(kUrl),
                                       ui::PAGE_TRANSITION_LINK, false));
  }

  EXPECT_EQ(kTabCount, GetTabStripModel(browser())->GetTabCount());

  // Add second last tab to split view with the last tab.
  GetTabStripModel(browser())->AddToNewSplit(
      {kTabCount - 2},
      split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kVertical,
                                     1.0f),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  EXPECT_TRUE(GetTabStripModel(browser())->IsActiveTabSplit());

  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_CLOSE_TAB));

  EXPECT_FALSE(GetTabStripModel(browser())->IsActiveTabSplit());
  EXPECT_EQ(2, GetTabStripModel(browser())->GetTabCount());
}

// All tabs in the selection model get closed.
IN_PROC_BROWSER_TEST_F(BrowserCommandsWithCloseHotkeySplitViewTest,
                       CloseAllTabsInSelectionModel) {
  // Add 4 tabs.
  constexpr int kTabCount = 4;
  for (int i = 1; i < kTabCount; i++) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), i, GURL(kUrl),
                                       ui::PAGE_TRANSITION_LINK, false));
  }

  EXPECT_EQ(kTabCount, GetTabStripModel(browser())->GetTabCount());

  // Add second last tab to split view with the last tab.
  GetTabStripModel(browser())->AddToNewSplit(
      {kTabCount - 2},
      split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kVertical,
                                     1.0f),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  EXPECT_TRUE(GetTabStripModel(browser())->IsActiveTabSplit());

  // Add a non-split tab to the selection model.
  GetTabStripModel(browser())->SelectTabAt(kTabCount - 3);

  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_CLOSE_TAB));

  // Only one, non-split tab should remain.
  EXPECT_FALSE(GetTabStripModel(browser())->IsActiveTabSplit());
  EXPECT_EQ(1, GetTabStripModel(browser())->GetTabCount());
}

// Check that the ThirdPartyCookieBreakageIndicator UKM is sent on Reload.
// Disabled because of crbug.com/1468528
IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, DISABLED_ReloadBreakageUKM) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();

  // Test simple reload measurements without 3PCB.
  SetThirdPartyCookieBlocking(false);
  EXPECT_FALSE(settings->ShouldBlockThirdPartyCookies());

  AddAndReloadTabs(1);
  CheckReloadBreakageMetrics(ukm_recorder, 1, 0, false, false);

  AddAndReloadTabs(1);
  CheckReloadBreakageMetrics(ukm_recorder, 2, 1, false, false);

  // Test that enabled 3PCB is correctly reflected in the metrics.
  SetThirdPartyCookieBlocking(true);
  EXPECT_TRUE(settings->ShouldBlockThirdPartyCookies());

  AddAndReloadTabs(1);
  CheckReloadBreakageMetrics(ukm_recorder, 3, 2, false, true);

  // Test that allow-listing is correctly reflected in the metrics.
  GURL origin(kUrl);
  settings->SetThirdPartyCookieSetting(origin,
                                       ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(settings->IsThirdPartyAccessAllowed(origin, nullptr));

  AddAndReloadTabs(1);
  CheckReloadBreakageMetrics(ukm_recorder, 4, 3, false, false);

  // Reload multiple tabs, all reloads are counted.
  AddAndReloadTabs(3);
  CheckReloadBreakageMetrics(ukm_recorder, 7, 4, false, false);
  CheckReloadBreakageMetrics(ukm_recorder, 7, 5, false, false);
  CheckReloadBreakageMetrics(ukm_recorder, 7, 6, false, false);

  // Load a page with an iframe and try to set a cross-site cookie inside of
  // that iframe.
  constexpr char host_a[] = "a.test";
  constexpr char host_b[] = "b.test";
  GURL main_url(https_server_.GetURL(host_a, "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  GURL page = https_server_.GetURL(
      host_b, "/set-cookie?thirdparty=1;SameSite=None;Secure");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", page));

  // Reload the page with the cross-site iframe.
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));

  // We should now observe a 3P cookie *actually* blocked.
  CheckReloadBreakageMetrics(ukm_recorder, 8, 7, true, true);
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, MoveTabsToNewWindow) {
  // Single Tab Move to New Window.
  // 1 (Current) + 1 (Added) = 2
  AddTabs(1);
  std::vector<int> indices = {0};
  // 2 (Current) - 1 (Moved) = 1
  auto browser_created_observer =
      std::make_optional<ui_test_utils::BrowserCreatedObserver>();
  chrome::MoveTabsToNewWindow(browser(), indices);
  const BrowserWindowInterface* const second_browser =
      browser_created_observer->Wait();
  ASSERT_TRUE(browser()->GetTabStripModel()->count() == 1);

  // Multi-Tab Move to New Window.
  // 1 (Current) + 3 (Added) = 4
  AddTabs(3);
  indices = {0, 1};
  // 4 (Current) - 2 (Moved) = 2
  browser_created_observer.emplace();
  chrome::MoveTabsToNewWindow(browser(), indices);
  const BrowserWindowInterface* const third_browser =
      browser_created_observer->Wait();
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());

  // Check that the two additional windows have been created.
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());

  // Check that the tabs made it to other windows.
  EXPECT_EQ(1, second_browser->GetTabStripModel()->count());
  EXPECT_EQ(2, third_browser->GetTabStripModel()->count());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, MoveTabsToNewWindow_WithGroup) {
  // Three tabs with first two in a group
  AddTabs(2);
  std::vector<int> indices = {1, 2};
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup(indices);
  browser()->tab_strip_model()->ChangeTabGroupVisuals(
      group_id, tab_groups::TabGroupVisualData(
                    u"Test Group", tab_groups::TabGroupColorId::kGrey));

  // Move both tabs in the group to a new window.
  auto browser_created_observer =
      std::make_optional<ui_test_utils::BrowserCreatedObserver>();
  chrome::MoveTabsToNewWindow(browser(), indices);
  const BrowserWindowInterface* const second_browser =
      browser_created_observer->Wait();

  // Original browser has one tab and no group.
  ASSERT_EQ(1, browser()->GetTabStripModel()->count());
  EXPECT_FALSE(
      browser()->GetTabStripModel()->group_model()->ContainsTabGroup(group_id));

  // New browser has two tabs with the tab group.
  ASSERT_EQ(2, second_browser->GetTabStripModel()->count());
  CheckBrowserContainsTabGroupWithSize(second_browser, group_id, 2u);
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, MoveTabsToNewWindow_WithSplitView) {
  // Three tabs with last two in a group
  AddTabs(2);
  browser()->tab_strip_model()->ActivateTabAt(2);
  const split_tabs::SplitTabId split_id =
      browser()->tab_strip_model()->AddToNewSplit(
          {1},
          split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kVertical,
                                         1.0f),
          split_tabs::SplitTabCreatedSource::kToolbarButton);

  // Move both tabs in the split to a new window.
  auto browser_created_observer =
      std::make_optional<ui_test_utils::BrowserCreatedObserver>();
  chrome::MoveTabsToNewWindow(browser(), {1, 2});
  const BrowserWindowInterface* const second_browser =
      browser_created_observer->Wait();

  // Original browser has one tab and no split view.
  ASSERT_EQ(1, browser()->GetTabStripModel()->count());
  EXPECT_FALSE(browser()->GetTabStripModel()->ContainsSplit(split_id));

  // New browser has two tabs with the split view.
  ASSERT_EQ(2, second_browser->GetTabStripModel()->count());
  EXPECT_TRUE(second_browser->GetTabStripModel()->ContainsSplit(split_id));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest,
                       MoveTabsToNewWindow_WithTabsAndCollections) {
  // Seven tabs with the following structure: 0 1gs 2gs 3g 4 5 6
  AddTabs(6);
  browser()->tab_strip_model()->ActivateTabAt(1);
  const split_tabs::SplitTabId split_id =
      browser()->tab_strip_model()->AddToNewSplit(
          {2},
          split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kVertical,
                                         1.0f),
          split_tabs::SplitTabCreatedSource::kToolbarButton);
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({1, 2, 3});

  // Move tabs 1 through 4 to a new window.
  auto browser_created_observer =
      std::make_optional<ui_test_utils::BrowserCreatedObserver>();
  chrome::MoveTabsToNewWindow(browser(), {1, 2, 3, 4});
  const BrowserWindowInterface* const second_browser =
      browser_created_observer->Wait();

  // Original browser has three tabs and no split view or group.
  ASSERT_EQ(3, browser()->GetTabStripModel()->count());
  EXPECT_FALSE(browser()->GetTabStripModel()->ContainsSplit(split_id));
  EXPECT_FALSE(
      browser()->GetTabStripModel()->group_model()->ContainsTabGroup(group_id));

  // New browser has 4 tabs with a split and group.
  ASSERT_EQ(4, second_browser->GetTabStripModel()->count());
  EXPECT_TRUE(second_browser->GetTabStripModel()->ContainsSplit(split_id));
  CheckBrowserContainsTabGroupWithSize(second_browser, group_id, 3u);
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, MoveGroupToNewWindow) {
  AddTabs(2);
  std::vector<int> indices = {1, 2};
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup(indices);
  browser()->tab_strip_model()->ChangeTabGroupVisuals(
      group_id, tab_groups::TabGroupVisualData(
                    u"Test Group", tab_groups::TabGroupColorId::kGrey));

  auto browser_created_observer =
      std::make_optional<ui_test_utils::BrowserCreatedObserver>();
  chrome::MoveGroupToNewWindow(browser(), group_id);
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  Browser* active_browser = browser_created_observer->Wait();

  CheckBrowserContainsTabGroupWithSize(active_browser, group_id, 2u);
  EXPECT_EQ(tab_groups::TabGroupVisualData(u"Test Group",
                                           tab_groups::TabGroupColorId::kGrey),
            *active_browser->tab_strip_model()
                 ->group_model()
                 ->GetTabGroup(group_id)
                 ->visual_data());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, MoveGroupToExistingWindow) {
  // Prepare the source browser with a few tabs and a tab group.
  AddTabs(browser(), 3);
  std::vector<int> indices = {1, 2};
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup(indices);
  browser()->tab_strip_model()->ChangeTabGroupVisuals(
      group_id,
      tab_groups::TabGroupVisualData(u"Test Group ExistingWindow",
                                     tab_groups::TabGroupColorId::kBlue));

  // Prepare the target browser (existing window).
  Browser* target_browser =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  ASSERT_TRUE(target_browser);
  AddTabs(target_browser, 1);

  // Perform the move to the existing window.
  chrome::MoveGroupToExistingWindow(browser(), target_browser, group_id);

  // Verify the source window no longer contains the tab group.
  EXPECT_FALSE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(group_id));
  EXPECT_EQ(2u, browser()->tab_strip_model()->count());

  // Verify the target window received the tab group with correct properties.
  CheckBrowserContainsTabGroupWithSize(target_browser, group_id, 2u);
  EXPECT_EQ(tab_groups::TabGroupVisualData(u"Test Group ExistingWindow",
                                           tab_groups::TabGroupColorId::kBlue),
            *target_browser->tab_strip_model()
                 ->group_model()
                 ->GetTabGroup(group_id)
                 ->visual_data());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, MoveTabsToExistingWindow) {
  // Create another window, and add tabs.
  Browser* second_window =
      ui_test_utils::OpenNewEmptyWindowAndWaitUntilActivated(
          browser()->profile());
  AddTabs(browser(), 2);
  AddTabs(second_window, 1);
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_EQ(2, second_window->tab_strip_model()->count());

  // Single tab move to an existing window.
  chrome::MoveTabsToExistingWindow(browser(), second_window, {0});
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(3, second_window->tab_strip_model()->count());

  // Multiple tab move to an existing window.
  chrome::MoveTabsToExistingWindow(second_window, browser(), {0, 2});
  ASSERT_EQ(4, browser()->tab_strip_model()->count());
  ASSERT_EQ(1, second_window->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest,
                       MoveTabsToExistingWindow_ActiveTabMoved) {
  // Source browser: 0(NTP),1,2(active),3
  AddTabs(browser(), 3);
  browser()->tab_strip_model()->ActivateTabAt(2);
  content::WebContents* src_active_tab =
      browser()->tab_strip_model()->GetWebContentsAt(2);
  ASSERT_EQ(4, browser()->tab_strip_model()->count());

  // Target browser: 0(active)
  Browser* target_browser =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  AddTabs(target_browser, 1);
  ASSERT_EQ(1, target_browser->tab_strip_model()->count());

  // Move tabs: includes active tab.
  chrome::MoveTabsToExistingWindow(browser(), target_browser, {1, 2, 3});

  // Verify tab counts after move.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(4, target_browser->tab_strip_model()->count());

  // Verify active tab in target matches the original active tab.
  // 0,1,2(active),3
  EXPECT_EQ(2, target_browser->tab_strip_model()->active_index());
  EXPECT_EQ(src_active_tab,
            target_browser->tab_strip_model()->GetActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest,
                       MoveTabsToExistingWindow_ActiveTabNotMoved) {
  // Source browser: 0(NTP,active),1,2,3
  AddTabs(browser(), 3);
  browser()->tab_strip_model()->ActivateTabAt(0);
  ASSERT_EQ(4, browser()->tab_strip_model()->count());

  // Target browser: 0(active)
  Browser* target_browser =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  AddTabs(target_browser, 1);
  ASSERT_EQ(1, target_browser->tab_strip_model()->count());

  // Move tabs: does NOT include active tab.
  content::WebContents* first_moved_tab =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  chrome::MoveTabsToExistingWindow(browser(), target_browser, {1, 2, 3});

  // Verify tab counts after move.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(4, target_browser->tab_strip_model()->count());

  // Fallback to the first moved tab.
  // 0,1(active),2,3
  EXPECT_EQ(1, target_browser->tab_strip_model()->active_index());
  EXPECT_EQ(first_moved_tab,
            target_browser->tab_strip_model()->GetActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest,
                       MoveTabsToExistingWindow_WithGroup) {
  // Source browser: three tabs with first two in a group
  AddTabs(2);
  std::vector<int> indices = {1, 2};
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup(indices);
  browser()->tab_strip_model()->ChangeTabGroupVisuals(
      group_id, tab_groups::TabGroupVisualData(
                    u"Test Group", tab_groups::TabGroupColorId::kGrey));

  // Target browser: 0(active)
  Browser* target_browser =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  AddTabs(target_browser, 1);
  ASSERT_EQ(1, target_browser->tab_strip_model()->count());

  // Move both tabs in the group to a new window.
  chrome::MoveTabsToExistingWindow(browser(), target_browser, indices);

  // Original browser has one tab and no group.
  ASSERT_EQ(1, browser()->GetTabStripModel()->count());
  EXPECT_FALSE(
      browser()->GetTabStripModel()->group_model()->ContainsTabGroup(group_id));

  // New browser has three tabs with the tab group.
  ASSERT_EQ(3, target_browser->GetTabStripModel()->count());
  CheckBrowserContainsTabGroupWithSize(target_browser, group_id, 2u);
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest,
                       MoveTabsToExistingWindow_WithSplitView) {
  // Source browser: three tabs with last two in a group
  AddTabs(2);
  browser()->tab_strip_model()->ActivateTabAt(2);
  const split_tabs::SplitTabId split_id =
      browser()->tab_strip_model()->AddToNewSplit(
          {1},
          split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kVertical,
                                         1.0f),
          split_tabs::SplitTabCreatedSource::kToolbarButton);

  // Target browser: 0(active)
  Browser* target_browser =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  AddTabs(target_browser, 1);
  ASSERT_EQ(1, target_browser->tab_strip_model()->count());

  // Move both tabs in the split to a new window.
  chrome::MoveTabsToExistingWindow(browser(), target_browser, {1, 2});

  // Original browser has one tab and no split view.
  ASSERT_EQ(1, browser()->GetTabStripModel()->count());
  EXPECT_FALSE(browser()->GetTabStripModel()->ContainsSplit(split_id));

  // New browser has three tabs with the split view.
  ASSERT_EQ(3, target_browser->GetTabStripModel()->count());
  EXPECT_TRUE(target_browser->GetTabStripModel()->ContainsSplit(split_id));
}

// Tests IDC_MOVE_TAB_TO_NEW_WINDOW. This is a browser test and not a unit test
// since it needs to create a new browser window, which doesn't work with a
// TestingProfile.
IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, MoveActiveTabToNewWindow) {
  GURL url1("chrome://version");
  GURL url2("chrome://about");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  // Should be disabled with 1 tab.
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_MOVE_TAB_TO_NEW_WINDOW));
  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_LINK));
  // Two tabs is enough for it to be meaningful to pop one out.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_MOVE_TAB_TO_NEW_WINDOW));

  // Pre-command, assert that we have one browser, with two tabs, with the
  // url2 tab active.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);

  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  chrome::ExecuteCommand(browser(), IDC_MOVE_TAB_TO_NEW_WINDOW);
  BrowserWindowInterface* const active_browser =
      browser_created_observer.Wait();
  ui_test_utils::WaitUntilBrowserBecomeActive(active_browser);

  // Now we should have: two browsers, each with one tab (url1 in browser(),
  // and url2 in the new one).
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 2u);
  EXPECT_NE(active_browser, browser());
  EXPECT_EQ(browser()->GetTabStripModel()->count(), 1);
  EXPECT_EQ(active_browser->GetTabStripModel()->count(), 1);
  EXPECT_EQ(browser()->GetTabStripModel()->GetActiveWebContents()->GetURL(),
            url1);
  EXPECT_EQ(
      active_browser->GetTabStripModel()->GetActiveWebContents()->GetURL(),
      url2);
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest,
                       MoveActiveTabToNewWindowMultipleSelection) {
  GURL url1("chrome://version");
  GURL url2("chrome://about");
  GURL url3("chrome://terms");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_LINK));
  ASSERT_TRUE(AddTabAtIndex(2, url3, ui::PAGE_TRANSITION_LINK));
  // Select the first tab.
  browser()->tab_strip_model()->SelectTabAt(0);
  // First and third (since it's active) should be selected
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabSelected(0));
  EXPECT_FALSE(browser()->tab_strip_model()->IsTabSelected(1));
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabSelected(2));

  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  chrome::ExecuteCommand(browser(), IDC_MOVE_TAB_TO_NEW_WINDOW);
  BrowserWindowInterface* const active_browser =
      browser_created_observer.Wait();
  ui_test_utils::WaitUntilBrowserBecomeActive(active_browser);

  // Now we should have two browsers:
  // The original, now with only a single tab: url2
  // The new one with the two tabs we moved: url1 and url3. This one should
  // be active.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 2u);
  EXPECT_NE(active_browser, browser());
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 1);
  ASSERT_EQ(active_browser->GetTabStripModel()->count(), 2);
  EXPECT_EQ(browser()->GetTabStripModel()->GetActiveWebContents()->GetURL(),
            url2);
  EXPECT_EQ(active_browser->GetTabStripModel()->GetWebContentsAt(0)->GetURL(),
            url1);
  EXPECT_EQ(active_browser->GetTabStripModel()->GetWebContentsAt(1)->GetURL(),
            url3);
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, StartsOrganizationRequest) {
  base::HistogramTester histogram_tester;

  chrome::ExecuteCommand(browser(), IDC_ORGANIZE_TABS);

  TabOrganizationService* service =
      TabOrganizationServiceFactory::GetForProfile(browser()->profile());
  const TabOrganizationSession* session =
      service->GetSessionForBrowser(browser());

  EXPECT_EQ(TabOrganizationRequest::State::NOT_STARTED,
            session->request()->state());

  histogram_tester.ExpectUniqueSample("Tab.Organization.AllEntrypoints.Clicked",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.ThreeDotMenu.Clicked",
                                      true, 1);
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, ShowsDeclutter) {
  TabSearchBubbleHost* tab_search_bubble_host =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetTabSearchBubbleHost();
  EXPECT_FALSE(tab_search_bubble_host->bubble_created_time_for_testing());

  chrome::ExecuteCommand(browser(), IDC_DECLUTTER_TABS);

  EXPECT_TRUE(tab_search_bubble_host->bubble_created_time_for_testing());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest,
                       ConvertPopupToTabbedBrowserShutdownRace) {
  // Confirm we do not incorrectly start shutdown when converting a popup into a
  // tab, in the case where the popup is the only active Browser object
  Browser* popup_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  chrome::AddTabAt(popup_browser, GURL(url::kAboutBlankURL), -1, true);
  popup_browser->tab_strip_model()->SelectTabAt(0);
  browser()->tab_strip_model()->CloseAllTabs();
  ConvertPopupToTabbedBrowser(popup_browser);
  EXPECT_EQ(false, browser_shutdown::HasShutdownStarted());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest,
                       OpenProductSpecifications_ShowNewTab) {
  // Mock that the disclosure dialog has shown.
  browser()->profile()->GetPrefs()->SetInteger(
      commerce::kProductSpecificationsAcceptedDisclosureVersion,
      static_cast<int>(
          commerce::product_specifications::mojom::DisclosureVersion::kV1));

  int tab_count = browser()->tab_strip_model()->count();
  chrome::OpenCommerceProductSpecificationsTab(
      browser(), {GURL("foo.com"), GURL("bar.com")}, 0);

  auto* dialog = commerce::ProductSpecificationsDisclosureDialog::
      current_instance_for_testing();
  ASSERT_FALSE(dialog);
  // No new tab is created since the dialog will block creating new product
  // specifications tab.
  ASSERT_EQ(tab_count + 1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest,
                       OpenProductSpecifications_ShowDialog) {
  int tab_count = browser()->tab_strip_model()->count();
  chrome::OpenCommerceProductSpecificationsTab(
      browser(), {GURL("foo.com"), GURL("bar.com")}, 0);

  auto* dialog = commerce::ProductSpecificationsDisclosureDialog::
      current_instance_for_testing();
  ASSERT_TRUE(dialog);
  // No new tab is created.
  ASSERT_EQ(tab_count, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, AddingToReadingListOpensToast) {
  GURL main_url(https_server_.GetURL("a.test", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  chrome::ExecuteCommand(browser(), IDC_READING_LIST_MENU_ADD_TAB);
  EXPECT_TRUE(browser()->GetFeatures().toast_controller()->IsShowingToast());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest,
                       AddingToReadingListWithSidePanelShowsNoToast) {
  GURL main_url(https_server_.GetURL("a.test", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  auto* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  side_panel_ui->Show(SidePanelEntryId::kReadingList);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadingList));
  }));
  chrome::ExecuteCommand(browser(), IDC_READING_LIST_MENU_ADD_TAB);
  EXPECT_FALSE(browser()->GetFeatures().toast_controller()->IsShowingToast());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, CopyingUrlOpensToast) {
  GURL main_url(https_server_.GetURL("a.test", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  chrome::ExecuteCommand(browser(), IDC_COPY_URL);
  EXPECT_TRUE(browser()->GetFeatures().toast_controller()->IsShowingToast());
}

}  // namespace chrome
