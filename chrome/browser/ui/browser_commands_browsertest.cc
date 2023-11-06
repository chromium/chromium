// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"

#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/ui_base_features.h"

namespace chrome {

class BrowserCommandsTest : public InProcessBrowserTest {
 public:
  BrowserCommandsTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatures(
        {features::kTabOrganization, features::kChromeRefresh2023}, {});
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

  void AddAndReloadTabs(int tab_count) {
    for (int i = 0; i < tab_count; ++i) {
      ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), i + 1, GURL(kUrl),
                                         ui::PAGE_TRANSITION_LINK, false));
    }

    // Add tabs to the selection (the last one created remains selected) and
    // trigger a reload command on all of them.
    for (int i = 0; i < tab_count - 1; ++i) {
      browser()->tab_strip_model()->ToggleSelectionAt(i + 1);
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
        "ThirdPartyCookies.BreakageIndicator",
        {"BreakageIndicatorType", "TPCBlocked", "TPCBlockedInSettings"});
    EXPECT_EQ(entries.size(), size);
    EXPECT_EQ(
        entries.at(index).metrics.at("BreakageIndicatorType"),
        static_cast<int>(net::cookie_util::BreakageIndicatorType::USER_RELOAD));
    EXPECT_EQ(entries.at(index).metrics.at("TPCBlocked"), blocked);
    EXPECT_EQ(entries.at(index).metrics.at("TPCBlockedInSettings"),
              settings_blocked);
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

  for (ReloadObserver& watcher : watcher_vec)
    EXPECT_EQ(0, watcher.load_count());

  // Add two tabs to the selection (the last one created remains selected) and
  // trigger a reload command on all of them.
  for (int i = 0; i < kTabCount - 1; i++)
    browser()->tab_strip_model()->ToggleSelectionAt(i + 1);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));

  int load_sum = 0;
  for (ReloadObserver& watcher : watcher_vec)
    load_sum += watcher.load_count();
  EXPECT_EQ(kTabCount, load_sum);
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
  auto AddTabs = [](Browser* browser, unsigned int num_tabs) {
    for (unsigned int i = 0; i < num_tabs; ++i)
      chrome::NewTab(browser);
  };

  // Single Tab Move to New Window.
  // 1 (Current) + 1 (Added) = 2
  AddTabs(browser(), 1);
  std::vector<int> indices = {0};
  // 2 (Current) - 1 (Moved) = 1
  chrome::MoveTabsToNewWindow(browser(), indices);
  ASSERT_TRUE(browser()->tab_strip_model()->count() == 1);

  // Multi-Tab Move to New Window.
  // 1 (Current) + 3 (Added) = 4
  AddTabs(browser(), 3);
  indices = {0, 1};
  // 4 (Current) - 2 (Moved) = 2
  chrome::MoveTabsToNewWindow(browser(), indices);
  ASSERT_TRUE(browser()->tab_strip_model()->count() == 2);

  // Check that the two additional windows have been created.
  BrowserList* active_browser_list = BrowserList::GetInstance();
  EXPECT_EQ(3u, active_browser_list->size());

  // Check that the tabs made it to other windows.
  Browser* browser = active_browser_list->get(1);
  EXPECT_EQ(1, browser->tab_strip_model()->count());
  browser = active_browser_list->get(2);
  EXPECT_EQ(2, browser->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, MoveToExistingWindow) {
  auto AddTabs = [](Browser* browser, unsigned int num_tabs) {
    for (unsigned int i = 0; i < num_tabs; ++i)
      chrome::NewTab(browser);
  };

  // Create another window, and add tabs.
  chrome::NewEmptyWindow(browser()->profile());
  Browser* second_window = BrowserList::GetInstance()->GetLastActive();
  AddTabs(browser(), 2);
  AddTabs(second_window, 1);
  ASSERT_TRUE(browser()->tab_strip_model()->count() == 3);
  ASSERT_TRUE(second_window->tab_strip_model()->count() == 2);

  // Single tab move to an existing window.
  std::vector<int> indices = {0};
  chrome::MoveTabsToExistingWindow(browser(), second_window, indices);
  ASSERT_TRUE(browser()->tab_strip_model()->count() == 2);
  ASSERT_TRUE(second_window->tab_strip_model()->count() == 3);

  // Multiple tab move to an existing window.
  indices = {0, 2};
  chrome::MoveTabsToExistingWindow(second_window, browser(), indices);
  ASSERT_TRUE(browser()->tab_strip_model()->count() == 4);
  ASSERT_TRUE(second_window->tab_strip_model()->count() == 1);
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

  BrowserList* browser_list = BrowserList::GetInstance();
  // Pre-command, assert that we have one browser, with two tabs, with the
  // url2 tab active.
  EXPECT_EQ(browser_list->size(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);

  chrome::ExecuteCommand(browser(), IDC_MOVE_TAB_TO_NEW_WINDOW);

  // Now we should have: two browsers, each with one tab (url1 in browser(),
  // and url2 in the new one).
  Browser* active_browser = browser_list->GetLastActive();
  EXPECT_EQ(browser_list->size(), 2u);
  EXPECT_NE(active_browser, browser());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(active_browser->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url1);
  EXPECT_EQ(active_browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
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
  browser()->tab_strip_model()->ToggleSelectionAt(0);
  // First and third (since it's active) should be selected
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabSelected(0));
  EXPECT_FALSE(browser()->tab_strip_model()->IsTabSelected(1));
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabSelected(2));

  chrome::ExecuteCommand(browser(), IDC_MOVE_TAB_TO_NEW_WINDOW);
  // Now we should have two browsers:
  // The original, now with only a single tab: url2
  // The new one with the two tabs we moved: url1 and url3. This one should
  // be active.
  BrowserList* browser_list = BrowserList::GetInstance();
  Browser* active_browser = browser_list->GetLastActive();
  EXPECT_EQ(browser_list->size(), 2u);
  EXPECT_NE(active_browser, browser());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  ASSERT_EQ(active_browser->tab_strip_model()->count(), 2);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);
  EXPECT_EQ(active_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL(),
            url1);
  EXPECT_EQ(active_browser->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            url3);
}

IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, StartsOrganizationRequest) {
  chrome::ExecuteCommand(browser(), IDC_ORGANIZE_TABS);

  TabOrganizationService* service =
      TabOrganizationServiceFactory::GetForProfile(browser()->profile());
  const TabOrganizationSession* session =
      service->GetSessionForBrowser(browser());

  EXPECT_NE(TabOrganizationRequest::State::NOT_STARTED,
            session->request()->state());
}

}  // namespace chrome
