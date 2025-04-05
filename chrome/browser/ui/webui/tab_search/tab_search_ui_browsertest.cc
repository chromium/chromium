// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"

#include <string>

#include "base/containers/contains.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/webui/buildflags.h"

#if BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/webui_util_desktop.h"
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
  int tab_count = browser()->tab_strip_model()->GetTabCount();
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

IN_PROC_BROWSER_TEST_F(TabSearchUIBrowserTest, CloseTabAction) {
  ASSERT_EQ(4, browser()->tab_strip_model()->GetTabCount());

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
  int tab_count = browser()->tab_strip_model()->GetTabCount();
  ASSERT_EQ(3, tab_count);

  std::vector<tabs::TabHandle> open_tab_ids(tab_count);
  for (int tab_index = 0; tab_index < tab_count; tab_index++) {
    open_tab_ids.push_back(
        browser()->tab_strip_model()->GetTabAtIndex(tab_index)->GetHandle());
  }
  ASSERT_FALSE(base::Contains(open_tab_ids, tab_id));
}

// When hosting the Tab Search UI as a browser tab, ensure that closing the tab
// hosting Tab Search does not result in any UAF errors. Test for regression
// (https://crbug.com/1175507).
IN_PROC_BROWSER_TEST_F(TabSearchUIBrowserTest,
                       CloseTabSearchAsBrowserTabDoesNotCrash) {
  AppendTab(chrome::kChromeUITabSearchURL);
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(5, tab_strip_model->GetTabCount());
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
  ASSERT_EQ(4, tab_strip_model->GetTabCount());

  // Check to make sure the browser tab hosting Tab Search has been closed but
  // the rest remain.
  int tab_count = tab_strip_model->GetTabCount();
  ASSERT_EQ(4, tab_count);

  std::vector<tabs::TabHandle> open_tab_ids(tab_count);
  for (int tab_index = 0; tab_index < tab_count; tab_index++) {
    open_tab_ids.push_back(
        tab_strip_model->GetTabAtIndex(tab_index)->GetHandle());
  }
  ASSERT_FALSE(base::Contains(open_tab_ids, tab_id));
}

#if BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
class TabSearchUIBundledCodeCacheBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  TabSearchUIBundledCodeCacheBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(features::kWebUIBundledCodeCache,
                                              WebUIBundledCodeCacheEnabled());

    // Bundled code caching should be resillient to fieldtrial variations.
    if (ShouldEnableFieldTrialTestingConfig()) {
      variations::EnableTestingConfig();
    } else {
      variations::DisableTestingConfig();
    }
  }

  bool WebUIBundledCodeCacheEnabled() const { return std::get<0>(GetParam()); }
  bool ShouldEnableFieldTrialTestingConfig() const {
    return std::get<1>(GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(TabSearchUIBundledCodeCacheBrowserTest,
                       SuccessfullyLoadsCodeCache) {
  // Assert the bundled code-cache map is non-empty.
  EXPECT_FALSE(webui::GetWebUIResourceUrlToCodeCacheMap().empty());

  base::HistogramTester histogram_tester;
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Blink.ResourceRequest.WebUIBundledCodeCacheFetcher."
                "DidReceiveCachedCode",
                true),
            0);
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Blink.ResourceRequest.WebUIBundledCachedMetadataHandler."
                "ConsumeCache",
                true),
            0);

  // Load tab search and collect all renderer metrics.
  content::WaitForLoadStop(chrome::AddAndReturnTabAt(
      browser(), GURL(chrome::kChromeUITabSearchURL), -1, /*foreground=*/true));
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Assert code cache resources were successfully fetched and loaded by blink.
  const int received_success_count = histogram_tester.GetBucketCount(
      "Blink.ResourceRequest.WebUIBundledCodeCacheFetcher.DidReceiveCachedCode",
      true);
  const int consumed_success_count = histogram_tester.GetBucketCount(
      "Blink.ResourceRequest.WebUIBundledCachedMetadataHandler.ConsumeCache",
      true);
  if (WebUIBundledCodeCacheEnabled()) {
    EXPECT_GT(received_success_count, 0);
    EXPECT_GT(consumed_success_count, 0);
  } else {
    EXPECT_EQ(received_success_count, 0);
    EXPECT_EQ(consumed_success_count, 0);
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
