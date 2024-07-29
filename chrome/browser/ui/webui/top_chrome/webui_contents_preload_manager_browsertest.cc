// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"

#include <map>

#include "base/containers/span.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/page_load_metrics/observers/non_tab_webui_page_load_metrics_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/top_chrome/preload_candidate_selector.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;

namespace {

void WaitForHistogram(const std::string& histogram_name) {
  // Continue if histogram was already recorded.
  if (base::StatisticsRecorder::FindHistogram(histogram_name)) {
    return;
  }

  // Else, wait until the histogram is recorded.
  base::RunLoop run_loop;
  auto histogram_observer =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindLambdaForTesting(
              [&](const char* histogram_name, uint64_t name_hash,
                  base::HistogramBase::Sample sample) { run_loop.Quit(); }));
  run_loop.Run();
}

int GetCommandIdForURL(GURL webui_url) {
  static const std::map<GURL, int> url_to_command_id = {
      {GURL(chrome::kChromeUITabSearchURL), IDC_TAB_SEARCH},
      {GURL(chrome::kChromeUIHistoryClustersSidePanelURL),
       IDC_SHOW_BOOKMARK_SIDE_PANEL},
      {GURL(chrome::kChromeUIBookmarksSidePanelURL),
       IDC_SHOW_HISTORY_CLUSTERS_SIDE_PANEL}};

  CHECK(url_to_command_id.contains(webui_url));

  return url_to_command_id.at(webui_url);
}

std::vector<GURL> GetAllPreloadableWebUIURLs() {
  return WebUIContentsPreloadManager::GetAllPreloadableWebUIURLsForTesting();
}

using PreloadModeName = const char*;
std::vector<PreloadModeName> GetAllPreloadManagerModes() {
  return {features::kPreloadTopChromeWebUIModePreloadOnWarmupName,
          features::kPreloadTopChromeWebUIModePreloadOnMakeContentsName};
}

class MockPreloadCandidateSelector : public webui::PreloadCandidateSelector {
 public:
  MOCK_METHOD(void, Init, (const std::vector<GURL>&), (override));
  MOCK_METHOD(std::optional<GURL>,
              GetURLToPreload,
              (const webui::PreloadContext&),
              (const, override));
};

}  // namespace

class WebUIContentsPreloadManagerBrowserTestBase : public InProcessBrowserTest {
 public:
  virtual void SetUpFeature() = 0;
  virtual void SetUpPreloadURL() = 0;

  // InProcessBrowserTest:
  void SetUp() override {
    SetUpFeature();
    auto preload_candidate_selector =
        std::make_unique<testing::NiceMock<MockPreloadCandidateSelector>>();
    preload_candidate_selector_ = preload_candidate_selector.get();
    preload_manager()->SetPreloadCandidateSelectorForTesting(
        std::move(preload_candidate_selector));
    SetUpPreloadURL();

    InProcessBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    navigation_waiter_ = std::make_unique<content::TestNavigationObserver>(
        preload_manager()->preloaded_web_contents());
    navigation_waiter_->StartWatchingNewWebContents();

    InProcessBrowserTest::SetUpOnMainThread();
  }
  void TearDown() override {
    preload_candidate_selector_ = nullptr;
    // The mock object does not expect itself to leak outside of the test.
    // Clearing it from the preload manager to destroy it.
    preload_manager()->SetPreloadCandidateSelectorForTesting(nullptr);

    InProcessBrowserTest::TearDown();
  }

  WebUIContentsPreloadManager* preload_manager() {
    return WebUIContentsPreloadManager::GetInstance();
  }

  content::TestNavigationObserver* navigation_waiter() {
    return navigation_waiter_.get();
  }

 protected:
  MockPreloadCandidateSelector* mock_preload_candidate_selector() {
    return preload_candidate_selector_;
  }

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

 private:
  std::unique_ptr<content::TestNavigationObserver> navigation_waiter_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockPreloadCandidateSelector> preload_candidate_selector_;
};

class WebUIContentsPreloadManagerBrowserTest
    : public WebUIContentsPreloadManagerBrowserTestBase,
      public ::testing::WithParamInterface<std::tuple<GURL, PreloadModeName>> {
 public:
  struct PrintParams {
    template <typename ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      // Remove ".top-chrome" from URL. Replace "-." with "_" since gtest does
      // not allow them in test name.
      GURL webui_url = std::get<0>(info.param);
      std::string host = webui_url.host();
      std::string preload_mode = std::get<1>(info.param);
      base::ReplaceSubstringsAfterOffset(&host, 0, ".top-chrome", "");
      base::ReplaceChars(host, "-.", "_", &host);
      base::ReplaceChars(preload_mode, "-.", "_", &preload_mode);
      return host + "_" + preload_mode;
    }
  };

  // WebUIContentsPreloadManagerBrowserTestBase:
  void SetUpFeature() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        features::kPreloadTopChromeWebUI,
        {{features::kPreloadTopChromeWebUIModeName, std::get<1>(GetParam())}});
  }
  void SetUpPreloadURL() override {
    ON_CALL(*mock_preload_candidate_selector(), GetURLToPreload(_))
        .WillByDefault(Return(std::get<0>(GetParam())));
  }
};

// A smoke test that ensures the browser does not crash when triggering
// a preloaded WebUI.
IN_PROC_BROWSER_TEST_P(WebUIContentsPreloadManagerBrowserTest,
                       TriggerPreloadedUI) {
  const GURL webui_url = std::get<0>(GetParam());
  const std::string preload_mode = std::get<1>(GetParam());
  if (preload_mode == features::kPreloadTopChromeWebUIModePreloadOnWarmupName) {
    ASSERT_TRUE(preload_manager()->preloaded_web_contents());
    ASSERT_EQ(preload_manager()->preloaded_web_contents()->GetVisibleURL(),
              webui_url);
  }
  browser()->command_controller()->ExecuteCommand(
      GetCommandIdForURL(webui_url));
  navigation_waiter()->Wait();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebUIContentsPreloadManagerBrowserTest,
    testing::Combine(testing::ValuesIn(GetAllPreloadableWebUIURLs()),
                     testing::ValuesIn(GetAllPreloadManagerModes())),
    WebUIContentsPreloadManagerBrowserTest::PrintParams());

class WebUIContentsPreoloadManagerPageLoadMetricsTest
    : public WebUIContentsPreloadManagerBrowserTestBase {
 public:
  void SetUpFeature() override {
    feature_list()->InitAndEnableFeature(features::kPreloadTopChromeWebUI);
  }
  void SetUpPreloadURL() override {
    ON_CALL(*mock_preload_candidate_selector(), GetURLToPreload(_))
        .WillByDefault(Return(GURL(chrome::kChromeUITabSearchURL)));
  }
};

// TODO(crbug.com/353803591): the page metrics propagation is stopped due
// to first_image_paint being earlier than first_paint.
// Tests that the time from the WebUI request is requested to when First
// Contentful Paint (FCP) is recorded.
IN_PROC_BROWSER_TEST_F(WebUIContentsPreoloadManagerPageLoadMetricsTest,
                       DISABLED_RequestToFCP) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      chrome::kNonTabWebUIRequestToFCPHistogramName, 0);

  preload_manager()->MaybePreloadForBrowserContextForTesting(
      browser()->profile());
  navigation_waiter()->Wait();
  ASSERT_TRUE(preload_manager()->GetPreloadedURLForTesting().has_value());

  // FCP is not recorded because the WebUI is not yet shown.
  histogram_tester.ExpectTotalCount(
      chrome::kNonTabWebUIRequestToFCPHistogramName, 0);

  WebUIContentsPreloadManager::RequestResult request_result =
      preload_manager()->Request(
          *preload_manager()->GetPreloadedURLForTesting(),
          browser()->profile());
  content::WebContents* web_contents = request_result.web_contents.get();
  ASSERT_NE(web_contents, nullptr);

  // Show the WebContents in a WebView.
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.bounds = gfx::Rect(0, 0, 100, 100);
  widget->Init(std::move(params));

  auto webview = std::make_unique<views::WebView>(browser()->profile());
  webview->SetWebContents(web_contents);
  webview->SetPreferredSize(gfx::Size(100, 100));
  widget->GetRootView()->AddChildView(std::move(webview));
  widget->Show();

  WaitForHistogram(chrome::kNonTabWebUIRequestToFCPHistogramName);
  histogram_tester.ExpectTotalCount(
      chrome::kNonTabWebUIRequestToFCPHistogramName, 1);

  widget->CloseNow();
}
