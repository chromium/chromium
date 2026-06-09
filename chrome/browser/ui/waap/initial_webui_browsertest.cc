// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher_impl.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/common/webui_url_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/mapping/metrics_mapping_features.h"
#include "components/metrics/mapping/metrics_name_mapping.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/viz/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "url/gurl.h"

namespace waap {
namespace {

constexpr char kTestMetricName[] =
    "Navigation.DocumentLoader.DidCommitNavigation";
constexpr char kTestMetricNameRenamed[] =
    "Webium.Navigation.DocumentLoader.DidCommitNavigation";

}  // namespace

// Initializes a web ui controller after navigation. Note that this is probably
// an indication that these tests require too much insight into the state of
// inner implementation details, or there is some deficiency in the testing
// framework. Perhaps these tests should be promoted to ui tests.
class WebUIControllerInitalizer : protected content::WebContentsObserver {
 public:
  ~WebUIControllerInitalizer() override = default;

  virtual void Init(content::WebUIController* web_ui_controller) = 0;
  void Watch(content::WebContents* web_contents) {
    content::WebContentsObserver::Observe(web_contents);
  }

 protected:
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    auto* controller = handle->GetWebContents()->GetWebUI()->GetController();
    Init(controller);
    content::WebContentsObserver::Observe(nullptr);
  }
};

// We should probably just hoist the concrete DependencyProvider out of the
// webview class so that it's portable enough for use in test.
class ToolbarDependencyProvider : public WebUIToolbarUI::DependencyProvider {
 public:
  explicit ToolbarDependencyProvider(Browser* browser) : browser_(browser) {}

  ~ToolbarDependencyProvider() = default;

  // This might blow up in the future. We are implicitly assuming that the
  // delegate isn't going to be used in this test.
  browser_controls_api::BrowserControlsService::BrowserControlsServiceDelegate*
  GetBrowserControlsDelegate() override {
    return nullptr;
  }

  toolbar_ui_api::ToolbarUIService::ToolbarUIServiceDelegate*
  GetToolbarUIServiceDelegate() override {
    return nullptr;
  }

  std::unique_ptr<toolbar_ui_api::NavigationControlsStateFetcher>
  GetNavigationControlsStateFetcher() override {
    return std::make_unique<toolbar_ui_api::NavigationControlsStateFetcherImpl>(
        base::BindLambdaForTesting(
            []() { return CreateValidNavigationControlsState(); }));
  }

  std::unique_ptr<toolbar_ui_api::IconTableFetcher> GetIconTableFetcher()
      override {
    return std::make_unique<FakeIconTableFetcher>();
  }

  CommandUpdater* GetCommandUpdater() override {
    return reinterpret_cast<CommandUpdater*>(
        browser_->GetFeatures().browser_command_controller());
  }

 private:
  raw_ptr<BrowserWindowInterface> browser_;
};

class WebUIToolbarInitializer : public WebUIControllerInitalizer {
 public:
  explicit WebUIToolbarInitializer(Browser* browser) : injector_(browser) {}

  ~WebUIToolbarInitializer() override = default;

  void Init(content::WebUIController* controller) override {
    auto* toolbar_controller = controller->GetAs<WebUIToolbarUI>();
    toolbar_controller->Init(&injector_);
  }

 private:
  ToolbarDependencyProvider injector_;
};

class InitialWebUIBrowserTestBase : public InProcessBrowserTest {
 public:
  InitialWebUIBrowserTestBase()
      : InitialWebUIBrowserTestBase(
            std::vector<base::test::FeatureRefAndParams>{}) {}

  explicit InitialWebUIBrowserTestBase(
      std::vector<base::test::FeatureRefAndParams> additional_features) {
    std::vector<base::test::FeatureRefAndParams> base_features = {
        {features::kInitialWebUI, {{"use_separate_process", "true"}}},
        {features::kWebUIReloadButton, {}},
        {features::kSkipIPCChannelPausingForNonGuests, {}},
        {features::kWebUIInProcessResourceLoadingV2, {}},
        {features::kInitialWebUISyncNavStartToCommit, {}}};

    std::vector<base::test::FeatureRefAndParams> features;
    features.reserve(base_features.size() + additional_features.size());

    for (const auto& base_f : base_features) {
      bool overridden = false;
      for (const auto& add_f : additional_features) {
        if (&add_f.feature.get() == &base_f.feature.get()) {
          overridden = true;
          break;
        }
      }
      if (!overridden) {
        features.push_back(base_f);
      }
    }

    for (const auto& add_f : additional_features) {
      features.push_back(add_f);
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(features, {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  protected:
  ukm::TestAutoSetUkmRecorder& ukm_recorder() { return *ukm_recorder_; }

  std::unique_ptr<content::WebContents> CreateAndNavigateWebContents(
      const GURL& url,
      WebUIControllerInitalizer* initializer) {
    // Create a new WebContents, since initial WebUI navigations are only
    // allowed to happen as the first navigation in a new WebContents.
    content::BrowserContext* browser_context = browser()
                                                   ->tab_strip_model()
                                                   ->GetActiveWebContents()
                                                   ->GetBrowserContext();
    content::WebContents::CreateParams new_contents_params(
        browser_context,
        content::SiteInstance::CreateForURL(browser_context, url));
    std::unique_ptr<content::WebContents> new_web_contents(
        content::WebContents::Create(new_contents_params));
    if (initializer) {
      initializer->Watch(new_web_contents.get());
    }
    webui::SetBrowserWindowInterface(new_web_contents.get(), browser());
    // UKM PageLoad metrics for Initial WebUI are recorded by
    // `UkmPageLoadMetricsObserver`, which is attached to the WebContents via
    // `MetricsWebContentsObserver`. For "bare" WebContents created manually in
    // tests, we must explicitly call this to ensure the observer is attached.
    InitializePageLoadMetricsForWebContents(new_web_contents.get());

    // Navigate to `url`.
    content::NavigationController& controller =
        new_web_contents->GetController();
    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.WatchExistingWebContents();
    auto handle = controller.LoadURLWithParams(
        content::NavigationController::LoadURLParams(url));

    navigation_observer.Wait();

    // Ensure the navigation successfully commits.
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(navigation_observer.last_navigation_url(), url);

    return new_web_contents;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

class InitialWebUINavigationBrowserTest : public InitialWebUIBrowserTestBase {};

// TopChrome WebUI flag should be correctly set even if a non-TopChrome WebUI is
// created first.
IN_PROC_BROWSER_TEST_F(InitialWebUINavigationBrowserTest,
                       Flag_NonTopChromeCommittedFirst) {
  // 1) Navigate to non-TopChrome WebUI in a new WebContents.
  GURL url(chrome::kChromeUIVersionURL);
  EXPECT_FALSE(IsTopChromeWebUIURL(url));
  EXPECT_FALSE(IsForInitialWebUI(url));
  std::unique_ptr<content::WebContents> non_initial_webui_web_contents =
      CreateAndNavigateWebContents(url, nullptr);
  // Ensure that the process doesn't have the TopChrome WebUI flag set.
  EXPECT_FALSE(non_initial_webui_web_contents->GetPrimaryMainFrame()
                   ->GetProcess()
                   ->IsForTopChromeWebUI());

  // 2) Navigate to initial WebUI in a new WebContents.
  GURL url2(chrome::kChromeUIWebUIToolbarURL);
  EXPECT_TRUE(IsTopChromeWebUIURL(url2));
  EXPECT_TRUE(IsForInitialWebUI(url2));
  WebUIToolbarInitializer initializer(browser());

  std::unique_ptr<content::WebContents> initial_webui_web_contents =
      CreateAndNavigateWebContents(url2, &initializer);
  // Ensure that the process has the TopChrome WebUI flag set.
  EXPECT_TRUE(initial_webui_web_contents->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsForTopChromeWebUI());
}

// Initial WebUI process should not be shared with non-Initial WebUI per
// https://crrev.com/c/7508984.
// However, both should have the TopChrome WebUI flag set.
IN_PROC_BROWSER_TEST_F(InitialWebUINavigationBrowserTest,
                       InitialWebUIProcessSharing) {
  // 1) Navigate to initial WebUI in a new WebContents.
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  EXPECT_TRUE(IsTopChromeWebUIURL(url));
  WebUIToolbarInitializer initializer(browser());
  std::unique_ptr<content::WebContents> initial_webui_web_contents =
      CreateAndNavigateWebContents(url, &initializer);

  // Ensure that the process has the TopChrome WebUI flag set.
  EXPECT_TRUE(initial_webui_web_contents->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsForTopChromeWebUI());

  // 2) Navigate to another TopChrome WebUI (but not Initial WebUI) in a new
  // WebContents.
  GURL url2(chrome::kChromeUITabSearchURL);
  EXPECT_TRUE(IsTopChromeWebUIURL(url2));
  std::unique_ptr<content::WebContents> non_initial_webui_web_contents =
      CreateAndNavigateWebContents(url2, nullptr);

  // It also has the TopChrome WebUI flag set.
  EXPECT_TRUE(non_initial_webui_web_contents->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsForTopChromeWebUI());
  // Initial WebUI and other TopChrome WebUI should use different processes
  // because Initial WebUI is explicitly isolated.
  EXPECT_NE(
      initial_webui_web_contents->GetPrimaryMainFrame()->GetProcess(),
      non_initial_webui_web_contents->GetPrimaryMainFrame()->GetProcess());

  // 3) Navigate to initial WebUI again in a new WebContents.
  std::unique_ptr<content::WebContents> initial_webui_web_contents2 =
      CreateAndNavigateWebContents(url, &initializer);

  // Initial WebUI should share process with the other Initial WebUI
  // WebContents, but not the non-Initial TopChrome WebUI one.
  EXPECT_EQ(initial_webui_web_contents->GetPrimaryMainFrame()->GetProcess(),
            initial_webui_web_contents2->GetPrimaryMainFrame()->GetProcess());
  EXPECT_NE(
      initial_webui_web_contents2->GetPrimaryMainFrame()->GetProcess(),
      non_initial_webui_web_contents->GetPrimaryMainFrame()->GetProcess());

  // 4) Navigate to another non-Initial TopChrome WebUI in a new WebContents.
  GURL url3(chrome::kChromeUIReadLaterURL);
  EXPECT_TRUE(IsTopChromeWebUIURL(url3));
  std::unique_ptr<content::WebContents> non_initial_webui_web_contents2 =
      CreateAndNavigateWebContents(url3, nullptr);

  // Non-Initial TopChrome WebUIs share a process with EACH OTHER,
  // but not with the Initial WebUI.
  EXPECT_EQ(
      non_initial_webui_web_contents->GetPrimaryMainFrame()->GetProcess(),
      non_initial_webui_web_contents2->GetPrimaryMainFrame()->GetProcess());
  EXPECT_NE(
      non_initial_webui_web_contents2->GetPrimaryMainFrame()->GetProcess(),
      initial_webui_web_contents->GetPrimaryMainFrame()->GetProcess());
}

// Verifies that UKM PageLoad metrics are correctly recorded for an Initial
// WebUI navigation. This test checks that:
// 1. The UKM recorder is active and receives entries.
// 2. `MetricsWebContentsObserver` is correctly attached to the WebContents.
// 3. The UKM flush (triggered by WebContents destruction) correctly records
//    the buffered PageLoad metrics.
IN_PROC_BROWSER_TEST_F(InitialWebUINavigationBrowserTest, RecordPageLoadUKM) {
  using PageLoad = ukm::builders::PageLoad;

  // 1) Navigate to initial WebUI.
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  WebUIToolbarInitializer initializer(browser());
  std::unique_ptr<content::WebContents> initial_webui_web_contents =
      CreateAndNavigateWebContents(url, &initializer);

  // 2) Set up a RunLoop to wait for the UKM entry.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(PageLoad::kEntryName,
                                       ukm_loop.QuitClosure());

  // 3) Close the WebContents to trigger the UKM flush.
  initial_webui_web_contents.reset();

  // 4) Wait for the UKM entry to be recorded.
  if (ukm_recorder().GetEntriesByName(PageLoad::kEntryName).empty()) {
    ukm_loop.Run();
  }

  // 5) Verify UKM recording.
  auto entries = ukm_recorder().GetEntriesByName(PageLoad::kEntryName);
  EXPECT_FALSE(entries.empty());
}

// Verifies that when a new browser window is created while another window
// already exists, the `FirstPaintGap metrics with the "WithExistingWindow"
// dimension are recorded.
IN_PROC_BROWSER_TEST_F(InitialWebUINavigationBrowserTest,
                       RecordsFirstPaintGapDeltaWithExistingWindow) {
  base::HistogramTester histogram_tester;

  // The test harness automatically creates a default browser window on startup.
  // Wait for the new window metric to be recorded.
  // The metric expects "WithExistingWindow" when the new browser has prior
  // windows. Evaluate the actual state to avoid hardcoding test assumptions on
  // platforms where the default browser might not match desktop norms
  // perfectly.
  const std::string expected_metric = base::StrCat(
      {"InitialWebUI.NewWindow.AllSources.",
       ProfileBrowserCollection::GetForProfile(browser()->profile())
                   ->GetSize() > 0
           ? "WithExistingWindow"
           : "WithoutExistingWindow",
       ".BrowserWindowToReloadButton.FirstPaintGap"});
  base::StatisticsRecorder::HistogramWaiter waiter(expected_metric);

  // Create a new browser window without actively showing/painting it yet.
  Browser::CreateParams params(browser()->profile(), true);
  Browser* new_browser = Browser::Create(params);

  if (auto* manager = InitialWebUIWindowMetricsManager::From(new_browser)) {
    manager->SkipStartupForTesting();
    manager->SetWindowCreationInfo(
        waap::NewWindowCreationSource::kBrowserInitiated,
        base::TimeTicks::Now());

    base::TimeTicks t1 = base::TimeTicks::Now();
    manager->OnBrowserWindowFirstPresentation(t1);
    manager->OnReloadButtonFirstPaint(t1 + base::Milliseconds(50));
  }

  AddBlankTabAndShow(new_browser);
  waiter.Wait();

  histogram_tester.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithExistingWindow."
      "BrowserWindowToReloadButton.FirstPaintGap",
      1);
  histogram_tester.ExpectTotalCount(
      "InitialWebUI.NewWindow.BrowserInitiated.WithExistingWindow."
      "BrowserWindowToReloadButton.FirstPaintGap",
      1);
}

// TODO(crbug.com/490810407): Verify that `NavigationTimeline` UKM is recorded.
// `NavigationRequest::GetNavigationTimelineUkmBuilder()` uses a low sampling
// rate such that it is unlikely to be recorded during a test. Adding browser
// test coverage requires a test configuration with a 100% sampling rate.

class InitialWebUIMetricsMappingBrowserTest
    : public InitialWebUIBrowserTestBase {
 public:
  InitialWebUIMetricsMappingBrowserTest() {
    metrics::MetricsNameMappingConfiguration config;
    metrics::MetricsNameMapping* mapping = config.add_rules();
    mapping->set_metric_name(kTestMetricName);
    mapping->set_new_metric_name(kTestMetricNameRenamed);

    std::string serialized_config;
    config.SerializeToString(&serialized_config);
    std::string base64_config = base::Base64Encode(serialized_config);

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{metrics::features::kWebiumMetricsMapping,
          {{metrics::features::kWebiumMetricsMappingConfig.name,
            base64_config}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InitialWebUIMetricsMappingBrowserTest,
                       WebiumRendererMetricsAreMapped) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial WebUI page.
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  WebUIToolbarInitializer initializer(browser());
  std::unique_ptr<content::WebContents> initial_webui_web_contents =
      CreateAndNavigateWebContents(url, &initializer);

  // Fetch the histograms from the Webium renderer and merge them into the
  // browser's StatisticsRecorder.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // The Navigation.DocumentLoader.DidCommitNavigation metric is emitted
  // unconditionally by the renderer upon navigation commit.
  // The base metric should have been renamed by the synchronizer.
  // We check that the Webium-prefixed metric successfully captured
  // the sample by checking the count.
  int total_webium_count = 0;
  for (const base::Bucket& bucket :
       histogram_tester.GetAllSamples(kTestMetricNameRenamed)) {
    total_webium_count += bucket.count;
  }
  EXPECT_GE(total_webium_count, 1);
}

// TODO(crbug.com/491012584): Flaky on ChromeOS MSan.
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_NormalRendererMetricsAreNotMapped \
  DISABLED_NormalRendererMetricsAreNotMapped
#else
#define MAYBE_NormalRendererMetricsAreNotMapped \
  NormalRendererMetricsAreNotMapped
#endif
IN_PROC_BROWSER_TEST_F(InitialWebUIMetricsMappingBrowserTest,
                       MAYBE_NormalRendererMetricsAreNotMapped) {
  base::HistogramTester histogram_tester;

  // Navigate to a regular WebUI page (NOT TopChrome WebUI).
  GURL url(chrome::kChromeUIVersionURL);
  std::unique_ptr<content::WebContents> non_initial_webui_web_contents =
      CreateAndNavigateWebContents(url, nullptr);

  // Fetch histograms and merge them.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Because this is NOT a Webium renderer, the metric should NOT be prefixed.
  histogram_tester.ExpectTotalCount(kTestMetricNameRenamed, 0);

  int total_normal_count = 0;
  for (const base::Bucket& bucket :
       histogram_tester.GetAllSamples(kTestMetricName)) {
    total_normal_count += bucket.count;
  }
  EXPECT_GE(total_normal_count, 1);
}

class InitialWebUIMetricsAllowlistBrowserTest
    : public InitialWebUIBrowserTestBase {
 public:
  InitialWebUIMetricsAllowlistBrowserTest() {
    metrics::MetricsNameMappingConfiguration config;
    metrics::MetricsNameMapping* mapping = config.add_rules();
    mapping->set_metric_name(kTestMetricName);
    // No new_metric_name set, so it should be allowed without renaming.

    std::string serialized_config;
    config.SerializeToString(&serialized_config);
    std::string base64_config = base::Base64Encode(serialized_config);

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{metrics::features::kWebiumMetricsMapping,
          {{metrics::features::kWebiumMetricsMappingConfig.name,
            base64_config}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InitialWebUIMetricsAllowlistBrowserTest,
                       WebiumRendererMetricsAllowedWithoutRenaming) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial WebUI page.
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  WebUIToolbarInitializer initializer(browser());
  std::unique_ptr<content::WebContents> initial_webui_web_contents =
      CreateAndNavigateWebContents(url, &initializer);

  // Fetch the histograms from the Webium renderer and merge them into the
  // browser's StatisticsRecorder.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // The Navigation.DocumentLoader.DidCommitNavigation metric is emitted
  // unconditionally by the renderer upon navigation commit.
  // The configuration allows this metric but does NOT provide a rename.
  // We check that the original name is used and NO prefixed version exists.

  // 1. Verify prefixed metric does NOT exist.
  histogram_tester.ExpectTotalCount(kTestMetricNameRenamed, 0);

  // 2. Verify original metric DOES exist.
  int total_count = 0;
  for (const base::Bucket& bucket :
       histogram_tester.GetAllSamples(kTestMetricName)) {
    total_count += bucket.count;
  }
  EXPECT_GE(total_count, 1);
}

class InitialWebUIMetricsDropBrowserTest : public InitialWebUIBrowserTestBase {
 public:
  InitialWebUIMetricsDropBrowserTest() {
    // Enable the feature with an empty configuration.
    // This implies that NO metrics are mapped or allowed, so all Webium
    // renderer metrics should be dropped.
    metrics::MetricsNameMappingConfiguration config;
    std::string serialized_config;
    config.SerializeToString(&serialized_config);
    std::string base64_config = base::Base64Encode(serialized_config);

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{metrics::features::kWebiumMetricsMapping,
          {{metrics::features::kWebiumMetricsMappingConfig.name,
            base64_config}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/491012584): Flaky on ChromeOS MSan and Linux MSan.
#if (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)) && defined(MEMORY_SANITIZER)
#define MAYBE_WebiumRendererMetricsDroppedIfNoRule \
  DISABLED_WebiumRendererMetricsDroppedIfNoRule
#else
#define MAYBE_WebiumRendererMetricsDroppedIfNoRule \
  WebiumRendererMetricsDroppedIfNoRule
#endif
IN_PROC_BROWSER_TEST_F(InitialWebUIMetricsDropBrowserTest,
                       MAYBE_WebiumRendererMetricsDroppedIfNoRule) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial WebUI page.
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  WebUIToolbarInitializer initializer(browser());
  std::unique_ptr<content::WebContents> initial_webui_web_contents =
      CreateAndNavigateWebContents(url, &initializer);

  // Fetch the histograms from the Webium renderer and merge them into the
  // browser's StatisticsRecorder.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // The Navigation.DocumentLoader.DidCommitNavigation metric is emitted
  // unconditionally by the renderer upon navigation commit.
  // The configuration is empty, so this metric should be DROPPED.

  // 1. Verify prefixed metric does NOT exist.
  histogram_tester.ExpectTotalCount(kTestMetricNameRenamed, 0);

  // 2. Verify original metric does NOT exist (it was dropped, not emitted
  // as-is).
  histogram_tester.ExpectTotalCount(kTestMetricName, 0);
}

// TODO(crbug.com/493786816): Add test case for startup metrics verification
// once flaky behaviors on ChromeOS are fixed.
// Note: On ChromeOS when run with `WebUIReloadButtonDeferBrowserViewShow=true`,
// the `UserSessionManager::LaunchBrowser()` function is responsible for
// triggering startup metrics profiling by calling
// `metrics::BeginFirstWebContentsProfiling()`. However, it contains an explicit
// guard that only triggers profiling if the browser window is "active"
// according to the window manager:
// ```cpp
//   aura::Window* active_window = ash::window_util::GetActiveWindow();
//   bool is_browser_window_active =
//       active_window && active_window->GetProperty(chromeos::kAppTypeKey) ==
//                            chromeos::AppType::BROWSER;
//   if (is_browser_window_active) {
//     metrics::BeginFirstWebContentsProfiling();
//   }
// ```
// When the deferral feature is enabled, `is_browser_window_active` evaluates to
// false at this initial trigger point. Hence,
// `metrics::BeginFirstWebContentsProfiling()` is skipped, and the startup
// metrics, such as `Startup.FirstWebContents.NonEmptyPaint3`, are never
// recorded.

class InitialWebUISurfaceSyncBrowserTest : public InitialWebUIBrowserTestBase {
 public:
  InitialWebUISurfaceSyncBrowserTest()
      : InitialWebUIBrowserTestBase(
            {{blink::features::kInitialWebUISurfaceSync, {}},
             {features::kBypassOutdatedSurfaceActivation, {}},
             {features::kWebUIReloadButton,
              {{"WebUIReloadButtonDeferBrowserViewShow", "false"}}}}) {}
};

IN_PROC_BROWSER_TEST_F(InitialWebUISurfaceSyncBrowserTest,
                       FirstPaintGapIsZero) {
  base::HistogramTester histogram_tester;

  // We need to wait for the histogram.
  const std::string expected_metric =
      "InitialWebUI.NewWindow.AllSources.WithExistingWindow."
      "BrowserWindowToReloadButton.FirstPaintGap";

  base::StatisticsRecorder::HistogramWaiter waiter(expected_metric);

  // Create a new window.
  Browser::CreateParams params(browser()->profile(), true);
  Browser* new_browser = Browser::Create(params);

  if (auto* manager = InitialWebUIWindowMetricsManager::From(new_browser)) {
    manager->SkipStartupForTesting();
    manager->SetWindowCreationInfo(
        waap::NewWindowCreationSource::kBrowserInitiated,
        base::TimeTicks::Now());
  }

  AddBlankTabAndShow(new_browser);
  waiter.Wait();

  // Assert that the FirstPaintGap is 0.
  histogram_tester.ExpectUniqueSample(expected_metric, 0, 1);
}

// TODO(crbug.com/507317176): The following two tests now only work on Windows
// when dealing with the minimized window. Since we have manually tested the
// behavior on Linux and macOS, we will only enable them on Windows and maybe
// fix it later.

#if BUILDFLAG(IS_WIN)

// Tests that the duration metrics are not recorded for windows created as
// minimized.
IN_PROC_BROWSER_TEST_F(InitialWebUINavigationBrowserTest,
                       InitiallyMinimizedWindowSkipsMetrics) {
  base::HistogramTester histogram_tester;

  // Create a minimized browser window.
  Browser::CreateParams params(browser()->profile(), true);
  params.initial_show_state = ui::mojom::WindowShowState::kMinimized;
  Browser* new_browser = Browser::Create(params);

  if (auto* manager = InitialWebUIWindowMetricsManager::From(new_browser)) {
    manager->SkipStartupForTesting();
    manager->SetWindowCreationInfo(
        waap::NewWindowCreationSource::kBrowserInitiated,
        base::TimeTicks::Now());
  }

  // Show the window which should be shown minimized, and verify it.
  new_browser->GetWindow()->Show();
  EXPECT_TRUE(new_browser->GetWindow()->IsMinimized());

  // Restore (open) the window.
  new_browser->GetWindow()->Restore();
  EXPECT_FALSE(new_browser->GetWindow()->IsMinimized());

  // Simulate presentation and paint events (which now happen after the window
  // is opened).
  if (auto* manager = InitialWebUIWindowMetricsManager::From(new_browser)) {
    base::TimeTicks t1 = base::TimeTicks::Now();
    manager->OnBrowserWindowFirstPresentation(t1);
    manager->OnReloadButtonFirstPaint(t1 + base::Milliseconds(50));
  }

  // Verify ShowRequestedToFirstPaint was not recorded.
  histogram_tester.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow.BrowserWindow."
      "ShowRequestedToFirstPaint.FromConstructor",
      0);

  // Verify FirstPaintGap was not recorded.
  histogram_tester.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "BrowserWindowToReloadButton.FirstPaintGap",
      0);
}

// Tests that the duration metrics should be skipped for the windows that are
// restored as minimized.
IN_PROC_BROWSER_TEST_F(InitialWebUINavigationBrowserTest,
                       SessionRestoreMinimizedWindow) {
  Profile* profile = browser()->profile();

  // Enable session restore and minimize the current window.
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile, pref);
  browser()->GetWindow()->Minimize();
  EXPECT_TRUE(browser()->GetWindow()->IsMinimized());

  // Keep the profile and process alive when we close the window.
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // Close the browser and reset the static state of the metrics manager so the
  // next window restored is treated as startup, which allows us to test the
  // startup metric paths.
  CloseBrowserSynchronously(browser());
  InitialWebUIWindowMetricsManager::ResetForTesting();

  // Create a new window, which should trigger session restore.
  base::HistogramTester histogram_tester;
  ui_test_utils::BrowserCreatedObserver browser_created_observer;

  chrome::NewEmptyWindow(profile);

  Browser* restored_browser = browser_created_observer.Wait();
  ASSERT_TRUE(restored_browser);

  // Verify the restored window is minimized.
  EXPECT_TRUE(restored_browser->GetWindow()->IsMinimized());

  // Restore (open) the window.
  restored_browser->GetWindow()->Restore();
  EXPECT_FALSE(restored_browser->GetWindow()->IsMinimized());

  // Simulate paint events (which now happen after the window is opened).
  if (auto* manager =
          InitialWebUIWindowMetricsManager::From(restored_browser)) {
    base::TimeTicks t1 = base::TimeTicks::Now();
    manager->OnBrowserWindowFirstPresentation(t1);
    manager->OnReloadButtonFirstPaint(t1 + base::Milliseconds(50));
  }

  // Verify no metrics were recorded, since it is treated as startup as we
  // reset for testing, we should check startup metrics.
  histogram_tester.ExpectTotalCount(
      "InitialWebUI.Startup.SessionRestore.BrowserWindow."
      "ShowRequestedToFirstPaint",
      0);

  histogram_tester.ExpectTotalCount(
      "InitialWebUI.Startup.SessionRestore.BrowserWindowToReloadButton."
      "FirstPaintGap",
      0);

  keep_alive.reset();
  profile_keep_alive.reset();
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace waap
