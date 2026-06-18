// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"

#include <algorithm>
#include <string>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/mapping/metrics_mapping_features.h"
#include "components/metrics/mapping/metrics_name_mapping.pb.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/webui/buildflags.h"

#if BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/util/webui_util_desktop.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/common/content_features.h"
#include "ui/webui/resources/grit/webui_code_cache_resources_map.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)

class TabSearchUIBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    AppendTab(chrome::kChromeUISettingsURL);
    AppendTab(chrome::kChromeUIHistoryURL);
    AppendTab(chrome::kChromeUIBookmarksURL);

    webui_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile()));

    webui_contents_->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(
            GURL(chrome::kChromeUITabSearchURL)));

    // Finish loading after initializing.
    ASSERT_TRUE(content::WaitForLoadStop(webui_contents_.get()));
  }

  void TearDownOnMainThread() override { webui_contents_.reset(); }

  void AppendTab(std::string url) {
    chrome::AddTabAt(browser(), GURL(url), -1, true);
  }

  tabs::TabInterface* GetActiveTab() {
    return browser()->tab_strip_model()->GetActiveTab();
  }

  TabSearchUI* GetWebUIController() {
    return webui_contents_->GetWebUI()
        ->GetController()
        ->template GetAs<TabSearchUI>();
  }

 protected:
  std::unique_ptr<content::WebContents> webui_contents_;
};

// TODO(romanarora): Investigate a way to call WebUI custom methods and refactor
// JS code below.

// TODO(crbug.com/407949601): Fix and re-enable
IN_PROC_BROWSER_TEST_F(TabSearchUIBrowserTest, DISABLED_InitialTabItemsListed) {
  constexpr int expected_tab_item_count = 4;
  const std::string tab_item_count_js = base::StringPrintf(
      "new Promise((resolve) => {"
      "  const interval = setInterval(() => {"
      "    const tabItems = document.querySelector('tab-search-app').shadowRoot"
      "        .querySelector('tab-search-page').shadowRoot"
      "        .getElementById('tabsList')"
      "        .querySelectorAll('tab-search-item');"
      "    if (tabItems && tabItems.length === %d) {"
      "      resolve(tabItems.length);"
      "      clearInterval(interval);"
      "    }"
      "  }, 100);"
      "});",
      expected_tab_item_count);
  int tab_item_count = content::EvalJs(webui_contents_.get(), tab_item_count_js,
                                       content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                       ISOLATED_WORLD_ID_CHROME_INTERNAL)
                           .ExtractInt();
  ASSERT_EQ(expected_tab_item_count, tab_item_count);
}

// Flaky - see https://crbug.com/40932977
IN_PROC_BROWSER_TEST_F(TabSearchUIBrowserTest, DISABLED_SwitchToTabAction) {
  int tab_count = browser()->tab_strip_model()->count();
  tabs::TabHandle tab_id =
      browser()->tab_strip_model()->GetTabAtIndex(tab_count - 1)->GetHandle();
  ASSERT_EQ(tab_id, GetActiveTab()->GetHandle());

  tab_id = browser()->tab_strip_model()->GetTabAtIndex(0)->GetHandle();

  const std::string tab_item_js = base::StringPrintf(
      "document.querySelector('tab-search-app').shadowRoot"
      "    .querySelector('tab-search-page').shadowRoot"
      "    .getElementById('tabsList')"
      "    .querySelector('tab-search-item[id=\"%s\"]')",
      base::NumberToString(tab_id.raw_value()).c_str());
  ASSERT_TRUE(content::ExecJs(webui_contents_.get(), tab_item_js + ".click()",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              ISOLATED_WORLD_ID_CHROME_INTERNAL));
  ASSERT_EQ(tab_id, GetActiveTab()->GetHandle());
}

// TODO(https://crbug.com/401303184): Disabled due to excessive flakiness.
IN_PROC_BROWSER_TEST_F(TabSearchUIBrowserTest, DISABLED_CloseTabAction) {
  ASSERT_EQ(4, browser()->tab_strip_model()->count());

  tabs::TabHandle tab_id =
      browser()->tab_strip_model()->GetTabAtIndex(0)->GetHandle();

  const std::string tab_item_button_js = base::StringPrintf(
      "document.querySelector('tab-search-app').shadowRoot"
      "    .querySelector('tab-search-page').shadowRoot"
      "    .getElementById('tabsList')"
      "    .querySelector('tab-search-item[id=\"%s\"]')"
      "    .shadowRoot.getElementById('closeButton')",
      base::NumberToString(tab_id.raw_value()).c_str());
  ASSERT_TRUE(content::ExecJs(webui_contents_.get(),
                              tab_item_button_js + ".click()",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              ISOLATED_WORLD_ID_CHROME_INTERNAL));
  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(3, tab_count);

  std::vector<tabs::TabHandle> open_tab_ids(tab_count);
  for (int tab_index = 0; tab_index < tab_count; tab_index++) {
    open_tab_ids.push_back(
        browser()->tab_strip_model()->GetTabAtIndex(tab_index)->GetHandle());
  }
  ASSERT_FALSE(std::ranges::contains(open_tab_ids, tab_id));
}

// When hosting the Tab Search UI as a browser tab, ensure that closing the tab
// hosting Tab Search does not result in any UAF errors. Test for regression
// (https://crbug.com/40054717).
IN_PROC_BROWSER_TEST_F(TabSearchUIBrowserTest,
                       CloseTabSearchAsBrowserTabDoesNotCrash) {
  AppendTab(chrome::kChromeUITabSearchURL);
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(5, tab_strip_model->count());
  content::WebContents* tab_contents = tab_strip_model->GetWebContentsAt(4);
  const tabs::TabHandle tab_id = tab_strip_model->GetTabAtIndex(4)->GetHandle();

  // Finish loading after initializing.
  ASSERT_TRUE(content::WaitForLoadStop(tab_contents));

  // WaitForLoadStop() waits for navigation commit. However, that does not
  // guarantee that the page's javascript has been run. The page's javascript
  // sends an async mojo request which results in creation of a page-handler.
  // Only after that can the test continue.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return tab_contents->GetWebUI()
        ->GetController()
        ->template GetAs<TabSearchUI>()
        ->page_handler_for_testing();
  }));
  TabSearchPageHandler* page_handler = tab_contents->GetWebUI()
                                           ->GetController()
                                           ->template GetAs<TabSearchUI>()
                                           ->page_handler_for_testing();
  ASSERT_NE(nullptr, page_handler);
  content::WebContentsDestroyedWatcher close_observer(tab_contents);
  page_handler->CloseTab(tab_id.raw_value());
  tab_contents->DispatchBeforeUnload(false /* auto_cancel */);
  close_observer.Wait();
  ASSERT_EQ(4, tab_strip_model->count());

  // Check to make sure the browser tab hosting Tab Search has been closed but
  // the rest remain.
  int tab_count = tab_strip_model->count();
  ASSERT_EQ(4, tab_count);

  std::vector<tabs::TabHandle> open_tab_ids(tab_count);
  for (int tab_index = 0; tab_index < tab_count; tab_index++) {
    open_tab_ids.push_back(
        tab_strip_model->GetTabAtIndex(tab_index)->GetHandle());
  }
  ASSERT_FALSE(std::ranges::contains(open_tab_ids, tab_id));
}

#if BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
class TabSearchUIBundledCodeCacheBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  TabSearchUIBundledCodeCacheBrowserTest() {
    // Bundled code caching should be resillient to fieldtrial variations.
    if (ShouldEnableFieldTrialTestingConfig()) {
      variations::EnableTestingConfig();
    } else {
      variations::DisableTestingConfig();
    }

    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (WebUIBundledCodeCacheEnabled()) {
      enabled_features.push_back({features::kWebUIBundledCodeCache, {}});
    } else {
      disabled_features.push_back(features::kWebUIBundledCodeCache);
    }

    if (base::FeatureList::IsEnabled(
            metrics::features::kWebiumMetricsMapping)) {
      // Construct a config that allows our specific test histograms.
      metrics::MetricsNameMappingConfiguration config;
      config.add_rules()->set_metric_name(
          "Blink.ResourceRequest.WebUIBundledCodeCacheFetcher."
          "DidReceiveCachedCode");
      config.add_rules()->set_metric_name(
          "Blink.ResourceRequest.WebUIBundledCachedMetadataHandler."
          "ConsumeCache");

      // Maintain existing guardrail metrics to match production behavior.
      config.add_rules()->set_metric_name(
          "EventLatency.GestureScrollUpdate.Touchscreen.TotalLatency");
      config.add_rules()->set_metric_name(
          "PageLoad.InteractiveTiming.InputDelay3");
      config.add_rules()->set_metric_name(
          "Graphics.Smoothness.PercentDroppedFrames3.AllSequences");

      std::string test_allowlist_config =
          base::Base64Encode(config.SerializeAsString());

      enabled_features.push_back({metrics::features::kWebiumMetricsMapping,
                                  {{"config", test_allowlist_config}}});
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  bool WebUIBundledCodeCacheEnabled() const { return std::get<0>(GetParam()); }
  bool ShouldEnableFieldTrialTestingConfig() const {
    return std::get<1>(GetParam());
  }

 protected:
  void FetchAndMergeHistograms() {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(TabSearchUIBundledCodeCacheBrowserTest,
                       SuccessfullyLoadsCodeCache) {
  if (base::FeatureList::IsEnabled(features::kInitialWebUI) &&
      ShouldEnableFieldTrialTestingConfig()) {
    GTEST_SKIP() << "Skipping test because it fails with InitialWebUI enabled. "
                    "See crbug.com/464087732.";
  }

  // Assert the bundled code-cache map is non-empty.
  EXPECT_FALSE(webui::GetWebUIResourceUrlToCodeCacheMap().empty());

  // Fetch initial histogram counts. We cannot assume these are 0 since other
  // WebUIs (e.g. InitialWebUI) might have loaded during browser startup and
  // fetched shared resources from the bundled cache. We instead verify that
  // the counts strictly increase after navigating to Tab Search.
  FetchAndMergeHistograms();

  const int initial_received_success_count = histogram_tester_.GetBucketCount(
      "Blink.ResourceRequest.WebUIBundledCodeCacheFetcher.DidReceiveCachedCode",
      true);
  const int initial_consumed_success_count = histogram_tester_.GetBucketCount(
      "Blink.ResourceRequest.WebUIBundledCachedMetadataHandler.ConsumeCache",
      true);

  // Load tab search and collect all renderer metrics.
  content::WaitForLoadStop(chrome::AddAndReturnTabAt(
      browser(), GURL(chrome::kChromeUITabSearchURL), -1, /*foreground=*/true));
  FetchAndMergeHistograms();

  // Assert code cache resources were successfully fetched and loaded by blink.
  const int received_success_count = histogram_tester_.GetBucketCount(
      "Blink.ResourceRequest.WebUIBundledCodeCacheFetcher.DidReceiveCachedCode",
      true);
  const int consumed_success_count = histogram_tester_.GetBucketCount(
      "Blink.ResourceRequest.WebUIBundledCachedMetadataHandler.ConsumeCache",
      true);

  if (WebUIBundledCodeCacheEnabled()) {
    EXPECT_GT(received_success_count, initial_received_success_count);
    EXPECT_GT(consumed_success_count, initial_consumed_success_count);
  } else {
    EXPECT_EQ(received_success_count, initial_received_success_count);
    EXPECT_EQ(consumed_success_count, initial_consumed_success_count);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    TabSearchUIBundledCodeCacheBrowserTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const ::testing::TestParamInfo<
        TabSearchUIBundledCodeCacheBrowserTest::ParamType>& info) {
      return base::StringPrintf(
          "%s_%s",
          std::get<0>(info.param) ? "BundledCodeCacheEnabled"
                                  : "BundledCodeCacheDisabled",
          std::get<1>(info.param) ? "WithFieldTrials" : "WithoutFieldTrials");
    });
#endif  // BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)

// ChromeOS has a different concept of guest profile, so we will only test
// standard desktop behavior.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_GuestModeSplitViewFavicons DISABLED_GuestModeSplitViewFavicons
#else
#define MAYBE_GuestModeSplitViewFavicons GuestModeSplitViewFavicons
#endif  // BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(TabSearchUIBrowserTest,
                       MAYBE_GuestModeSplitViewFavicons) {
  // Open guest browser.
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_OPEN_GUEST_PROFILE));
  Browser* guest_browser = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(guest_browser);
  ASSERT_TRUE(guest_browser->profile()->IsGuestSession());

  // Start embedded test server.
  ASSERT_TRUE(embedded_test_server()->Start());

  // Add two tabs with real favicons to the guest browser.
  const GURL favicon_url1 =
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html");
  chrome::AddTabAt(guest_browser, favicon_url1, -1, true);
  ASSERT_TRUE(content::WaitForLoadStop(
      guest_browser->tab_strip_model()->GetActiveWebContents()));

  const GURL favicon_url2 =
      embedded_test_server()->GetURL("/favicon/title2_with_favicon.html");
  chrome::AddTabAt(guest_browser, favicon_url2, -1, true);
  ASSERT_TRUE(content::WaitForLoadStop(
      guest_browser->tab_strip_model()->GetActiveWebContents()));

  // Active tab is favicon_url2 (at index 2). Activate tab at index 1.
  guest_browser->tab_strip_model()->ActivateTabAt(1);

  // Put them in a split view.
  guest_browser->tab_strip_model()->AddToNewSplit(
      {2}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  // Load chrome://tab-search in the guest browser.
  content::WebContents* webui_contents = chrome::AddAndReturnTabAt(
      guest_browser, GURL(chrome::kChromeUITabSearchURL), -1,
      /*foreground=*/true);
  ASSERT_TRUE(content::WaitForLoadStop(webui_contents));

  // Get the PageHandler.
  TabSearchUI* tab_search_ui = webui_contents->GetWebUI()
                                   ->GetController()
                                   ->template GetAs<TabSearchUI>();
  ASSERT_TRUE(tab_search_ui);
  TabSearchPageHandler* page_handler =
      tab_search_ui->page_handler_for_testing();
  ASSERT_TRUE(page_handler);

  // Fetch and verify the profile data once the split tabs are present and
  // their proper favicons are loaded.
  tab_search::mojom::ProfileDataPtr profile_data;
  auto get_split_tabs_loaded = [&]() -> bool {
    base::RunLoop run_loop;
    page_handler->GetProfileData(base::BindOnce(
        [](base::OnceClosure quit_closure,
           tab_search::mojom::ProfileDataPtr* out_profile_data,
           tab_search::mojom::ProfileDataPtr profile_data) {
          *out_profile_data = std::move(profile_data);
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure(), &profile_data));
    run_loop.Run();

    if (!profile_data || profile_data->windows.empty()) {
      return false;
    }

    int found_split_tabs = 0;
    for (const auto& window : profile_data->windows) {
      for (const auto& tab : window->tabs) {
        if (tab->url == favicon_url1 || tab->url == favicon_url2) {
          if (!tab->split || !tab->split_id.has_value() ||
              tab->is_default_favicon) {
            return false;
          }
          found_split_tabs++;
        }
      }
    }
    return found_split_tabs == 2;
  };

  ASSERT_TRUE(base::test::RunUntil(get_split_tabs_loaded));

  // Perform full verification on the populated profile data.
  int found_split_tabs = 0;
  std::optional<base::Token> split_id;
  for (const auto& window : profile_data->windows) {
    for (const auto& tab : window->tabs) {
      if (tab->url == favicon_url1 || tab->url == favicon_url2) {
        EXPECT_TRUE(tab->split);
        EXPECT_TRUE(tab->split_id.has_value());
        if (!split_id.has_value()) {
          split_id = tab->split_id;
        } else {
          EXPECT_EQ(split_id.value(), tab->split_id.value());
        }
        EXPECT_FALSE(tab->is_default_favicon);
        EXPECT_TRUE(tab->favicon_url.has_value());
        EXPECT_FALSE(tab->favicon_url->spec().empty());
        found_split_tabs++;
      }
    }
  }
  EXPECT_EQ(found_split_tabs, 2);
}
