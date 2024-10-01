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
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager_test_api.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/url_constants.h"

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

// Returns the command ID that can be used to trigger the WebUI.
int GetCommandIdForURL(GURL webui_url) {
  TopChromeWebUIConfig* config = TopChromeWebUIConfig::From(nullptr, webui_url);
  CHECK(config);
  CHECK(config->GetCommandIdForTesting().has_value())
      << "A preloadable WebUI must override "
         "TopChromeWebUIConfig::GetCommandIdForTesting()";
  return *config->GetCommandIdForTesting();
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
    test_api().SetPreloadCandidateSelector(
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
    test_api().SetPreloadCandidateSelector(nullptr);

    InProcessBrowserTest::TearDown();
  }

  WebUIContentsPreloadManager* preload_manager() {
    return WebUIContentsPreloadManager::GetInstance();
  }

  WebUIContentsPreloadManagerTestAPI& test_api() { return test_api_; }

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
  WebUIContentsPreloadManagerTestAPI test_api_;
  raw_ptr<MockPreloadCandidateSelector> preload_candidate_selector_;
};

class WebUIContentsPreloadManagerBrowserSmokeTest
    : public WebUIContentsPreloadManagerBrowserTestBase,
      public ::testing::WithParamInterface<PreloadModeName> {
 public:
  struct PrintParam {
    template <typename ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      // Preload mode names contain "-" which is disallowed in test names.
      // Replace it with "_";
      std::string preload_mode = info.param;
      base::ReplaceChars(preload_mode, "-", "_", &preload_mode);
      return preload_mode;
    }
  };

  // WebUIContentsPreloadManagerBrowserTestBase:
  void SetUpFeature() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        features::kPreloadTopChromeWebUI,
        {{features::kPreloadTopChromeWebUIModeName, GetParam()},
         {features::kPreloadTopChromeWebUISmartPreloadName, "true"}});
  }
  void SetUpPreloadURL() override {
    // Don't preload for the default browser. The smoke test will
    // test each WebUI in a new browser.
    ON_CALL(*mock_preload_candidate_selector(), GetURLToPreload(_))
        .WillByDefault(Return(std::nullopt));
  }
};

// A smoke test that ensures the browser does not crash when triggering
// preloaded WebUIs. Each WebUI is triggered in a new browser.
// Note: the list of preloadable WebUIs is not available until their WebUIConfig
// is registered during browser startup, therefore cannot be used as test
// parameters.
IN_PROC_BROWSER_TEST_P(WebUIContentsPreloadManagerBrowserSmokeTest,
                       TriggerPreloadedUIs) {
  const std::string preload_mode = GetParam();
  for (const GURL& webui_url : test_api().GetAllPreloadableWebUIURLs()) {
    // Set the next preload WebUI URL.
    ON_CALL(*mock_preload_candidate_selector(), GetURLToPreload(_))
        .WillByDefault(Return(webui_url));

    // Create a new browser.
    Browser* new_browser = CreateBrowser(browser()->profile());

    // Check if the expected WebUI is preloaded.
    if (preload_mode ==
        features::kPreloadTopChromeWebUIModePreloadOnWarmupName) {
      ASSERT_TRUE(preload_manager()->preloaded_web_contents());
      ASSERT_EQ(preload_manager()->preloaded_web_contents()->GetVisibleURL(),
                webui_url);
    }

    // Trigger the WebUI.
    new_browser->command_controller()->ExecuteCommand(
        GetCommandIdForURL(webui_url));
    navigation_waiter()->Wait();

    // Clean up.
    CloseBrowserSynchronously(new_browser);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebUIContentsPreloadManagerBrowserSmokeTest,
    testing::ValuesIn(GetAllPreloadManagerModes()),
    WebUIContentsPreloadManagerBrowserSmokeTest::PrintParam());

namespace {

constexpr char kTestWebUIHost[] = "test-host";
constexpr char kTestWebUIOrigin[] = "chrome://test-host";
// This URL will be served with the content of //chrome/test/data/title1.html
constexpr char kTestWebUIURL[] = "chrome://test-host/title1.html";

class TestTopChromeWebUIController : public TopChromeWebUIController {
 public:
  explicit TestTopChromeWebUIController(content::WebUI* web_ui)
      : TopChromeWebUIController(web_ui) {}
  static std::string GetWebUIName() { return "Test"; }
};

class TestTopChromeWebUIConfig
    : public DefaultTopChromeWebUIConfig<TestTopChromeWebUIController> {
 public:
  explicit TestTopChromeWebUIConfig(std::string_view host)
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme, host) {}

  // DefaultTopChromeWebUIConfig:
  bool IsPreloadable() override { return true; }
};

}  // namespace

class WebUIContentsPreloadManagerPageLoadMetricsTest
    : public WebUIContentsPreloadManagerBrowserTestBase {
 public:
  void SetUpFeature() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        features::kPreloadTopChromeWebUI,
        {{features::kPreloadTopChromeWebUISmartPreloadName, "true"}});
  }
  void SetUpPreloadURL() override {
    // Intentionally use a simple HTML as test WebUI. Testing more complex WebUI
    // (e.g. Tab Search) sparadically times out on waiting for FCP because the
    // first image paint event arrives unexpectedly earlier than the first
    // paint. See crbug.com/353803591#comment4
    config_registration_ =
        std::make_unique<content::ScopedWebUIConfigRegistration>(
            std::make_unique<TestTopChromeWebUIConfig>(kTestWebUIHost));
    ON_CALL(*mock_preload_candidate_selector(), GetURLToPreload(_))
        .WillByDefault(Return(GURL(kTestWebUIURL)));
  }

 private:
  std::unique_ptr<content::ScopedWebUIConfigRegistration> config_registration_;
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// The page load metrics test is flaky on LaCrOS because sometimes viz::Display
// reports a negative frame latency that causes page load metrics to stop
// propagation.
#define MAYBE_RequestToFCPAndLCP DISABLED_RequestToFCPAndLCP
#else
#define MAYBE_RequestToFCPAndLCP RequestToFCPAndLCP
#endif
// Tests that the time from the WebUI is requested to when First Contentful
// Paint (FCP) is recorded.
IN_PROC_BROWSER_TEST_F(WebUIContentsPreloadManagerPageLoadMetricsTest,
                       MAYBE_RequestToFCPAndLCP) {
  // Serves the test origin with files from the test data folder.
  auto url_loader_interceptor =
      content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
          GetChromeTestDataDir().MaybeAsASCII(), GURL(kTestWebUIOrigin));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kNonTabWebUIRequestToFCPHistogramName, 0);
  histogram_tester.ExpectTotalCount(kNonTabWebUIRequestToLCPHistogramName, 0);

  test_api().MaybePreloadForBrowserContext(browser()->profile());
  navigation_waiter()->Wait();
  ASSERT_TRUE(test_api().GetPreloadedURL().has_value());

  // FCP and LCP are not recorded because the WebUI is not yet shown.
  histogram_tester.ExpectTotalCount(kNonTabWebUIRequestToFCPHistogramName, 0);
  histogram_tester.ExpectTotalCount(kNonTabWebUIRequestToLCPHistogramName, 0);

  WebUIContentsPreloadManager::RequestResult request_result =
      preload_manager()->Request(*test_api().GetPreloadedURL(),
                                 browser()->profile());
  content::WebContents* web_contents = request_result.web_contents.get();
  ASSERT_NE(web_contents, nullptr);

  // Show the WebContents in a WebView. The FCP is recorded on viz frame commit,
  // so we must attach the WebUI to a rendering surface (e.g. WebView in a
  // Widget).
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

  WaitForHistogram(kNonTabWebUIRequestToFCPHistogramName);
  histogram_tester.ExpectTotalCount(kNonTabWebUIRequestToFCPHistogramName, 1);
  // LCP is not recorded until WebContents close or navigation.
  histogram_tester.ExpectTotalCount(kNonTabWebUIRequestToLCPHistogramName, 0);

  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(url::kAboutBlankURL)));
  WaitForHistogram(kNonTabWebUIRequestToLCPHistogramName);
  histogram_tester.ExpectTotalCount(kNonTabWebUIRequestToLCPHistogramName, 1);

  widget->CloseNow();
}

class WebUIContentsPreloadManagerHistoryClusterMetricTest
    : public WebUIContentsPreloadManagerBrowserTestBase {
 public:
  void SetUpFeature() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        features::kPreloadTopChromeWebUI,
        {{features::kPreloadTopChromeWebUISmartPreloadName, "true"}});
  }
  void SetUpPreloadURL() override {
    ON_CALL(*mock_preload_candidate_selector(), GetURLToPreload(_))
        .WillByDefault(
            Return(GURL(chrome::kChromeUIHistoryClustersSidePanelURL)));
  }
};

// Tests that history cluster metrics are NOT recorded for a preloaded History
// Clusters UI that is never shown.
IN_PROC_BROWSER_TEST_F(WebUIContentsPreloadManagerHistoryClusterMetricTest,
                       PreloadButNeverShow) {
  base::HistogramTester histogram_tester;
  test_api().MaybePreloadForBrowserContext(browser()->profile());
  navigation_waiter()->Wait();
  ASSERT_EQ(test_api().GetPreloadedURL(),
            GURL(chrome::kChromeUIHistoryClustersSidePanelURL));

  // History Cluster metrics, if any, are recorded on WebUI destruction.
  test_api().SetPreloadedContents(nullptr);
  // The metrics are not recorded because the WebUI is never shown.
  histogram_tester.ExpectTotalCount("History.Clusters.Actions.InitialState", 0);
}

// Tests that history cluster metrics are recorded for a preloaded History
// Clusters UI that is shown.
IN_PROC_BROWSER_TEST_F(WebUIContentsPreloadManagerHistoryClusterMetricTest,
                       PreloadAndShow) {
  base::HistogramTester histogram_tester;
  test_api().MaybePreloadForBrowserContext(browser()->profile());
  navigation_waiter()->Wait();
  ASSERT_EQ(test_api().GetPreloadedURL(),
            GURL(chrome::kChromeUIHistoryClustersSidePanelURL));

  std::unique_ptr<content::WebContents> web_contents = std::move(
      preload_manager()
          ->Request(GURL(chrome::kChromeUIHistoryClustersSidePanelURL),
                    browser()->profile())
          .web_contents);
  web_contents->UpdateWebContentsVisibility(content::Visibility::VISIBLE);
  // History Cluster metrics are recorded on WebUI destruction.
  web_contents.reset();
  histogram_tester.ExpectTotalCount("History.Clusters.Actions.InitialState", 1);
}
