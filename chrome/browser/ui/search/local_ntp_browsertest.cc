// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_browsertest_base.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/ntp_tiles/constants.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "ui/native_theme/test_native_theme.h"
#include "url/gurl.h"

namespace {

// In a non-signed-in, fresh profile with no history, there should be one
// default TopSites tile (see history::PrepopulatedPage).
const int kDefaultMostVisitedItemCount = 1;

// This is the default maximum custom links we can have. The number comes from
// ntp_tiles::CustomLinksManager.
const int kDefaultCustomLinkMaxCount = 10;

// Name for the Most Visited iframe in the NTP.
const char kMostVisitedIframe[] = "mv-single";

#if defined(OS_WIN) || defined(OS_MAC)
// Name for the edit/add custom link iframe in the NTP.
const char kEditCustomLinkIframe[] = "custom-links-edit";
#endif

// Returns the RenderFrameHost corresponding to the |iframe_name| in the
// given |tab|. |tab| must correspond to an NTP.
content::RenderFrameHost* GetIframe(content::WebContents* tab,
                                    const char* iframe_name) {
  for (content::RenderFrameHost* frame : tab->GetAllFrames()) {
    if (frame->GetFrameName() == iframe_name) {
      return frame;
    }
  }
  return nullptr;
}

class LocalNTPTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // Some tests depend on the prepopulated most visited tiles coming from
    // TopSites, so make sure they are available before running the tests.
    // (TopSites is loaded asynchronously at startup, so without this, there's
    // a chance that it hasn't finished and we receive 0 tiles.)
    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(browser()->profile());
    TestInstantServiceObserver mv_observer(instant_service);
    // Make sure the observer knows about the current items. Typically, this
    // gets triggered by navigating to an NTP.
    instant_service->UpdateMostVisitedInfo();
    mv_observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount);
  }
};

// Disabled for being flaky. crbug.com/1096976
IN_PROC_BROWSER_TEST_F(LocalNTPTest,
                       DISABLED_EmbeddedSearchAPIOnlyAvailableOnNTP) {
  // Set up a test server, so we have some arbitrary non-NTP URL to navigate to.
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(test_server.Start());
  const GURL other_url = test_server.GetURL("/simple.html");

  // Open a local NTP.
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeSearchLocalNtpUrl));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // Check that the embeddedSearch API is available.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_TRUE(result);

  // Navigate somewhere else in the same tab.
  content::TestNavigationObserver elsewhere_observer(active_tab);
  ui_test_utils::NavigateToURL(browser(), other_url);
  elsewhere_observer.Wait();
  ASSERT_TRUE(elsewhere_observer.last_navigation_succeeded());
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Now the embeddedSearch API should have gone away.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_FALSE(result);

  // Navigate back to the NTP.
  content::TestNavigationObserver back_observer(active_tab);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back_observer.Wait();
  // The API should be back.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_TRUE(result);

  // Navigate forward to the non-NTP page.
  content::TestNavigationObserver fwd_observer(active_tab);
  chrome::GoForward(browser(), WindowOpenDisposition::CURRENT_TAB);
  fwd_observer.Wait();
  // The API should be gone.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_FALSE(result);

  // Navigate to a new local NTP instance.
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeSearchLocalNtpUrl));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // Now the API should be available again.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_TRUE(result);
}

// The spare RenderProcessHost is warmed up *before* the target destination is
// known and therefore doesn't include any special command-line flags that are
// used when launching a RenderProcessHost known to be needed for NTP.  This
// test ensures that the spare RenderProcessHost doesn't accidentally end up
// being used for NTP navigations.
IN_PROC_BROWSER_TEST_F(LocalNTPTest, SpareProcessDoesntInterfereWithSearchAPI) {
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a non-NTP URL, so that the next step needs to swap the process.
  GURL non_ntp_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath().AppendASCII("title1.html"));
  ui_test_utils::NavigateToURL(browser(), non_ntp_url);
  content::RenderProcessHost* old_process =
      active_tab->GetMainFrame()->GetProcess();

  // Navigate to a local NTP while a spare process is present.
  content::RenderProcessHost::WarmupSpareRenderProcessHost(
      browser()->profile());
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeSearchLocalNtpUrl));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  // Verify that a process swap has taken place.  This is an indirect indication
  // that the spare process could have been used (during the process swap).
  // This assertion is a sanity check of the test setup, rather than
  // verification of the core thing that the test cares about.
  content::RenderProcessHost* new_process =
      active_tab->GetMainFrame()->GetProcess();
  ASSERT_NE(new_process, old_process);

  // Check that the embeddedSearch API is available - the spare
  // RenderProcessHost either shouldn't be used, or if used it should have been
  // launched with the appropriate, NTP-specific cmdline flags.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_TRUE(result);
}

// Regression test for crbug.com/776660 and crbug.com/776655.
IN_PROC_BROWSER_TEST_F(LocalNTPTest, EmbeddedSearchAPIExposesStaticFunctions) {
  // Open a local NTP.
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeSearchLocalNtpUrl));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  struct TestCase {
    const char* function_name;
    const char* args;
  } test_cases[] = {
      {"window.chrome.embeddedSearch.searchBox.paste", "\"text\""},
      {"window.chrome.embeddedSearch.searchBox.startCapturingKeyStrokes", ""},
      {"window.chrome.embeddedSearch.searchBox.stopCapturingKeyStrokes", ""},
      {"window.chrome.embeddedSearch.newTabPage.deleteMostVisitedItem", "1"},
      {"window.chrome.embeddedSearch.newTabPage.deleteMostVisitedItem",
       "\"1\""},
      {"window.chrome.embeddedSearch.newTabPage.getMostVisitedItemData", "1"},
      {"window.chrome.embeddedSearch.newTabPage.logEvent", "1"},
      {"window.chrome.embeddedSearch.newTabPage.undoAllMostVisitedDeletions",
       ""},
      {"window.chrome.embeddedSearch.newTabPage.undoMostVisitedDeletion", "1"},
      {"window.chrome.embeddedSearch.newTabPage.undoMostVisitedDeletion",
       "\"1\""},
  };

  for (const TestCase& test_case : test_cases) {
    // Make sure that the API function exists.
    bool result = false;
    ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
        active_tab, base::StringPrintf("!!%s", test_case.function_name),
        &result));
    ASSERT_TRUE(result);

    // Check that it can be called normally.
    EXPECT_TRUE(content::ExecuteScript(
        active_tab,
        base::StringPrintf("%s(%s)", test_case.function_name, test_case.args)));

    // Check that it can be called even after it's assigned to a var, i.e.
    // without a "this" binding.
    EXPECT_TRUE(content::ExecuteScript(
        active_tab,
        base::StringPrintf("var f = %s; f(%s)", test_case.function_name,
                           test_case.args)));
  }
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, EmbeddedSearchAPIEndToEnd) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  // Navigating to an NTP should trigger an update of the MV items.
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount);

  // Make sure the same number of items is available in JS.
  int most_visited_count = -1;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      active_tab, "window.chrome.embeddedSearch.newTabPage.mostVisited.length",
      &most_visited_count));
  ASSERT_EQ(kDefaultMostVisitedItemCount, most_visited_count);

  // Get the ID of one item.
  int most_visited_rid = -1;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      active_tab, "window.chrome.embeddedSearch.newTabPage.mostVisited[0].rid",
      &most_visited_rid));

  // Delete that item. The deletion should arrive on the native side.
  ASSERT_TRUE(content::ExecuteScript(
      active_tab,
      base::StringPrintf(
          "window.chrome.embeddedSearch.newTabPage.deleteMostVisitedItem(%d)",
          most_visited_rid)));
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount - 1);
}

// Regression test for crbug.com/592273.
IN_PROC_BROWSER_TEST_F(LocalNTPTest, EmbeddedSearchAPIAfterDownload) {
  // Set up a test server, so we have some URL to download.
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(test_server.Start());
  const GURL download_url = test_server.GetURL("/download-test1.lib");

  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  // Navigating to an NTP should trigger an update of the MV items.
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount);

  // Download some file.
  content::DownloadTestObserverTerminal download_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()), 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT);
  ui_test_utils::NavigateToURL(browser(), download_url);
  download_observer.WaitForFinished();

  // This should have changed the visible URL, but not the last committed one.
  ASSERT_EQ(download_url, active_tab->GetVisibleURL());
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetLastCommittedURL());

  // Make sure the same number of items is available in JS.
  int most_visited_count = -1;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      active_tab, "window.chrome.embeddedSearch.newTabPage.mostVisited.length",
      &most_visited_count));
  ASSERT_EQ(kDefaultMostVisitedItemCount, most_visited_count);

  // Get the ID of one item.
  int most_visited_rid = -1;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      active_tab, "window.chrome.embeddedSearch.newTabPage.mostVisited[0].rid",
      &most_visited_rid));

  // Since the current page is still an NTP, it should be possible to delete MV
  // items (as well as anything else that the embeddedSearch API allows).
  ASSERT_TRUE(content::ExecuteScript(
      active_tab,
      base::StringPrintf(
          "window.chrome.embeddedSearch.newTabPage.deleteMostVisitedItem(%d)",
          most_visited_rid)));
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount - 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, NTPRespectsBrowserLanguageSetting) {
  // If the platform cannot load the French locale (GetApplicationLocale() is
  // platform specific, and has been observed to fail on a small number of
  // platforms), abort the test.
  if (!local_ntp_test_utils::SwitchBrowserLanguageToFrench()) {
    LOG(ERROR) << "Failed switching to French language, aborting test.";
    return;
  }

  // Open a local NTP.
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeSearchLocalNtpUrl));

  // Verify that the NTP is in French.
  EXPECT_EQ(base::ASCIIToUTF16("Nouvel onglet"), active_tab->GetTitle());
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, GoogleNTPLoadsWithoutError) {
  // Open a new blank tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  content::WebContentsConsoleObserver console_observer(active_tab);

  base::HistogramTester histograms;

  // Navigate to the NTP.
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  bool is_google = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.configData && !!window.configData.isGooglePage",
      &is_google));
  EXPECT_TRUE(is_google);

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.messages().empty())
      << console_observer.GetMessageAt(0u);

  // Make sure load time metrics were recorded.
  histograms.ExpectTotalCount("NewTabPage.LoadTime", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.LocalNTP", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.LocalNTP.Google", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.MostVisited", 1);

  // Make sure impression metrics were recorded. There should be 1 tile, the
  // default prepopulated TopSites (see history::PrepopulatedPage).
  histograms.ExpectTotalCount("NewTabPage.NumberOfTiles", 1);
  histograms.ExpectBucketCount("NewTabPage.NumberOfTiles", 1, 1);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression", 1);
  histograms.ExpectBucketCount("NewTabPage.SuggestionsImpression", 0, 1);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression.client", 1);
  // The material design NTP shouldn't have any thumbnails.
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression.Thumbnail", 0);
  histograms.ExpectTotalCount("NewTabPage.TileTitle", 1);
  histograms.ExpectTotalCount("NewTabPage.TileTitle.client", 1);
  histograms.ExpectTotalCount("NewTabPage.TileType", 1);
  histograms.ExpectTotalCount("NewTabPage.TileType.client", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, NonGoogleNTPLoadsWithoutError) {
  local_ntp_test_utils::SetUserSelectedDefaultSearchProvider(
      browser()->profile(), "https://www.example.com",
      /*ntp_url=*/"");

  // Open a new blank tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a console observer, listening for any message ("*" pattern).
  content::WebContentsConsoleObserver console_observer(active_tab);

  base::HistogramTester histograms;

  // Navigate to the NTP.
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  bool is_google = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.configData && !!window.configData.isGooglePage",
      &is_google));
  EXPECT_FALSE(is_google);

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.messages().empty())
      << console_observer.GetMessageAt(0u);

  // Make sure load time metrics were recorded.
  histograms.ExpectTotalCount("NewTabPage.LoadTime", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.LocalNTP", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.LocalNTP.Other", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.MostVisited", 1);

  // Make sure impression metrics were recorded. There should be 1 tile, the
  // default prepopulated TopSites (see history::PrepopulatedPage).
  histograms.ExpectTotalCount("NewTabPage.NumberOfTiles", 1);
  histograms.ExpectBucketCount("NewTabPage.NumberOfTiles", 1, 1);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression", 1);
  histograms.ExpectBucketCount("NewTabPage.SuggestionsImpression", 0, 1);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression.client", 1);
  // The material design NTP shouldn't have any thumbnails.
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression.Thumbnail", 0);
  histograms.ExpectTotalCount("NewTabPage.TileTitle", 1);
  histograms.ExpectTotalCount("NewTabPage.TileTitle.client", 1);
  histograms.ExpectTotalCount("NewTabPage.TileType", 1);
  histograms.ExpectTotalCount("NewTabPage.TileType.client", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, FrenchGoogleNTPLoadsWithoutError) {
  if (!local_ntp_test_utils::SwitchBrowserLanguageToFrench()) {
    LOG(ERROR) << "Failed switching to French language, aborting test.";
    return;
  }

  // Open a new blank tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  content::WebContentsConsoleObserver console_observer(active_tab);

  // Navigate to the NTP and make sure it's actually in French.
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  ASSERT_EQ(base::ASCIIToUTF16("Nouvel onglet"), active_tab->GetTitle());

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.messages().empty())
      << console_observer.GetMessageAt(0u);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, LoadsMDIframe) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Get the Most Visited iframe and check that the tiles loaded correctly.
  content::RenderFrameHost* iframe = GetIframe(active_tab, kMostVisitedIframe);

  // Get the total number of (non-empty) tiles from the iframe.
  int total_favicons = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      iframe, "document.querySelectorAll('.md-icon').length", &total_favicons));
  // Also get how many of the tiles succeeded and failed in loading their
  // favicon images.
  int succeeded_favicons = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      iframe,
      "document.querySelectorAll('.md-icon:not(.failed-favicon)').length",
      &succeeded_favicons));
  int failed_favicons = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      iframe, "document.querySelectorAll('.md-icon.failed-favicon').length",
      &failed_favicons));
  // And check if only one add button exists in the frame.
  int add_button_favicon = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      iframe, "document.querySelectorAll('.md-add-icon').length",
      &add_button_favicon));
  EXPECT_EQ(1, add_button_favicon);

  // First, sanity check that the numbers line up (none of the css classes was
  // renamed, etc).
  EXPECT_EQ(total_favicons, succeeded_favicons + failed_favicons);

  // Since we're in a non-signed-in, fresh profile with no history, there should
  // be the default TopSites tiles (see history::PrepopulatedPage).
  // Check that there is at least one tile, and that all of them loaded their
  // images successfully.
  EXPECT_EQ(total_favicons, succeeded_favicons);
  EXPECT_EQ(0, failed_favicons);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, DontShowAddCustomLinkButtonWhenMaxLinks) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount);

  // Get the Most Visited iframe and add to maximum number of tiles.
  content::RenderFrameHost* iframe = GetIframe(active_tab, kMostVisitedIframe);
  for (int i = kDefaultMostVisitedItemCount; i < kDefaultCustomLinkMaxCount;
       ++i) {
    std::string rid = std::to_string(i + 100);
    std::string url = "https://" + rid + ".com";
    std::string title = "url for " + rid;
    // Add most visited tiles via the EmbeddedSearch API. rid = -1 means add new
    // most visited tile.
    local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
        iframe,
        "window.chrome.embeddedSearch.newTabPage.updateCustomLink(-1, '" + url +
            "', '" + title + "')");
  }
  // Confirm that there are max number of custom link tiles.
  observer.WaitForMostVisitedItems(kDefaultCustomLinkMaxCount);

  // Check there is no add button in the iframe. Make sure not to select from
  // old tiles that are in the process of being deleted.
  bool no_add_button = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe,
      "document.querySelectorAll('#mv-tiles .md-add-icon').length === 0",
      &no_add_button));
  EXPECT_TRUE(no_add_button);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, ReorderCustomLinks) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount);

  // Fill tiles up to the maximum count.
  content::RenderFrameHost* iframe = GetIframe(active_tab, kMostVisitedIframe);
  for (int i = kDefaultMostVisitedItemCount; i < kDefaultCustomLinkMaxCount;
       ++i) {
    std::string rid = std::to_string(i + 100);
    std::string url = "https://" + rid + ".com";
    std::string title = "url for " + rid;
    local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
        iframe,
        "window.chrome.embeddedSearch.newTabPage.updateCustomLink(-1, '" + url +
            "', '" + title + "')");
  }
  // Confirm that there are max number of custom link tiles.
  observer.WaitForMostVisitedItems(kDefaultCustomLinkMaxCount);

  // Get the title of the tile at index 1. Make sure not to select from old
  // tiles that are in the process of being deleted.
  std::string title;
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      iframe, "document.querySelectorAll('#mv-tiles .md-title')[1].innerText",
      &title));

  // Move the tile to the front.
  std::string rid;
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      iframe,
      "document.querySelectorAll('#mv-tiles "
      ".md-tile')[1].getAttribute('data-rid')",
      &rid));
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe, "window.chrome.embeddedSearch.newTabPage.reorderCustomLink(" +
                  rid + ", 0)");

  // Check that the first tile is the tile that was moved.
  std::string new_title;
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      iframe, "document.querySelectorAll('#mv-tiles .md-title')[0].innerText",
      &new_title));
  EXPECT_EQ(new_title, title);

  // Move the tile again to the end.
  std::string end_index = std::to_string(kDefaultCustomLinkMaxCount - 1);
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      iframe,
      "document.querySelectorAll('#mv-tiles "
      ".md-tile')[0].getAttribute('data-rid')",
      &rid));
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe, "window.chrome.embeddedSearch.newTabPage.reorderCustomLink(" +
                  rid + ", " + end_index + ")");

  // Check that the last tile is the tile that was moved.
  new_title = std::string();
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      iframe,
      "document.querySelectorAll('#mv-tiles .md-title')[" + end_index +
          "].innerText",
      &new_title));
  EXPECT_EQ(new_title, title);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest,
                       ToggleShortcutType_WithoutCustomLinksInitialized) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount);

  content::RenderFrameHost* iframe = GetIframe(active_tab, kMostVisitedIframe);

  // Assert that custom links is enabled (which should be by default). If so,
  // the tiles will have an edit menu.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!window.chrome.embeddedSearch.newTabPage.isUsingMostVisited",
      &result));
  ASSERT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!!document.querySelector('#mv-tiles .md-edit-menu')", &result));
  ASSERT_TRUE(result);

  // Enable Most Visited sites and disable custom links.
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe,
      "window.chrome.embeddedSearch.newTabPage.toggleMostVisitedOrCustomLinks("
      ")");

  // Check that custom links is disabled. The tiles should not have an edit
  // menu.
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "window.chrome.embeddedSearch.newTabPage.isUsingMostVisited",
      &result));
  EXPECT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!document.querySelector('#mv-tiles .md-edit-menu')", &result));
  EXPECT_TRUE(result);

  // Disable Most Visited sites and enable custom links.
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe,
      "window.chrome.embeddedSearch.newTabPage.toggleMostVisitedOrCustomLinks("
      ")");

  // Check if custom links is enabled. The tiles should have an edit menu.
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!window.chrome.embeddedSearch.newTabPage.isUsingMostVisited",
      &result));
  EXPECT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!!document.querySelector('#mv-tiles .md-edit-menu')", &result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest,
                       ToggleShortcutType_WithCustomLinksInitialized) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount);

  // Initialize custom links by adding a shortcut.
  content::RenderFrameHost* iframe = GetIframe(active_tab, kMostVisitedIframe);
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe,
      "window.chrome.embeddedSearch.newTabPage.updateCustomLink(-1, "
      "'https://1.com', 'Title1')");
  // Confirm that there are the correct number of custom link tiles.
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount + 1);

  // Assert that custom links is enabled (which should be by default). If so,
  // the tiles will have an edit menu.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!window.chrome.embeddedSearch.newTabPage.isUsingMostVisited",
      &result));
  ASSERT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!!document.querySelector('#mv-tiles .md-edit-menu')", &result));
  ASSERT_TRUE(result);

  // Enable Most Visited sites and disable custom links.
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe,
      "window.chrome.embeddedSearch.newTabPage.toggleMostVisitedOrCustomLinks("
      ")");

  // Check that custom links is disabled. The tiles should not have an edit
  // menu.
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "window.chrome.embeddedSearch.newTabPage.isUsingMostVisited",
      &result));
  EXPECT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!document.querySelector('#mv-tiles .md-edit-menu')", &result));
  EXPECT_TRUE(result);

  // Disable Most Visited sites and enable custom links.
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe,
      "window.chrome.embeddedSearch.newTabPage.toggleMostVisitedOrCustomLinks("
      ")");

  // Check if custom links is enabled. The tiles should have an edit menu.
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!window.chrome.embeddedSearch.newTabPage.isUsingMostVisited",
      &result));
  EXPECT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!!document.querySelector('#mv-tiles .md-edit-menu')", &result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, ToggleShortcutsVisibility) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount);

  // Initialize custom links by adding a shortcut.
  content::RenderFrameHost* iframe = GetIframe(active_tab, kMostVisitedIframe);
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe,
      "window.chrome.embeddedSearch.newTabPage.updateCustomLink(-1, "
      "'https://1.com', 'Title1')");
  // Confirm that there are the correct number of custom link tiles.
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount + 1);

  // Check that the shortcuts are visible.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "window.chrome.embeddedSearch.newTabPage.areShortcutsVisible",
      &result));
  ASSERT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "document.getElementById('most-visited').hidden", &result));
  ASSERT_FALSE(result);

  // Hide the shortcuts.
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe,
      "window.chrome.embeddedSearch.newTabPage.toggleShortcutsVisibility("
      "true)");

  // Check that the shortcuts are hidden.
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!window.chrome.embeddedSearch.newTabPage.areShortcutsVisible",
      &result));
  EXPECT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "document.getElementById('most-visited').hidden", &result));
  EXPECT_TRUE(result);

  // Show the shortcuts.
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe,
      "window.chrome.embeddedSearch.newTabPage.toggleShortcutsVisibility("
      "true)");

  // Check that the shortcuts are visible.
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "window.chrome.embeddedSearch.newTabPage.areShortcutsVisible",
      &result));
  EXPECT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "document.getElementById('most-visited').hidden", &result));
  EXPECT_FALSE(result);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, ToggleShortcutVisibilityAndType) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount);

  // Initialize custom links by adding a shortcut.
  content::RenderFrameHost* iframe = GetIframe(active_tab, kMostVisitedIframe);
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe,
      "window.chrome.embeddedSearch.newTabPage.updateCustomLink(-1, "
      "'https://1.com', 'Title1')");
  // Confirm that there are the correct number of custom link tiles.
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount + 1);

  // Check that the shortcuts are visible.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "window.chrome.embeddedSearch.newTabPage.areShortcutsVisible",
      &result));
  ASSERT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "document.getElementById('most-visited').hidden", &result));
  ASSERT_FALSE(result);

  // Hide the shortcuts and immediately enable Most Visited sites. The
  // successive calls should not interfere with each other, and only a single
  // update should be sent.
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe,
      "window.chrome.embeddedSearch.newTabPage.toggleShortcutsVisibility("
      "false); "
      "window.chrome.embeddedSearch.newTabPage.toggleMostVisitedOrCustomLinks("
      ")");
  // Confirm that there are the correct number of Most Visited tiles.
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount);

  // Check that the tiles have updated properly, i.e. the tiles should not have
  // an edit menu nor be visible.
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "window.chrome.embeddedSearch.newTabPage.isUsingMostVisited",
      &result));
  EXPECT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!document.querySelector('#mv-tiles .md-edit-menu')", &result));
  EXPECT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!window.chrome.embeddedSearch.newTabPage.areShortcutsVisible",
      &result));
  EXPECT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "document.getElementById('most-visited').hidden", &result));
  EXPECT_TRUE(result);

  // Show the shortcuts and immediately enable custom links.
  local_ntp_test_utils::ExecuteScriptOnNTPAndWaitUntilLoaded(
      iframe,
      "window.chrome.embeddedSearch.newTabPage.toggleShortcutsVisibility("
      "false); "
      "window.chrome.embeddedSearch.newTabPage.toggleMostVisitedOrCustomLinks("
      ")");
  // Confirm that there are the correct number of custom link tiles.
  observer.WaitForMostVisitedItems(kDefaultMostVisitedItemCount + 1);

  // Check that the tiles have updated properly, i.e. the tiles should have an
  // edit menu and be visible.
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!window.chrome.embeddedSearch.newTabPage.isUsingMostVisited",
      &result));
  EXPECT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "!!document.querySelector('#mv-tiles .md-edit-menu')", &result));
  EXPECT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      iframe, "window.chrome.embeddedSearch.newTabPage.areShortcutsVisible",
      &result));
  EXPECT_TRUE(result);
  result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "document.getElementById('most-visited').hidden", &result));
  EXPECT_FALSE(result);
}

class LocalNTPRTLTest : public LocalNTPTest {
 public:
  LocalNTPRTLTest() {}

 private:
  void SetUpCommandLine(base::CommandLine* cmdline) override {
    cmdline->AppendSwitchASCII(switches::kForceUIDirection,
                               switches::kForceDirectionRTL);
  }
};

IN_PROC_BROWSER_TEST_F(LocalNTPRTLTest, RightToLeft) {
  // Open a local NTP.
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeSearchLocalNtpUrl));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // Check that the "dir" attribute on the main "html" element says "rtl".
  std::string dir;
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      active_tab, "document.documentElement.dir", &dir));
  EXPECT_EQ("rtl", dir);
}

// TODO(crbug/980638): Update/Remove when Linux and/or ChromeOS support dark
// mode.
#if defined(OS_WIN) || defined(OS_MAC)

// Tests that dark mode styling is properly applied to the local NTP.
class LocalNTPDarkModeTest : public LocalNTPTest, public DarkModeTestBase {
 public:
  LocalNTPDarkModeTest() {}

 private:
  void SetUpOnMainThread() override {
    LocalNTPTest::SetUpOnMainThread();

    theme()->AddColorSchemeNativeThemeObserver(
        ui::NativeTheme::GetInstanceForWeb());
  }
};

IN_PROC_BROWSER_TEST_F(LocalNTPDarkModeTest, ToggleDarkMode) {
  // Initially disable dark mode.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  theme()->SetDarkMode(false);
  instant_service->SetNativeThemeForTesting(theme());
  theme()->NotifyObservers();

  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  content::RenderFrameHost* mv_iframe =
      GetIframe(active_tab, kMostVisitedIframe);
  content::RenderFrameHost* cl_iframe =
      GetIframe(active_tab, kEditCustomLinkIframe);

  // Dark mode should not be applied to the main page and iframes.
  ASSERT_FALSE(GetIsDarkModeApplied(active_tab));
  ASSERT_FALSE(GetIsDarkModeApplied(mv_iframe));
  ASSERT_FALSE(GetIsDarkModeApplied(cl_iframe));
  ASSERT_TRUE(GetIsLightChipsApplied(active_tab));

  // Enable dark mode and wait until the MV tiles have updated.
  theme()->SetDarkMode(true);
  theme()->NotifyObservers();

  // Check that dark mode has been properly applied.
  EXPECT_TRUE(GetIsDarkModeApplied(active_tab));
  EXPECT_TRUE(GetIsDarkModeApplied(mv_iframe));
  EXPECT_TRUE(GetIsDarkModeApplied(cl_iframe));
  EXPECT_FALSE(GetIsLightChipsApplied(active_tab));

  // Disable dark mode and wait until the MV tiles have updated.
  theme()->SetDarkMode(false);
  theme()->NotifyObservers();

  // Check that dark mode has been removed.
  EXPECT_FALSE(GetIsDarkModeApplied(active_tab));
  EXPECT_FALSE(GetIsDarkModeApplied(mv_iframe));
  EXPECT_FALSE(GetIsDarkModeApplied(cl_iframe));
  EXPECT_TRUE(GetIsLightChipsApplied(active_tab));
}

// Tests that dark mode styling is properly applied to the local NTP on start-
// up. The test parameter controls whether dark mode is initially enabled or
// disabled.
class LocalNTPDarkModeStartupTest : public LocalNTPDarkModeTest,
                                    public testing::WithParamInterface<bool> {
 public:
  LocalNTPDarkModeStartupTest() {}

  bool DarkModeEnabled() { return GetParam(); }

 private:
  void SetUpOnMainThread() override {
    LocalNTPTest::SetUpOnMainThread();

    theme()->AddColorSchemeNativeThemeObserver(
        ui::NativeTheme::GetInstanceForWeb());

    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(browser()->profile());
    theme()->SetDarkMode(GetParam());
    instant_service->SetNativeThemeForTesting(theme());
    theme()->NotifyObservers();
  }
};

IN_PROC_BROWSER_TEST_P(LocalNTPDarkModeStartupTest, DarkModeApplied) {
  const bool kDarkModeEnabled = DarkModeEnabled();
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  content::RenderFrameHost* mv_iframe =
      GetIframe(active_tab, kMostVisitedIframe);
  content::RenderFrameHost* cl_iframe =
      GetIframe(active_tab, kEditCustomLinkIframe);

  // Check that dark mode, if enabled, has been properly applied to the main
  // page and iframes.
  EXPECT_EQ(kDarkModeEnabled, GetIsDarkModeApplied(active_tab));
  EXPECT_EQ(kDarkModeEnabled, GetIsDarkModeApplied(mv_iframe));
  EXPECT_EQ(kDarkModeEnabled, GetIsDarkModeApplied(cl_iframe));
}

INSTANTIATE_TEST_SUITE_P(All, LocalNTPDarkModeStartupTest, testing::Bool());

#endif

IN_PROC_BROWSER_TEST_F(LocalNTPTest, ErrorPagesAreNotNTPs) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  // Trigger an error page by navigating to an invalid url.
  const GURL invalid_url = GURL("https://invalid.test");
  ui_test_utils::NavigateToURL(browser(), invalid_url);
  ASSERT_EQ(invalid_url, active_tab->GetVisibleURL());
  // The error page is not an NTP.
  EXPECT_FALSE(search::IsInstantNTP(active_tab));

  // Navigate back to the NTP.
  content::TestNavigationObserver back_observer(active_tab);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back_observer.Wait();
  // Now the page should be an NTP again.
  EXPECT_TRUE(search::IsInstantNTP(active_tab));
}

// This is a regression test for https://crbug.com/946489 and
// https://crbug.com/963544 which say that clicking a most-visited link from an
// NTP should 1) have `Sec-Fetch-Site: none` header and 2) have SameSite
// cookies.  In other words - NTP navigations should be treated as if they were
// browser-initiated (like following a bookmark through trusted Chrome UI).
IN_PROC_BROWSER_TEST_F(LocalNTPTest,
                       NtpNavigationsAreTreatedAsBrowserInitiated) {
  // Set up a test server for inspecting cookies and headers.
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  // Have the server set a SameSite cookie.
  GURL cookie_url(test_server.GetURL(
      "/set-cookie?same-site-cookie=1;SameSite=Strict;httponly"));
  ui_test_utils::NavigateToURL(browser(), cookie_url);

  // Open a local NTP.
  content::WebContents* ntp_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeSearchLocalNtpUrl));

  // Inject and click a link to foo.com/echoall and wait for the navigation to
  // succeed.
  GURL echo_all_url(test_server.GetURL("/echoall"));
  const char* kNavScriptTemplate = R"(
      var a = document.createElement('a');
      a.href = $1;
      a.innerText = 'Simulated most-visited link';
      document.body.appendChild(a);
      a.click();
  )";
  content::TestNavigationObserver nav_observer(ntp_tab);
  content::NavigationHandleObserver handle_observer(ntp_tab, echo_all_url);
  ASSERT_TRUE(content::ExecuteScript(
      ntp_tab, content::JsReplace(kNavScriptTemplate, echo_all_url)));
  nav_observer.Wait();
  ASSERT_TRUE(nav_observer.last_navigation_succeeded());
  ASSERT_FALSE(handle_observer.is_error());
  ASSERT_FALSE(search::IsInstantNTP(ntp_tab));

  // Extract and verify request headers reported via /echoall test page.
  const char* kHeadersExtractionScript =
      "document.getElementsByTagName('pre')[1].innerText;";
  std::string request_headers =
      content::EvalJs(ntp_tab, kHeadersExtractionScript).ExtractString();
  EXPECT_THAT(request_headers, ::testing::HasSubstr("Sec-Fetch-Site: none"));
  EXPECT_THAT(request_headers, ::testing::HasSubstr("same-site-cookie=1"));

  // Verify other navigation properties.
  EXPECT_FALSE(nav_observer.last_initiator_origin().has_value());
  EXPECT_FALSE(handle_observer.is_renderer_initiated());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                           handle_observer.page_transition()));
}

// This is a regression test for https://crbug.com/1020610 - it verifies that
// NTP navigations do show up as a pending navigation.
IN_PROC_BROWSER_TEST_F(LocalNTPTest, PendingNavigations) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open a local NTP.
  content::WebContents* ntp_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeSearchLocalNtpUrl));

  // Inject and click a link to foo.com/hung and wait for the navigation to
  // start.
  GURL slow_url(embedded_test_server()->GetURL("/hung"));
  const char* kNavScriptTemplate = R"(
      var a = document.createElement('a');
      a.href = $1;
      a.innerText = 'Simulated most-visited link';
      document.body.appendChild(a);
      a.click();
  )";
  content::TestNavigationManager nav_manager(ntp_tab, slow_url);
  ASSERT_TRUE(content::ExecuteScript(
      ntp_tab, content::JsReplace(kNavScriptTemplate, slow_url)));
  ASSERT_TRUE(nav_manager.WaitForRequestStart());

  // Verify that the visible entry points at the |slow_url|.
  content::NavigationEntry* pending_entry =
      ntp_tab->GetController().GetPendingEntry();
  ASSERT_TRUE(pending_entry);
  content::NavigationEntry* visible_entry =
      ntp_tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(visible_entry);
  content::NavigationEntry* committed_entry =
      ntp_tab->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(committed_entry);
  EXPECT_EQ(visible_entry, pending_entry);
  EXPECT_EQ(slow_url, visible_entry->GetURL());
  EXPECT_NE(pending_entry, committed_entry);
  EXPECT_NE(slow_url, committed_entry->GetURL());

  // Verify that the omnibox displays |slow_url|.
  OmniboxView* view = browser()->window()->GetLocationBar()->GetOmniboxView();
  // Depending on field trial configuration, the omnibox text might contain the
  // full URL or just a portion of it.
  if (base::FeatureList::IsEnabled(
          omnibox::kRevealSteadyStateUrlPathQueryAndRefOnHover)) {
    EXPECT_EQ(base::ASCIIToUTF16(slow_url.spec()), view->GetText());
  } else {
    std::string omnibox_text = base::UTF16ToUTF8(view->GetText());
    EXPECT_THAT(omnibox_text, ::testing::StartsWith(slow_url.host()));
    EXPECT_THAT(omnibox_text, ::testing::EndsWith(slow_url.path()));
    EXPECT_THAT(slow_url.spec(), ::testing::EndsWith(omnibox_text));
  }
}

// Verifies that Chrome won't spawn a separate renderer process for
// every single NTP tab.  This behavior goes all the way back to
// the initial commit [1] which achieved that behavior by forcing
// process-per-site mode for NTP tabs.  It seems desirable to preserve this
// behavior going forward.
//
// [1] https://chromium.googlesource.com/chromium/src/+/09911bf300f1a419907a9412154760efd0b7abc3/chrome/browser/browsing_instance.cc#55
IN_PROC_BROWSER_TEST_F(LocalNTPTest, ProcessPerSite) {
  GURL ntp_url(base::FeatureList::IsEnabled(ntp_features::kWebUI)
                   ? chrome::kChromeUINewTabPageURL
                   : chrome::kChromeSearchLocalNtpUrl);

  // Open NTP in |tab1|.
  content::WebContents* tab1;
  {
    content::WebContentsAddedObserver tab1_observer;

    // Try to simulate as closely as possible what would have happened in the
    // real user interaction.  In particular, do *not* use
    // local_ntp_test_utils::OpenNewTab, which requires the caller to specify
    // the URL of the new tab.
    chrome::NewTab(browser());

    // Wait for the new tab.
    tab1 = tab1_observer.GetWebContents();
    ASSERT_TRUE(WaitForLoadStop(tab1));

    // Sanity check: the NTP should be provided by |ntp_url| (and not by
    // chrome-search://remote-ntp [3rd-party NTP] or chrome://ntp [incognito]).
    std::string loc;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        tab1, "domAutomationController.send(window.location.href)", &loc));
    EXPECT_EQ(ntp_url, GURL(loc));
  }

  // Open another NTP in |tab2|.
  content::WebContents* tab2;
  {
    content::WebContentsAddedObserver tab2_observer;
    chrome::NewTab(browser());
    tab2 = tab2_observer.GetWebContents();
    ASSERT_TRUE(WaitForLoadStop(tab2));
    std::string loc;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        tab2, "domAutomationController.send(window.location.href)", &loc));
    EXPECT_EQ(ntp_url, GURL(loc));
  }

  // Verify that |tab1| and |tab2| share a process.
  EXPECT_EQ(tab1->GetMainFrame()->GetProcess(),
            tab2->GetMainFrame()->GetProcess());
}

// Just like LocalNTPTest.ProcessPerSite, but for an incognito window.
IN_PROC_BROWSER_TEST_F(LocalNTPTest, ProcessPerSite_Incognito) {
  GURL ntp_url("chrome://newtab");
  Browser* incognito_browser = new Browser(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(), true));

  // Open NTP in |tab1|.
  content::WebContents* tab1;
  {
    content::WebContentsAddedObserver tab1_observer;

    // Try to simulate as closely as possible what would have happened in the
    // real user interaction.  In particular, do *not* use
    // local_ntp_test_utils::OpenNewTab, which requires the caller to specify
    // the URL of the new tab.
    chrome::NewTab(incognito_browser);

    // Wait for the new tab.
    tab1 = tab1_observer.GetWebContents();
    ASSERT_TRUE(WaitForLoadStop(tab1));

    // Sanity check: the NTP should be provided by |ntp_url| (and not by
    // chrome-search://local-ntp [1st-party, non-incognito NTP] or
    // chrome-search://remote-ntp [3rd-party NTP]).
    std::string loc;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        tab1, "domAutomationController.send(window.location.href)", &loc));
    EXPECT_EQ(ntp_url, GURL(loc));
  }

  // Open another NTP in |tab2|.
  content::WebContents* tab2;
  {
    content::WebContentsAddedObserver tab2_observer;
    chrome::NewTab(incognito_browser);
    tab2 = tab2_observer.GetWebContents();
    ASSERT_TRUE(WaitForLoadStop(tab2));
    std::string loc;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        tab2, "domAutomationController.send(window.location.href)", &loc));
    EXPECT_EQ(ntp_url, GURL(loc));
  }

  // Verify that |tab1| and |tab2| share a process.
  EXPECT_EQ(tab1->GetMainFrame()->GetProcess(),
            tab2->GetMainFrame()->GetProcess());
}

}  // namespace
