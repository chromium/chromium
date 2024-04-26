// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/popup_tracker.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/supports_user_data.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_database_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/safe_browsing/core/browser/db/v4_embedded_test_server_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/ozone/public/ozone_switches.h"
#endif

namespace {
const char kUkmEngagementTime[] = "EngagementTime";
const char kUkmUserInitiatedClose[] = "UserInitiatedClose";
const char kUkmTrusted[] = "Trusted";
const char kUkmNumInteractions[] = "NumInteractions";
const char kUkmSafeBrowsingStatus[] = "SafeBrowsingStatus";
const char kUkmWindowOpenDisposition[] = "WindowOpenDisposition";
const char kUkmNumActivationInteractions[] = "NumActivationInteractions";
const char kUkmNumGestureScrollBeginInteractions[] =
    "NumGestureScrollBeginInteractions";
const char kUkmRedirectCount[] = "RedirectCount";
}  // namespace

using UkmEntry = ukm::builders::Popup_Closed;

class PopupTrackerBrowserTest : public InProcessBrowserTest {
 public:
  PopupTrackerBrowserTest() = default;
  ~PopupTrackerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

 protected:
  enum UserClosedPopup { kTrue, kFalse };

  size_t GetNumPopupUkmEntries() const {
    return test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName).size();
  }

  const ukm::mojom::UkmEntry* ExpectAndGetEntry(const GURL& expected_url) {
    const auto& entries =
        test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(1u, entries.size());
    const auto* entry = entries[0].get();
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, expected_url);
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(entry, kUkmEngagementTime));
    return entry;
  }

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, NoPopup_NoTracker) {
  base::HistogramTester tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_FALSE(blocked_content::PopupTracker::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents()));

  EXPECT_EQ(0u, GetNumPopupUkmEntries());
}

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest,
                       WindowOpenPopup_HasTracker_GestureClose) {
  base::HistogramTester tester;
  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "window.open('/title1.html')"));
  navigation_observer.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(blocked_content::PopupTracker::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Close the popup and check metric.
  int active_index = browser()->tab_strip_model()->active_index();
  content::WebContentsDestroyedWatcher destroyed_watcher(
      browser()->tab_strip_model()->GetWebContentsAt(active_index));
  browser()->tab_strip_model()->CloseWebContentsAt(
      active_index, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  auto* entry = ExpectAndGetEntry(first_url);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmUserInitiatedClose, 1u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmTrusted, 0u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmNumInteractions, 0u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmNumActivationInteractions,
                                        0u);
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kUkmNumGestureScrollBeginInteractions, 0u);
}

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest,
                       WindowOpenPopup_WithInteraction) {
  base::HistogramTester tester;
  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "window.open('/title1.html')"));
  navigation_observer.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(blocked_content::PopupTracker::FromWebContents(popup));

  // Perform some user gestures on the page.
  content::SimulateMouseClick(popup, 0, blink::WebMouseEvent::Button::kLeft);
  content::SimulateMouseClick(popup, 0, blink::WebMouseEvent::Button::kLeft);
  content::SimulateGestureScrollSequence(popup, gfx::Point(100, 100),
                                         gfx::Vector2dF(0, 15));

  // Close the popup and check metric.
  int active_index = browser()->tab_strip_model()->active_index();
  content::WebContentsDestroyedWatcher destroyed_watcher(popup);
  browser()->tab_strip_model()->CloseWebContentsAt(
      active_index, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  auto* entry = ExpectAndGetEntry(first_url);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmUserInitiatedClose, 1u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmTrusted, 0u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmNumInteractions, 3u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmNumActivationInteractions,
                                        2u);
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kUkmNumGestureScrollBeginInteractions, 1u);
}

// OpenURLFromTab goes through a different code path than traditional popups
// that use window.open(). Make sure the tracker is created in those cases.
// Disabled due to flakiness. See crbug.com/1186441.
IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, DISABLED_ControlClick_HasTracker) {
  base::HistogramTester tester;
  const GURL url = embedded_test_server()->GetURL(
      "/popup_blocker/popup-simulated-click-on-anchor.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Mac uses command instead of control for the new tab action.
  bool is_mac = false;
#if BUILDFLAG(IS_MAC)
  is_mac = true;
#endif

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  SimulateKeyPress(browser()->tab_strip_model()->GetActiveWebContents(),
                   ui::DomKey::ENTER, ui::DomCode::ENTER, ui::VKEY_RETURN,
                   !is_mac /* control */, false /* shift */, false /* alt */,
                   is_mac /* command */);
  navigation_observer.Wait();

  EXPECT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(blocked_content::PopupTracker::FromWebContents(new_contents));

  // Close the popup and check metric.
  content::WebContentsDestroyedWatcher destroyed_watcher(new_contents);
  BrowserList::CloseAllBrowsersWithProfile(
      Profile::FromBrowserContext(new_contents->GetBrowserContext()));
  destroyed_watcher.Wait();

  auto* entry = ExpectAndGetEntry(url);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmUserInitiatedClose, 0u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmTrusted, 1u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmNumInteractions, 0u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmNumActivationInteractions,
                                        0u);
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kUkmNumGestureScrollBeginInteractions, 0u);
}

// Disabled due to flakiness. See crbug.com/1186441.
IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, DISABLED_ShiftClick_HasTracker) {
  base::HistogramTester tester;
  const GURL url = embedded_test_server()->GetURL(
      "/popup_blocker/popup-simulated-click-on-anchor.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  SimulateKeyPress(browser()->tab_strip_model()->GetActiveWebContents(),
                   ui::DomKey::ENTER, ui::DomCode::ENTER, ui::VKEY_RETURN,
                   false /* control */, true /* shift */, false /* alt */,
                   false /* command */);
  navigation_observer.Wait();

  EXPECT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  content::WebContents* new_contents = BrowserList::GetInstance()
                                           ->GetLastActive()
                                           ->tab_strip_model()
                                           ->GetActiveWebContents();
  EXPECT_TRUE(blocked_content::PopupTracker::FromWebContents(new_contents));

  // Close the popup and check metric.
  content::WebContentsDestroyedWatcher destroyed_watcher(new_contents);
  BrowserList::CloseAllBrowsersWithProfile(
      Profile::FromBrowserContext(new_contents->GetBrowserContext()));
  destroyed_watcher.Wait();

  auto* entry = ExpectAndGetEntry(url);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmUserInitiatedClose, 0u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmTrusted, 1u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmNumInteractions, 0u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmNumActivationInteractions,
                                        0u);
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kUkmNumGestureScrollBeginInteractions, 0u);
}

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, AllowlistedPopup_HasTracker) {
  base::HistogramTester tester;
  const GURL url =
      embedded_test_server()->GetURL("/popup_blocker/popup-window-open.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Is blocked by the popup blocker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(content_settings::PageSpecificContentSettings::GetForFrame(
                  web_contents->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::POPUPS));

  // Click through to open the popup.
  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();
  auto* popup_blocker =
      blocked_content::PopupBlockerTabHelper::FromWebContents(web_contents);
  popup_blocker->ShowBlockedPopup(
      popup_blocker->GetBlockedPopupRequests().begin()->first,
      WindowOpenDisposition::NEW_FOREGROUND_TAB);
  navigation_observer.Wait();

  // Close the popup and check metric.
  content::WebContentsDestroyedWatcher destroyed_watcher(
      browser()->tab_strip_model()->GetActiveWebContents());
  browser()->tab_strip_model()->CloseAllTabs();
  destroyed_watcher.Wait();

  auto* entry = ExpectAndGetEntry(url);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmUserInitiatedClose, 0u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmTrusted, 1u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmNumInteractions, 0u);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmNumActivationInteractions,
                                        0u);
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kUkmNumGestureScrollBeginInteractions, 0u);
}

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, NoOpener_NoTracker) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  EXPECT_FALSE(blocked_content::PopupTracker::FromWebContents(new_contents));
  EXPECT_EQ(0u, GetNumPopupUkmEntries());
}

// Tests for the subresource_filter popup blocker.
class SafeBrowsingPopupTrackerBrowserTest : public PopupTrackerBrowserTest {
 public:
  SafeBrowsingPopupTrackerBrowserTest() = default;

  SafeBrowsingPopupTrackerBrowserTest(
      const SafeBrowsingPopupTrackerBrowserTest&) = delete;
  SafeBrowsingPopupTrackerBrowserTest& operator=(
      const SafeBrowsingPopupTrackerBrowserTest&) = delete;

  ~SafeBrowsingPopupTrackerBrowserTest() override = default;

  void SetUp() override {
    database_helper_ = CreateTestDatabase();
    PopupTrackerBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    database_helper_.reset();
  }

  virtual std::unique_ptr<TestSafeBrowsingDatabaseHelper> CreateTestDatabase() {
    std::vector<safe_browsing::ListIdentifier> list_ids = {
        safe_browsing::GetUrlSubresourceFilterId()};
    return std::make_unique<TestSafeBrowsingDatabaseHelper>(
        std::make_unique<safe_browsing::TestV4GetHashProtocolManagerFactory>(),
        std::move(list_ids));
  }

  void ConfigureAsList(const GURL& url,
                       const safe_browsing::ListIdentifier& list_identifier) {
    safe_browsing::ThreatMetadata metadata;
    database_helper_->AddFullHashToDbAndFullHashCache(url, list_identifier,
                                                      metadata);
  }

  TestSafeBrowsingDatabaseHelper* database_helper() {
    return database_helper_.get();
  }

 private:
  std::unique_ptr<TestSafeBrowsingDatabaseHelper> database_helper_;
};

// Pop-ups closed before navigation has finished will receive no safe browsing
// status.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPopupTrackerBrowserTest,
                       PopupClosedBeforeNavigationFinished_LoggedAsNoValue) {
  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  const GURL unsafe_url = embedded_test_server()->GetURL("/slow");
  ConfigureAsList(unsafe_url, safe_browsing::GetUrlSocEngId());

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "window.open('/slow?1000')"));

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(blocked_content::PopupTracker::FromWebContents(popup));

  // Close the popup and check metric.
  int active_index = browser()->tab_strip_model()->active_index();
  content::WebContentsDestroyedWatcher destroyed_watcher(popup);
  browser()->tab_strip_model()->CloseWebContentsAt(
      active_index, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  auto* entry = ExpectAndGetEntry(first_url);
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kUkmSafeBrowsingStatus,
      static_cast<int>(
          blocked_content::PopupTracker::PopupSafeBrowsingStatus::kNoValue));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingPopupTrackerBrowserTest,
                       SafePopup_LoggedAsSafe) {
  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  const GURL second_url = embedded_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "window.open('/title2.html')"));
  navigation_observer.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(blocked_content::PopupTracker::FromWebContents(popup));

  // Close the popup and check metric.
  int active_index = browser()->tab_strip_model()->active_index();
  content::WebContentsDestroyedWatcher destroyed_watcher(popup);
  browser()->tab_strip_model()->CloseWebContentsAt(
      active_index, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  auto* entry = ExpectAndGetEntry(first_url);
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kUkmSafeBrowsingStatus,
      static_cast<int>(
          blocked_content::PopupTracker::PopupSafeBrowsingStatus::kSafe));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingPopupTrackerBrowserTest,
                       PhishingPopup_LoggedAsUnsafe) {
  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  // Associate each domain with a separate safe browsing ListIdentifier to
  // exercise the set of lists.
  std::vector<std::pair<std::string, safe_browsing::ListIdentifier>>
      domain_list_pairs = {
          {"a.com", safe_browsing::GetUrlSocEngId()},
          {"b.com", safe_browsing::GetUrlSubresourceFilterId()}};

  // For each pair, configure the local safe browsing database and open a
  // pop-up to the url.
  for (const auto& domain_list_pair : domain_list_pairs) {
    const GURL unsafe_url =
        embedded_test_server()->GetURL(domain_list_pair.first, "/title2.html");
    ConfigureAsList(unsafe_url, domain_list_pair.second);

    content::TestNavigationObserver navigation_observer(nullptr, 1);
    navigation_observer.StartWatchingNewWebContents();

    EXPECT_TRUE(
        content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        "window.open('" + unsafe_url.spec() + "')"));
    navigation_observer.Wait();

    EXPECT_EQ(2, browser()->tab_strip_model()->count());
    content::WebContents* popup =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(blocked_content::PopupTracker::FromWebContents(popup));

    // Close the popup and check metric.
    int active_index = browser()->tab_strip_model()->active_index();
    content::WebContentsDestroyedWatcher destroyed_watcher(popup);
    browser()->tab_strip_model()->CloseWebContentsAt(
        active_index, TabCloseTypes::CLOSE_USER_GESTURE);
    destroyed_watcher.Wait();
  }

  // The URL should have 4 pop-up entries with an unsafe safe browsing status.
  auto entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Popup_Closed::kEntryName);
  EXPECT_EQ(2u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kUkmSafeBrowsingStatus,
        static_cast<int>(
            blocked_content::PopupTracker::PopupSafeBrowsingStatus::kUnsafe));
  }
}

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, PopupInTab_IsWindowFalse) {
  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "window.open('/title1.html')"));
  navigation_observer.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(blocked_content::PopupTracker::FromWebContents(popup));

  // Close the popup and check metric.
  int active_index = browser()->tab_strip_model()->active_index();
  content::WebContentsDestroyedWatcher destroyed_watcher(popup);
  browser()->tab_strip_model()->CloseWebContentsAt(
      active_index, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  auto* entry = ExpectAndGetEntry(first_url);
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kUkmWindowOpenDisposition,
      static_cast<int>(WindowOpenDisposition::NEW_FOREGROUND_TAB));
}

// TODO(crbug.com/40749398): Test is flaky on Linux.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PopupInWindow_IsWindowTrue DISABLED_PopupInWindow_IsWindowTrue
#else
#define MAYBE_PopupInWindow_IsWindowTrue PopupInWindow_IsWindowTrue
#endif
IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest,
                       MAYBE_PopupInWindow_IsWindowTrue) {
  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('/title1.html', 'new_window', "
      "'location=yes,height=570,width=520,scrollbars=yes,status=yes')"));
  navigation_observer.Wait();
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  Browser* created_browser = chrome::FindLastActive();

  EXPECT_EQ(1, created_browser->tab_strip_model()->count());
  content::WebContents* popup =
      created_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(blocked_content::PopupTracker::FromWebContents(popup));

  // Close the popup and check metric.
  int active_index = created_browser->tab_strip_model()->active_index();
  content::WebContentsDestroyedWatcher destroyed_watcher(popup);
  created_browser->tab_strip_model()->CloseWebContentsAt(
      active_index, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  auto* entry = ExpectAndGetEntry(first_url);
  test_ukm_recorder_->ExpectEntryMetric(
      entry, kUkmWindowOpenDisposition,
      static_cast<int>(WindowOpenDisposition::NEW_POPUP));
}

// TODO(crbug.com/40730174): Test is flaky on Lacros, Linux Ozone Wayland.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_PopupNoRedirect_RedirectCountZero DISABLED_PopupNoRedirect_RedirectCountZero
#else
#define MAYBE_PopupNoRedirect_RedirectCountZero PopupNoRedirect_RedirectCountZero
#endif
IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest,
                       MAYBE_PopupNoRedirect_RedirectCountZero) {
  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('/title1.html', 'new_window', "
      "'location=yes,height=570,width=520,scrollbars=yes,status=yes')"));
  navigation_observer.Wait();
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  Browser* created_browser = chrome::FindLastActive();

  EXPECT_EQ(1, created_browser->tab_strip_model()->count());
  content::WebContents* popup =
      created_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(blocked_content::PopupTracker::FromWebContents(popup));

  // Close the popup and check that the pop up did not redirect.
  int active_index = created_browser->tab_strip_model()->active_index();
  content::WebContentsDestroyedWatcher destroyed_watcher(popup);
  created_browser->tab_strip_model()->CloseWebContentsAt(
      active_index, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  auto* entry = ExpectAndGetEntry(first_url);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmRedirectCount, 0);
}

// TODO(crbug.com/40749618): Test is flaky on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_PopupRedirectsTwice_RedirectCountTwo \
  DISABLED_PopupRedirectsTwice_RedirectCountTwo
#else
#define MAYBE_PopupRedirectsTwice_RedirectCountTwo \
  PopupRedirectsTwice_RedirectCountTwo
#endif
IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest,
                       MAYBE_PopupRedirectsTwice_RedirectCountTwo) {
#if BUILDFLAG(IS_LINUX)
  {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kOzonePlatform) &&
        command_line->GetSwitchValueASCII(switches::kOzonePlatform) ==
            "wayland") {
      // TODO(crbug.com/40749618): Test is flaky on Linux Wayland configuration.
      GTEST_SKIP() << "Flaky on Linux Wayland";
    }
  }
#endif

  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  // Redirect the popup using /server-redirect twice.
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('/server-redirect?/server-redirect?/title1.html',"
      "'new_window', 'location=yes,height=570,width=520,scrollbars=yes,"
      "status=yes')"));
  navigation_observer.Wait();
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  Browser* created_browser = chrome::FindLastActive();

  EXPECT_EQ(1, created_browser->tab_strip_model()->count());
  content::WebContents* popup =
      created_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(blocked_content::PopupTracker::FromWebContents(popup));

  // Close the popup and check metric.
  int active_index = created_browser->tab_strip_model()->active_index();
  content::WebContentsDestroyedWatcher destroyed_watcher(popup);
  created_browser->tab_strip_model()->CloseWebContentsAt(
      active_index, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  auto* entry = ExpectAndGetEntry(first_url);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmRedirectCount, 2);
}

// TODO(crbug.com/40749954): Test is flaky on Windows, Linux and Lacros.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_PopupJavascriptRenavigation_RedirectCountZero \
  DISABLED_PopupJavascriptRenavigation_RedirectCountZero
#else
#define MAYBE_PopupJavascriptRenavigation_RedirectCountZero \
  PopupJavascriptRenavigation_RedirectCountZero
#endif
IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest,
                       MAYBE_PopupJavascriptRenavigation_RedirectCountZero) {
  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  // Redirect the popup using /server-redirect twice.
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "var w = window.open('',"
      "'new_window', 'location=yes,height=570,width=520,scrollbars=yes,"
      "status=yes'); "
      "w.location = '/title1.html'"));
  navigation_observer.Wait();
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  Browser* created_browser = chrome::FindLastActive();

  EXPECT_EQ(1, created_browser->tab_strip_model()->count());
  content::WebContents* popup =
      created_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(blocked_content::PopupTracker::FromWebContents(popup));

  // Close the popup and check metric.
  int active_index = created_browser->tab_strip_model()->active_index();
  content::WebContentsDestroyedWatcher destroyed_watcher(popup);
  created_browser->tab_strip_model()->CloseWebContentsAt(
      active_index, TabCloseTypes::CLOSE_USER_GESTURE);
  destroyed_watcher.Wait();

  auto* entry = ExpectAndGetEntry(first_url);
  test_ukm_recorder_->ExpectEntryMetric(entry, kUkmRedirectCount, 0);
}

class PopupTrackerPrerenderBrowserTest : public PopupTrackerBrowserTest {
 public:
  PopupTrackerPrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&PopupTrackerPrerenderBrowserTest::web_contents,
                                base::Unretained(this))) {}
  ~PopupTrackerPrerenderBrowserTest() override = default;

 protected:
  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Test that a navigation of the prerender page shouldn't affect recording UKM
// of PopupTracker.
IN_PROC_BROWSER_TEST_F(PopupTrackerPrerenderBrowserTest,
                       DoNotAffectRecordingUKMByPrerender) {
  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  // Load a prerender url in the popup window.
  const GURL prerender_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('/popup_blocker/popup-simple-prerender.html')"));
  prerender_helper()->WaitForPrerenderLoadCompletion(prerender_url);

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  auto* popup_tracker = blocked_content::PopupTracker::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_NE(popup_tracker, nullptr);

  // The PopupTracker should not treat a prerender navigation as a navigation
  // away from the first document. So, the |first_load_visible_time_| in
  // PopupTracker should be empty and not be used for the recording UKM.
  EXPECT_FALSE(popup_tracker->has_first_load_visible_time_for_testing());
}
