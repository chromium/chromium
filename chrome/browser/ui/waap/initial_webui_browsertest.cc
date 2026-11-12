// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/tabs/windows_api.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_webui_metrics_dispatcher.h"
#include "chrome/browser/ui/waap/initial_webui_profile_service.h"
#include "chrome/browser/ui/waap/initial_webui_profile_service_factory.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/browser/ui/waap/window_creation.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_prerenderer.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/toolbar_state_fetcher_impl.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/common/webui_url_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/mapping/metrics_mapping_features.h"
#include "components/metrics/mapping/metrics_name_mapping.pb.h"
#include "components/prefs/pref_service.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/ukm/gmock_matchers.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/viz/common/features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension_builder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/color/color_provider_key.h"
#include "url/gurl.h"
#include "url/url_constants.h"

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
  explicit ToolbarDependencyProvider(BrowserWindowInterface* browser)
      : browser_(browser) {}

  ~ToolbarDependencyProvider() override = default;

  base::WeakPtr<DependencyProvider> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

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

  std::unique_ptr<toolbar_ui_api::ToolbarStateFetcher> GetToolbarStateFetcher()
      override {
    return std::make_unique<toolbar_ui_api::ToolbarStateFetcherImpl>(
        base::BindLambdaForTesting([]() { return CreateValidToolbarState(); }));
  }

  std::unique_ptr<toolbar_ui_api::IconTableFetcher> GetIconTableFetcher()
      override {
    return std::make_unique<FakeIconTableFetcher>();
  }

  CommandUpdater* GetCommandUpdater() override {
    return reinterpret_cast<CommandUpdater*>(
        chrome::BrowserCommandController::From(browser_));
  }

  OmniboxController* GetOmniboxController() override { return nullptr; }

 private:
  raw_ptr<BrowserWindowInterface> browser_;
  base::WeakPtrFactory<DependencyProvider> weak_factory_{this};
};

class WebUIToolbarInitializer : public WebUIControllerInitalizer {
 public:
  explicit WebUIToolbarInitializer(BrowserWindowInterface* browser)
      : injector_(browser) {}

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
  explicit InitialWebUIBrowserTestBase(
      std::vector<base::test::FeatureRefAndParams> additional_features = {},
      std::vector<base::test::FeatureRef> disabled_features = {}) {
    std::vector<base::test::FeatureRefAndParams> base_features = {
        {features::kInitialWebUI, {{"use_separate_process", "true"}}},
        {features::kWebUIReloadButton, {}},
        {features::kSkipIPCChannelPausingForNonGuests, {}},
        {features::kWebUIInProcessResourceLoadingV2, {}}};

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

    scoped_feature_list_.InitWithFeaturesAndParameters(features,
                                                       disabled_features);
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

using InitialWebUINavigationBrowserTest = InitialWebUIBrowserTestBase;

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

  // Non-Initial WebUI populates default web-content generic font family maps
  // (e.g. "Times New Roman"), whereas Initial WebUI clears them to avoid
  // prewarming or loading unused default web fonts during startup.
  const blink::web_pref::WebPreferences& non_initial_prefs =
      non_initial_webui_web_contents->GetOrCreateWebPreferences();
  EXPECT_FALSE(non_initial_prefs.standard_font_family_map.empty());
  EXPECT_FALSE(non_initial_prefs.serif_font_family_map.empty());

  const blink::web_pref::WebPreferences& initial_prefs =
      initial_webui_web_contents->GetOrCreateWebPreferences();
  EXPECT_TRUE(initial_prefs.standard_font_family_map.empty());
  EXPECT_TRUE(initial_prefs.fixed_font_family_map.empty());
  EXPECT_TRUE(initial_prefs.serif_font_family_map.empty());
  EXPECT_TRUE(initial_prefs.sans_serif_font_family_map.empty());
  EXPECT_TRUE(initial_prefs.cursive_font_family_map.empty());
  EXPECT_TRUE(initial_prefs.fantasy_font_family_map.empty());
  EXPECT_TRUE(initial_prefs.math_font_family_map.empty());

  EXPECT_EQ("flex",
            content::EvalJs(initial_webui_web_contents.get(),
                            "getComputedStyle(document.querySelector("
                            "'toolbar-app')).display"));
}

// Verifies that when only `WebUIReloadButton` is enabled (the `InitialWebUI`
// startup configuration), chrome://webui-toolbar.top-chrome/ only defines
// `<toolbar-app>` and the main-bundle navigation buttons without loading or
// defining non-navigation toolbar custom elements from `lazy_load.js`.
IN_PROC_BROWSER_TEST_F(InitialWebUINavigationBrowserTest,
                       ReloadOnlyToolbarSkipsLazyControls) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  WebUIToolbarInitializer initializer(browser());
  std::unique_ptr<content::WebContents> web_contents =
      CreateAndNavigateWebContents(url, &initializer);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents.get()));

  EXPECT_EQ(true, content::EvalJs(web_contents.get(), R"(
      customElements.whenDefined('toolbar-app').then(
          () => !!customElements.get('reload-button') &&
                !!customElements.get('back-forward-button') &&
                !!customElements.get('home-button') &&
                !!customElements.get('split-tabs-button') &&
                !!document.querySelector('toolbar-app').shadowRoot))"));
  // `lazy_load.js` would be requested from an idle callback registered by
  // `<toolbar-app>`, so wait for a later idle callback before checking that
  // it was never requested.
  EXPECT_EQ(false, content::EvalJs(web_contents.get(), R"(
      new Promise(resolve => requestIdleCallback(resolve, {timeout: 1000}))
          .then(() =>
              !!document.querySelector('script[src$="lazy_load.js"]') ||
              !!customElements.get('location-bar') ||
              !!customElements.get('readonly-omnibox') ||
              !!customElements.get('app-menu-button') ||
              !!customElements.get('avatar-button') ||
              !!customElements.get('webui-toolbar-extensions') ||
              !!customElements.get('pinned-toolbar-actions')))"));
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
  using PageLoad = ukm::builders::InitialWebUIPageLoad;

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

class InitialWebUINavigationTimelineBrowserTest : public InProcessBrowserTest {
 public:
  InitialWebUINavigationTimelineBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> features = {
        {features::kInitialWebUI, {{"use_separate_process", "true"}}},
        {features::kWebUIReloadButton, {{"prewarm_webui", "false"}}},
        {features::kSkipIPCChannelPausingForNonGuests, {}},
        {features::kWebUIInProcessResourceLoadingV2, {}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(
        features, {features::kSpareRendererForSitePerProcess});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kForceNavigationTimelineUkmRecordingForTesting);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

 protected:
  ukm::TestAutoSetUkmRecorder& ukm_recorder() { return *ukm_recorder_; }

  std::unique_ptr<content::WebContents>
  CreateAndNavigateWebContentsWithoutPrecreatedSiteInstance(
      const GURL& url,
      WebUIControllerInitalizer* initializer) {
    content::BrowserContext* browser_context = browser()
                                                   ->tab_strip_model()
                                                   ->GetActiveWebContents()
                                                   ->GetBrowserContext();
    content::WebContents::CreateParams new_contents_params(browser_context);
    std::unique_ptr<content::WebContents> new_web_contents(
        content::WebContents::Create(new_contents_params));
    if (initializer) {
      initializer->Watch(new_web_contents.get());
    }
    webui::SetBrowserWindowInterface(new_web_contents.get(), browser());
    InitializePageLoadMetricsForWebContents(new_web_contents.get());

    // Clean up any spare renderers to force a new process creation.
    content::SpareRenderProcessHostManager::Get().CleanupSparesForTesting();

    // Navigate to `url`.
    content::NavigationController& controller =
        new_web_contents->GetController();
    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.WatchExistingWebContents();

    controller.LoadURLWithParams(
        content::NavigationController::LoadURLParams(url));
    navigation_observer.Wait();

    return new_web_contents;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

// Verifies that UKM NavigationTimeline metrics are correctly recorded for an
// Initial WebUI navigation. This test checks that the NavigationTimeline UKM
// event is recorded and contains the RendererCommitToDidCommitDuration,
// RendererProcessCreated, and RendererProcessLaunched metrics.
IN_PROC_BROWSER_TEST_F(InitialWebUINavigationTimelineBrowserTest,
                       RecordNavigationTimelineUKM) {
  using NavigationTimeline = ukm::builders::NavigationTimeline;

  // 1) Navigate to initial WebUI.
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  WebUIToolbarInitializer initializer(browser());

  // 2) Set up a RunLoop to wait for the UKM entry.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(NavigationTimeline::kEntryName,
                                       ukm_loop.QuitClosure());

  // Use the helper that does not pre-create SiteInstance to ensure the process
  // is created after navigation starts.
  std::unique_ptr<content::WebContents> initial_webui_web_contents =
      CreateAndNavigateWebContentsWithoutPrecreatedSiteInstance(url,
                                                                &initializer);

  // 3) Wait for the UKM entry to be recorded.
  if (ukm_recorder().GetEntriesByName(NavigationTimeline::kEntryName).empty()) {
    ukm_loop.Run();
  }

  // 4) Verify UKM recording using matchers.
  auto entries =
      ukm_recorder().GetEntriesByName(NavigationTimeline::kEntryName);
  EXPECT_THAT(entries,
              testing::ElementsAre(testing::AllOf(
                  ukm::testing::HasMetric("RendererCommitToDidCommitDuration"),
                  ukm::testing::HasMetric("RendererProcessCreated"),
                  ukm::testing::HasMetric("RendererProcessLaunched"))));
}

class PrewarmedWebUINavigationTimelineBrowserTest
    : public InProcessBrowserTest {
 public:
  PrewarmedWebUINavigationTimelineBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> features = {
        {features::kInitialWebUI, {{"use_separate_process", "true"}}},
        {features::kWebUIReloadButton, {{"prewarm_webui", "true"}}},
        {features::kSkipIPCChannelPausingForNonGuests, {}},
        {features::kWebUIInProcessResourceLoadingV2, {}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(
        features, {features::kSpareRendererForSitePerProcess});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kForceNavigationTimelineUkmRecordingForTesting);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

 protected:
  ukm::TestAutoSetUkmRecorder& ukm_recorder() { return *ukm_recorder_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

// Verifies that UKM NavigationTimeline metrics are correctly recorded (as 0)
// when the pre-warmed WebUI navigation reuses an existing process.
//
// Since the test harness already created the first browser window (and thus
// initialized the toolbar WebUI process), creating a second browser window
// here will trigger pre-warming but it will reuse the existing process.
// This reuse results in process creation/launch times that are in the past
// relative to the new navigation start, leading to negative durations.
// We verify that these are clamped to 0 and correctly recorded.
IN_PROC_BROWSER_TEST_F(PrewarmedWebUINavigationTimelineBrowserTest,
                       RecordNavigationTimelineUKMPrewarmedProcessReused) {
  using NavigationTimeline = ukm::builders::NavigationTimeline;

  // 1) Set up a RunLoop to wait for the UKM entry.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(NavigationTimeline::kEntryName,
                                       ukm_loop.QuitClosure());

  // 2) Create a new browser window. This should trigger pre-warming of the
  // toolbar WebUI.
  CreateBrowserWindow(BrowserWindowCreateParams(browser()->GetProfile(),
                                                /*from_user_gesture=*/true));

  // Wait for the navigation to commit and record UKM.
  if (ukm_recorder().GetEntriesByName(NavigationTimeline::kEntryName).empty()) {
    ukm_loop.Run();
  }

  // 3) Verify UKM recording.
  auto entries =
      ukm_recorder().GetEntriesByName(NavigationTimeline::kEntryName);
  ASSERT_FALSE(entries.empty());

  EXPECT_THAT(
      entries,
      testing::ElementsAre(testing::AllOf(
          ukm::testing::HasMetric("RendererCommitToDidCommitDuration"),
          ukm::testing::HasMetricWithValue("RendererProcessCreated", 0),
          ukm::testing::HasMetricWithValue("RendererProcessLaunched", 0))));
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
       ProfileBrowserCollection::GetForProfile(browser()->GetProfile())
                   ->GetSize() > 0
           ? "WithExistingWindow"
           : "WithoutExistingWindow",
       ".BrowserWindowToReloadButton.FirstPaintGap2"});
  base::StatisticsRecorder::HistogramWaiter waiter(expected_metric);

  // Create a new browser window without actively showing/painting it yet.
  BrowserWindowCreateParams params(browser()->GetProfile(),
                                   /*from_user_gesture=*/true);
  BrowserWindowInterface* new_browser =
      waap::CreateBrowserWindowAndRecordCreationSource(
          std::move(params), waap::NewWindowCreationSource::kNewEmptyWindow);

  if (auto* manager = InitialWebUIWindowMetricsManager::From(new_browser)) {
    manager->SkipStartupForTesting();

    base::TimeTicks t1 = base::TimeTicks::Now();
    manager->OnBrowserWindowFirstPresentation(t1);
    manager->GetMetricsDispatcher().OnFirstPaint(t1 + base::Milliseconds(50));
  }

  AddBlankTabAndShow(new_browser);
  waiter.Wait();

  histogram_tester.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithExistingWindow."
      "BrowserWindowToReloadButton.FirstPaintGap2",
      1);
  histogram_tester.ExpectTotalCount(
      "InitialWebUI.NewWindow.NewEmptyWindow.WithExistingWindow."
      "BrowserWindowToReloadButton.FirstPaintGap2",
      1);
}

// Verifies that the WebUI toolbar records the reload button creation exactly
// once for a new normal browser window.
IN_PROC_BROWSER_TEST_F(InitialWebUINavigationBrowserTest,
                       RecordsReloadButtonCreationForNewWindow) {
  ASSERT_TRUE(features::IsWebUIReloadButtonEnabled());
  base::HistogramTester histogram_tester;

  BrowserWindowCreateParams params(browser()->GetProfile(),
                                   /*from_user_gesture=*/true);
  BrowserWindowInterface* new_browser = CreateBrowserWindow(std::move(params));
  ASSERT_TRUE(InitialWebUIWindowMetricsManager::From(new_browser));
  AddBlankTabAndShow(new_browser);

  // The same histogram also records the browser window creation (bucket 0);
  // bucket 1 corresponds to `InitialWebUIView::kReloadButton`.
  histogram_tester.ExpectBucketCount("InitialWebUI.View.Creation",
                                     /*sample=*/1, 1);
}

// Verifies the `NewWindowCreationSource` that each window creation entry point
// assigns to the window it creates.
class InitialWebUIWindowCreationSourceBrowserTest
    : public InitialWebUIBrowserTestBase {
 protected:
  static waap::NewWindowCreationSource GetCreationSource(
      BrowserWindowInterface* browser) {
    auto* manager = InitialWebUIWindowMetricsManager::From(browser);
    CHECK(manager);
    return manager->GetCreationSourceForTesting();
  }
};

IN_PROC_BROWSER_TEST_F(InitialWebUIWindowCreationSourceBrowserTest,
                       NewEmptyWindow) {
  chrome::NewEmptyWindow(browser()->GetProfile(),
                         /*should_trigger_session_restore=*/false);

  BrowserWindowInterface* const new_browser =
      ui_test_utils::GetBrowserNotInSet({browser()});
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(GetCreationSource(new_browser),
            waap::NewWindowCreationSource::kNewEmptyWindow);
}

IN_PROC_BROWSER_TEST_F(InitialWebUIWindowCreationSourceBrowserTest,
                       MoveTabsToNewWindow) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  chrome::MoveTabsToNewWindow(browser(), {1});

  BrowserWindowInterface* const new_browser =
      ui_test_utils::GetBrowserNotInSet({browser()});
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(GetCreationSource(new_browser),
            waap::NewWindowCreationSource::kMoveTabsToNewWindow);
}

// Non-process-startup browser launches don't happen on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(InitialWebUIWindowCreationSourceBrowserTest, Launch) {
  // Simulate a launch handled by the already running browser process, e.g.
  // from an OS shortcut.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line,
                                   chrome::startup::IsFirstRun::kNo);
  launch.Launch(browser()->GetProfile(), chrome::startup::IsProcessStartup::kNo,
                /*restore_tabbed_browser=*/true);

  BrowserWindowInterface* const new_browser =
      ui_test_utils::GetBrowserNotInSet({browser()});
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(GetCreationSource(new_browser),
            waap::NewWindowCreationSource::kLaunch);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(InitialWebUIWindowCreationSourceBrowserTest,
                       SessionRestoreFallback) {
  // Nothing has been saved for this profile yet, so the restore finds no
  // tabbed window and opens a fallback window instead.
  BrowserWindowInterface* const new_browser = SessionRestore::RestoreSession(
      browser()->GetProfile(), /*browser=*/nullptr,
      SessionRestore::SYNCHRONOUS | SessionRestore::RESTORE_BROWSER |
          SessionRestore::ALWAYS_CREATE_TABBED_BROWSER,
      /*startup_tabs=*/{});
  ASSERT_TRUE(new_browser);
  ASSERT_NE(new_browser, browser());
  EXPECT_EQ(GetCreationSource(new_browser),
            waap::NewWindowCreationSource::kSessionRestoreFallback);
}

IN_PROC_BROWSER_TEST_F(InitialWebUIWindowCreationSourceBrowserTest,
                       Navigation) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      ui::mojom::WindowOpenDisposition::kNewWindow,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  BrowserWindowInterface* const new_browser =
      ui_test_utils::GetBrowserNotInSet({browser()});
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(GetCreationSource(new_browser),
            waap::NewWindowCreationSource::kNavigation);
}

IN_PROC_BROWSER_TEST_F(InitialWebUIWindowCreationSourceBrowserTest, Extension) {
  auto function = base::MakeRefCounted<extensions::WindowsCreateFunction>();
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test").Build();
  function->set_extension(extension.get());
  ASSERT_TRUE(extensions::api_test_utils::RunFunction(function.get(), "[]",
                                                      browser()->GetProfile()));

  BrowserWindowInterface* const new_browser =
      ui_test_utils::GetBrowserNotInSet({browser()});
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(GetCreationSource(new_browser),
            waap::NewWindowCreationSource::kExtension);
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

// TODO(crbug.com/491012584, crbug.com/540625890): Flaky on ChromeOS MSan/ASan,
// Win, and Mac.
#if (BUILDFLAG(IS_CHROMEOS) &&                                     \
     (defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER))) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
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
      "BrowserWindowToReloadButton.FirstPaintGap2";

  base::StatisticsRecorder::HistogramWaiter waiter(expected_metric);

  // Create a new window.
  BrowserWindowCreateParams params(browser()->GetProfile(),
                                   /*from_user_gesture=*/true);
  BrowserWindowInterface* new_browser =
      waap::CreateBrowserWindowAndRecordCreationSource(
          std::move(params), waap::NewWindowCreationSource::kNewEmptyWindow);

  if (auto* manager = InitialWebUIWindowMetricsManager::From(new_browser)) {
    manager->SkipStartupForTesting();
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

class InitialWebUIMinimizedWindowBrowserTest
    : public InitialWebUIBrowserTestBase {
 public:
  InitialWebUIMinimizedWindowBrowserTest()
      : InitialWebUIBrowserTestBase(
            {{features::kWebUIReloadButton,
              {{"WebUIReloadButtonDeferBrowserViewShow", "true"}}}}) {}
};

// Tests that the duration metrics are not recorded for windows created as
// minimized.
IN_PROC_BROWSER_TEST_F(InitialWebUIMinimizedWindowBrowserTest,
                       InitiallyMinimizedWindowSkipsMetrics) {
  base::HistogramTester histogram_tester;

  // Create a minimized browser window.
  BrowserWindowCreateParams params(browser()->GetProfile(),
                                   /*from_user_gesture=*/true);
  params.initial_show_state = ui::mojom::WindowShowState::kMinimized;
  BrowserWindowInterface* new_browser =
      waap::CreateBrowserWindowAndRecordCreationSource(
          std::move(params), waap::NewWindowCreationSource::kNewEmptyWindow);

  if (auto* manager = InitialWebUIWindowMetricsManager::From(new_browser)) {
    manager->SkipStartupForTesting();
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
    manager->GetMetricsDispatcher().OnFirstPaint(t1 + base::Milliseconds(50));
  }

  // Verify ShowRequestedToFirstPaint was not recorded.
  histogram_tester.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow.BrowserWindow."
      "ShowRequestedToFirstPaint2",
      0);

  // Verify FirstPaintGap was not recorded.
  histogram_tester.ExpectTotalCount(
      "InitialWebUI.NewWindow.AllSources.WithoutExistingWindow."
      "BrowserWindowToReloadButton.FirstPaintGap2",
      0);
}

// Tests that the duration metrics should be skipped for the windows that are
// restored as minimized.
IN_PROC_BROWSER_TEST_F(InitialWebUIMinimizedWindowBrowserTest,
                       SessionRestoreMinimizedWindow) {
  Profile* profile = browser()->GetProfile();

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

  BrowserWindowInterface* restored_browser = browser_created_observer.Wait();
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
    manager->GetMetricsDispatcher().OnFirstPaint(t1 + base::Milliseconds(50));
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

  histogram_tester.ExpectTotalCount(
      "InitialWebUI.Startup.SessionRestore.ReloadButton.FirstPaint", 0);

  keep_alive.reset();
  profile_keep_alive.reset();
}

#endif  // BUILDFLAG(IS_WIN)

class InitialWebUISameStartupPopupBrowserTest
    : public InitialWebUIBrowserTestBase {
 public:
  InitialWebUISameStartupPopupBrowserTest() {
    set_exit_when_last_browser_closes(false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InitialWebUIBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
    command_line->AppendSwitch(switches::kKeepAliveForTest);
  }
};

// Verify that startup metrics are not incorrectly hijacked by a popup window
// when both a popup and a normal window are created during startup.
// Popups do not have reload buttons and thus cannot report complete startup
// metrics, so they should be ignored by the InitialWebUIWindowMetricsManager.
IN_PROC_BROWSER_TEST_F(InitialWebUISameStartupPopupBrowserTest,
                       PopupPaintsFirstConsistentMetrics) {
  ASSERT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());

  Profile* profile = ProfileManager::GetLastUsedProfile();
  ASSERT_TRUE(profile);

  // Create popup browser.
  BrowserWindowCreateParams popup_params(BrowserWindowInterface::TYPE_POPUP,
                                         profile, /*from_user_gesture=*/true);
  BrowserWindowInterface* popup_browser =
      CreateBrowserWindow(std::move(popup_params));
  ASSERT_TRUE(popup_browser);

  // Create normal browser.
  BrowserWindowCreateParams normal_params(BrowserWindowInterface::TYPE_NORMAL,
                                          profile, /*from_user_gesture=*/true);
  BrowserWindowInterface* normal_browser =
      CreateBrowserWindow(std::move(normal_params));
  ASSERT_TRUE(normal_browser);

  auto* popup_manager = InitialWebUIWindowMetricsManager::From(popup_browser);
  auto* normal_manager = InitialWebUIWindowMetricsManager::From(normal_browser);

  // After the fix, the popup manager should NOT be instantiated.
  ASSERT_EQ(nullptr, popup_manager);
  ASSERT_NE(nullptr, normal_manager);

  base::HistogramTester histogram_tester;

  base::TimeTicks t0 = base::TimeTicks::Now();

  // Reset startup metrics utility state to enable logging even with
  // kNoStartupWindow.
  startup_metric_utils::GetBrowser().ResetSessionForTesting();

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, startup metrics logging requires the time origin to be
  // initialized via RecordWebContentsStartTime(). Since this test uses
  // kNoStartupWindow, the normal browser launch flow is bypassed and we must
  // initialize it manually.
  startup_metric_utils::GetBrowser().RecordWebContentsStartTime(t0);
#endif

  // Reset static state of the manager.
  InitialWebUIWindowMetricsManager::ResetForTesting();

  // Simulate normal window events.
  normal_manager->OnBrowserWindowShowRequested(t0);
  normal_manager->OnBrowserWindowFirstPresentation(t0 +
                                                   base::Milliseconds(100));
  normal_manager->GetMetricsDispatcher().OnFirstPaint(t0 +
                                                      base::Milliseconds(150));

  // Verify histograms.
  // We expect all startup metrics to be recorded from the normal window.
  histogram_tester.ExpectTotalCount(
      "InitialWebUI.Startup.BrowserWindow.FirstPaint", 1);

  histogram_tester.ExpectTotalCount(
      "InitialWebUI.Startup.ReloadButton.FirstPaint", 1);

  histogram_tester.ExpectTotalCount(
      "InitialWebUI.Startup.BrowserWindowToReloadButton.FirstPaintGap", 1);

  histogram_tester.ExpectTotalCount(
      "InitialWebUI.Startup.BrowserWindow.ShowRequestedToFirstPaint", 1);
}

namespace {

// Returns the WebUI toolbar WebContents of `browser` once it has loaded.
content::WebContents* GetLoadedToolbarWebContents(
    BrowserWindowInterface* browser) {
  auto* toolbar_button_provider = ToolbarButtonProvider::From(browser);
  if (!toolbar_button_provider ||
      !toolbar_button_provider->GetWebUIToolbarViewForTesting()) {
    return nullptr;
  }
  content::WebContents* web_contents =
      toolbar_button_provider->GetWebUIToolbarViewForTesting()->web_contents();
  if (web_contents) {
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  }
  return web_contents;
}

}  // namespace

class InitialWebUISharedRendererBrowserTest
    : public InitialWebUIBrowserTestBase {
 public:
  InitialWebUISharedRendererBrowserTest()
      : InitialWebUIBrowserTestBase(
            {{features::kTopChromeWebUISharedRenderer, {}}}) {}
};

// Verifies that the WebUI toolbar of each window is created in
// TopChromeWebUIProfile, with the profile of the window as its user profile.
IN_PROC_BROWSER_TEST_F(InitialWebUISharedRendererBrowserTest,
                       ToolbarIsInTopChromeWebUIProfile) {
  Profile* top_chrome_webui_profile = webui::GetTopChromeWebUIProfile();
  ASSERT_TRUE(top_chrome_webui_profile);

  content::WebContents* toolbar = GetLoadedToolbarWebContents(browser());
  ASSERT_TRUE(toolbar);
  EXPECT_EQ(top_chrome_webui_profile, toolbar->GetBrowserContext());
  EXPECT_EQ(browser()->GetProfile(), webui::GetUserProfile(toolbar));
  ASSERT_TRUE(toolbar->GetWebUI());
  EXPECT_TRUE(toolbar->GetWebUI()->GetController()->GetAs<WebUIToolbarUI>());

  // The toolbar of an incognito window is in the same TopChromeWebUIProfile.
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  content::WebContents* incognito_toolbar =
      GetLoadedToolbarWebContents(incognito_browser);
  ASSERT_TRUE(incognito_toolbar);
  EXPECT_EQ(top_chrome_webui_profile, incognito_toolbar->GetBrowserContext());
  EXPECT_EQ(incognito_browser->GetProfile(),
            webui::GetUserProfile(incognito_toolbar));
  // The toolbars share a renderer process.
  EXPECT_EQ(toolbar->GetPrimaryMainFrame()->GetProcess(),
            incognito_toolbar->GetPrimaryMainFrame()->GetProcess());
}

// Verifies that the renderer preferences of the WebUI toolbar follow the prefs
// of its user profile.
IN_PROC_BROWSER_TEST_F(InitialWebUISharedRendererBrowserTest,
                       ToolbarRendererPreferencesFollowUserProfilePrefs) {
  content::WebContents* toolbar = GetLoadedToolbarWebContents(browser());
  ASSERT_TRUE(toolbar);
  ASSERT_FALSE(toolbar->GetMutableRendererPrefs()->enable_do_not_track);

  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kEnableDoNotTrack,
                                                  true);
  EXPECT_TRUE(toolbar->GetMutableRendererPrefs()->enable_do_not_track);
}

// Profile creation and destruction without a full ChromeOS user session is not
// supported on ChromeOS (Ash).
#if !BUILDFLAG(IS_CHROMEOS)
class InitialWebUIProfileServiceShutdownBrowserTest
    : public InitialWebUIBrowserTestBase {
 public:
  InitialWebUIProfileServiceShutdownBrowserTest()
      : InitialWebUIBrowserTestBase(
            {{features::kWebUIReloadButton,
              {{"WebUIReloadButtonPrewarmWebUI", "true"},
               {"WebUIReloadButtonProfilePrewarming", "true"}}}}) {}
};

// Verifies that destroying a profile without taking the prewarmed toolbar
// WebContents shuts down cleanly without crashing.
//
// When profile prewarming is enabled, `InitialWebUIProfileService` pre-creates
// and holds a `WebContents`. If no browser window is created to consume it via
// `TakeToolbarContents()`, the `WebContents` remains owned by the service until
// profile destruction. It must be reset in
// `InitialWebUIProfileService::Shutdown()` before the `BrowserContext` is
// marked dead, preventing `DependencyManager::AssertContextWasntDestroyed`
// assertions from observers during `WebContents` teardown.
IN_PROC_BROWSER_TEST_F(InitialWebUIProfileServiceShutdownBrowserTest,
                       DestroyProfileWithoutTakingToolbarContents) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path =
      profile_manager->user_data_dir().Append(FILE_PATH_LITERAL("Secondary"));
  Profile& secondary_profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);

  auto* service =
      InitialWebUIProfileServiceFactory::GetForProfile(&secondary_profile);
  ASSERT_TRUE(service);
  EXPECT_TRUE(service->has_toolbar_contents_for_testing());

  ProfileDestructionWaiter waiter(&secondary_profile);
  profile_manager->ClearFirstBrowserWindowKeepAlive(&secondary_profile);
  waiter.Wait();
}

class InitialWebUIProfilePrewarmOmniboxBrowserTest
    : public InitialWebUIBrowserTestBase {
 public:
  InitialWebUIProfilePrewarmOmniboxBrowserTest()
      : InitialWebUIBrowserTestBase(
            {{features::kWebUILocationBar, {}},
             {features::kWebUIReloadButton,
              {{"WebUIReloadButtonPrewarmWebUI", "true"},
               {"WebUIReloadButtonProfilePrewarming", "true"},
               {"WebUIReloadButtonPrewarmWebUIPreNavigate", "true"}}}}) {}
};

// Verifies that when a profile-prewarmed WebUI toolbar has finished navigation
// before the first browser window is created, the WebUIToolbarUI is properly
// initialized with a valid OmniboxController after WebUILocationBar::Init()
// runs, rather than race-initializing prematurely in the WebUIToolbarWebView
// constructor with a null controller (b/563226016).
IN_PROC_BROWSER_TEST_F(InitialWebUIProfilePrewarmOmniboxBrowserTest,
                       PrewarmedToolbarInitializesOmniboxController) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path =
      profile_manager->user_data_dir().Append(FILE_PATH_LITERAL("Secondary"));
  Profile& secondary_profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);

  auto* service =
      InitialWebUIProfileServiceFactory::GetForProfile(&secondary_profile);
  ASSERT_TRUE(service);
  ASSERT_TRUE(service->has_toolbar_contents_for_testing());

  content::WebContents* prewarmed_contents =
      service->toolbar_web_contents_for_testing();
  ASSERT_TRUE(prewarmed_contents);

  // Wait for the background prewarm navigation to commit.
  content::TestNavigationObserver observer(prewarmed_contents);
  observer.Wait();
  ASSERT_TRUE(observer.last_navigation_succeeded());

  // Create the first browser window for this profile, which will consume the
  // prewarmed toolbar contents.
  BrowserWindowInterface* new_browser = CreateBrowser(&secondary_profile);
  ASSERT_TRUE(new_browser);

  auto* toolbar_button_provider = ToolbarButtonProvider::From(new_browser);
  ASSERT_TRUE(toolbar_button_provider);
  auto* webview = toolbar_button_provider->GetWebUIToolbarViewForTesting();
  ASSERT_TRUE(webview);
  content::WebContents* web_contents = webview->web_contents();
  ASSERT_TRUE(web_contents);
  content::WebUI* web_ui = web_contents->GetWebUI();
  ASSERT_TRUE(web_ui);
  auto* webui = web_ui->GetController()->GetAs<WebUIToolbarUI>();
  ASSERT_TRUE(webui);

  // Verify that the OmniboxController was properly wired up and is non-null.
  EXPECT_NE(nullptr, webui->omnibox_controller_for_testing());
  EXPECT_TRUE(webui->is_initialized());
  EXPECT_EQ(webview->GetOmniboxController(),
            webui->omnibox_controller_for_testing());
}

class InitialWebUISharedRendererProfilePrewarmingBrowserTest
    : public InitialWebUIBrowserTestBase {
 public:
  InitialWebUISharedRendererProfilePrewarmingBrowserTest()
      : InitialWebUIBrowserTestBase(
            {{features::kTopChromeWebUISharedRenderer, {}},
             {features::kWebUIReloadButton,
              {{"WebUIReloadButtonPrewarmWebUI", "true"},
               {"WebUIReloadButtonProfilePrewarming", "true"}}}}) {}
};

// Verifies that a new profile doesn't prewarm a toolbar WebContents with
// TopChromeWebUISharedRenderer, and that the toolbar of its first window is
// created in TopChromeWebUIProfile instead, in the same renderer process as
// the toolbar of the other profile.
IN_PROC_BROWSER_TEST_F(InitialWebUISharedRendererProfilePrewarmingBrowserTest,
                       ProfilePrewarmingIsSkipped) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path =
      profile_manager->user_data_dir().Append(FILE_PATH_LITERAL("Secondary"));
  Profile& secondary_profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);

  EXPECT_FALSE(
      InitialWebUIProfileServiceFactory::GetForProfile(&secondary_profile));

  BrowserWindowInterface* new_browser = CreateBrowser(&secondary_profile);
  content::WebContents* toolbar = GetLoadedToolbarWebContents(new_browser);
  ASSERT_TRUE(toolbar);
  EXPECT_EQ(webui::GetTopChromeWebUIProfile(), toolbar->GetBrowserContext());
  EXPECT_EQ(&secondary_profile, webui::GetUserProfile(toolbar));

  content::WebContents* first_toolbar = GetLoadedToolbarWebContents(browser());
  ASSERT_TRUE(first_toolbar);
  EXPECT_EQ(first_toolbar->GetPrimaryMainFrame()->GetProcess(),
            toolbar->GetPrimaryMainFrame()->GetProcess());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace {

constexpr char kTakeResultHistogram[] =
    "InitialWebUI.Toolbar.ProfilePrewarmTakeResult";
using TakeResult = WebUIContentsPrerenderer::TakeResult;

// Returns `additional_features` plus profile prewarming and prerendering, where
// the refill builds the next toolbar as soon as the taken one paints. The
// paint deadline is short, so that refills after windows that built their own
// toolbar, such as the startup window, do not keep tests idle.
std::vector<base::test::FeatureRefAndParams> GetRefillFeatures(
    std::vector<base::test::FeatureRefAndParams> additional_features) {
  additional_features.emplace_back(
      features::kWebUIReloadButton,
      base::FieldTrialParams{
          {"WebUIReloadButtonPrewarmWebUI", "true"},
          {"WebUIReloadButtonPrewarmWebUIPreNavigate", "true"},
          {"WebUIReloadButtonProfilePrewarming", "true"}});
  additional_features.emplace_back(
      features::kTopChromeWebUIPrerendering,
      base::FieldTrialParams{
          {"TopChromeWebUIPrerenderingDelay", "0s"},
          {"TopChromeWebUIPrerenderingPaintDeadline", "100ms"}});
  return additional_features;
}

}  // namespace

// Turns on profile prewarming and prerendering.
class InitialWebUIProfilePrewarmRefillBrowserTest
    : public InitialWebUIBrowserTestBase {
 public:
  InitialWebUIProfilePrewarmRefillBrowserTest()
      : InitialWebUIProfilePrewarmRefillBrowserTest(
            /*additional_features=*/{},
            /*disabled_features=*/{features::kTopChromeWebUISharedRenderer}) {}

 protected:
  explicit InitialWebUIProfilePrewarmRefillBrowserTest(
      std::vector<base::test::FeatureRefAndParams> additional_features,
      std::vector<base::test::FeatureRef> disabled_features)
      : InitialWebUIBrowserTestBase(
            GetRefillFeatures(std::move(additional_features)),
            std::move(disabled_features)) {}

  InitialWebUIProfileService* GetService() {
    return InitialWebUIProfileServiceFactory::GetForProfile(
        browser()->GetProfile());
  }

  // Waits until `service` holds a prewarmed toolbar and returns it. Returns
  // null if none arrives before the wait times out.
  content::WebContents* WaitForPrewarmedToolbar(
      InitialWebUIProfileService* service) {
    if (!base::test::RunUntil(
            [&]() { return service->has_toolbar_contents_for_testing(); })) {
      return nullptr;
    }
    return service->toolbar_web_contents_for_testing();
  }

  content::WebContents* GetToolbarContents(BrowserWindowInterface* browser) {
    WebUIToolbarWebView* webview =
        ToolbarButtonProvider::From(browser)->GetWebUIToolbarViewForTesting();
    return webview ? webview->web_contents() : nullptr;
  }
};

// Verifies that every new window of a profile adopts a prewarmed toolbar, not
// only the first one: after a window takes the prewarmed toolbar, the profile
// service builds the next one.
IN_PROC_BROWSER_TEST_F(InitialWebUIProfilePrewarmRefillBrowserTest,
                       EveryNewWindowAdoptsPrewarmedToolbar) {
  Profile* profile = browser()->GetProfile();
  InitialWebUIProfileService* service = GetService();
  ASSERT_TRUE(service);

  // The startup window took no prewarmed contents during startup and scheduled
  // the first refill. Verify this from the global StatisticsRecorder snapshot.
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(kTakeResultHistogram);
  ASSERT_TRUE(histogram);
  EXPECT_EQ(
      1, histogram->SnapshotSamples()->GetCount(
             static_cast<base::HistogramBase::Sample32>(TakeResult::kNoSpare)));

  // The startup window built its own toolbar and scheduled the first refill.
  // Wait for the refilled toolbar.
  content::WebContents* prewarmed_toolbar = WaitForPrewarmedToolbar(service);
  ASSERT_TRUE(prewarmed_toolbar);

  base::HistogramTester histogram_tester;
  BrowserWindowInterface* second_browser = CreateBrowser(profile);
  ASSERT_TRUE(second_browser);
  EXPECT_EQ(prewarmed_toolbar, GetToolbarContents(second_browser));

  // The next window gets a prewarmed toolbar of its own.
  content::WebContents* next_toolbar = WaitForPrewarmedToolbar(service);
  ASSERT_TRUE(next_toolbar);
  EXPECT_NE(prewarmed_toolbar, next_toolbar);
  BrowserWindowInterface* third_browser = CreateBrowser(profile);
  ASSERT_TRUE(third_browser);
  EXPECT_EQ(next_toolbar, GetToolbarContents(third_browser));
  histogram_tester.ExpectUniqueSample(kTakeResultHistogram,
                                      TakeResult::kAdopted, 2);
}

// Verifies that the refill runs once the taken toolbar paints, without waiting
// for the deadline.
#if !BUILDFLAG(IS_CHROMEOS)
// In ChromeOS browser test environments, background window composition is
// deferred until display activation, so DidFirstVisuallyNonEmptyPaint() does
// not reliably fire without explicit window activation; on ChromeOS, the
// deadline fallback ensures refill completes.
IN_PROC_BROWSER_TEST_F(InitialWebUIProfilePrewarmRefillBrowserTest,
                       RefillsAfterToolbarPaints) {
  InitialWebUIProfileService* service = GetService();
  ASSERT_TRUE(service);
  // Get the first spare under the short startup deadline.
  ASSERT_TRUE(WaitForPrewarmedToolbar(service));

  // Install the long deadline before creating the window that takes the spare,
  // since the deadline is read at ScheduleRefill time.
  service->toolbar_prerenderer_for_testing()->set_paint_deadline_for_testing(
      base::Days(1));
  ASSERT_TRUE(CreateBrowser(browser()->GetProfile()));
  EXPECT_TRUE(WaitForPrewarmedToolbar(service));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Verifies that when multiple windows open simultaneously (back to back without
// waiting for paints), at most one window adopts the prewarmed spare while the
// others build inline, take results are recorded once per window (one adopted,
// others no prewarmed contents or dropped), exactly one new spare exists after
// refill, the next window adopts it, and no crash occurs.
IN_PROC_BROWSER_TEST_F(InitialWebUIProfilePrewarmRefillBrowserTest,
                       SimultaneousNewWindowsAdoptAtMostOneSpare) {
  Profile* profile = browser()->GetProfile();
  InitialWebUIProfileService* service = GetService();
  ASSERT_TRUE(service);

  content::WebContents* spare = WaitForPrewarmedToolbar(service);
  ASSERT_TRUE(spare);

  base::HistogramTester histogram_tester;
  // Create 3 normal windows back to back without waiting for paints.
  BrowserWindowInterface* w1 = CreateBrowserWindow(
      BrowserWindowCreateParams(profile, /*from_user_gesture=*/true));
  BrowserWindowInterface* w2 = CreateBrowserWindow(
      BrowserWindowCreateParams(profile, /*from_user_gesture=*/true));
  BrowserWindowInterface* w3 = CreateBrowserWindow(
      BrowserWindowCreateParams(profile, /*from_user_gesture=*/true));
  ASSERT_TRUE(w1);
  ASSERT_TRUE(w2);
  ASSERT_TRUE(w3);

  int adopted_count = 0;
  if (GetToolbarContents(w1) == spare) {
    ++adopted_count;
  }
  if (GetToolbarContents(w2) == spare) {
    ++adopted_count;
  }
  if (GetToolbarContents(w3) == spare) {
    ++adopted_count;
  }
  EXPECT_EQ(adopted_count, 1);

  histogram_tester.ExpectBucketCount(kTakeResultHistogram, TakeResult::kAdopted,
                                     1);
  histogram_tester.ExpectBucketCount(kTakeResultHistogram, TakeResult::kNoSpare,
                                     2);
  EXPECT_FALSE(service->has_toolbar_contents_for_testing());
  EXPECT_TRUE(service->toolbar_prerenderer_for_testing()
                  ->is_refill_pending_for_testing());

  AddBlankTabAndShow(w1);
  AddBlankTabAndShow(w2);
  AddBlankTabAndShow(w3);

  content::WebContents* next_spare = WaitForPrewarmedToolbar(service);
  ASSERT_TRUE(next_spare);
  EXPECT_NE(next_spare, spare);
  EXPECT_TRUE(service->has_toolbar_contents_for_testing());

  base::HistogramTester next_histogram_tester;
  BrowserWindowInterface* fourth_browser = CreateBrowser(profile);
  ASSERT_TRUE(fourth_browser);
  EXPECT_EQ(next_spare, GetToolbarContents(fourth_browser));
  next_histogram_tester.ExpectUniqueSample(kTakeResultHistogram,
                                           TakeResult::kAdopted, 1);
}

// Verifies that a window builds its own toolbar instead of adopting a
// prewarmed one that read the colors before they changed, and that the
// refill builds with the new colors which the next window adopts.
IN_PROC_BROWSER_TEST_F(InitialWebUIProfilePrewarmRefillBrowserTest,
                       DropsStaleToolbarAfterColorSchemeChange) {
  Profile* profile = browser()->GetProfile();
  InitialWebUIProfileService* service = GetService();
  ASSERT_TRUE(service);
  ASSERT_TRUE(WaitForPrewarmedToolbar(service));

  const ui::ColorProviderKey old_key =
      PrewarmHelper::GetColorProviderKey(profile);
  ThemeServiceFactory::GetForProfile(profile)->SetBrowserColorScheme(
      old_key.color_mode == ui::ColorProviderKey::ColorMode::kDark
          ? ThemeService::BrowserColorScheme::kLight
          : ThemeService::BrowserColorScheme::kDark);
  const ui::ColorProviderKey new_key =
      PrewarmHelper::GetColorProviderKey(profile);
  ASSERT_NE(old_key, new_key);

  base::HistogramTester histogram_tester;
  BrowserWindowInterface* second_browser = CreateBrowser(profile);
  ASSERT_TRUE(second_browser);
  EXPECT_TRUE(GetToolbarContents(second_browser));
  histogram_tester.ExpectUniqueSample(kTakeResultHistogram,
                                      TakeResult::kDroppedStale, 1);

  content::WebContents* refilled_toolbar = WaitForPrewarmedToolbar(service);
  ASSERT_TRUE(refilled_toolbar);
  EXPECT_EQ(new_key.color_mode, refilled_toolbar->GetColorMode());

  BrowserWindowInterface* third_browser = CreateBrowser(profile);
  ASSERT_TRUE(third_browser);
  EXPECT_EQ(refilled_toolbar, GetToolbarContents(third_browser));
  histogram_tester.ExpectBucketCount(kTakeResultHistogram, TakeResult::kAdopted,
                                     1);
}

// Verifies that a window builds its own toolbar instead of adopting a
// prewarmed one that read the touch mode, and so its layout constants, before
// the touch mode changed.
IN_PROC_BROWSER_TEST_F(InitialWebUIProfilePrewarmRefillBrowserTest,
                       DropsStaleToolbarAfterTouchModeChange) {
  InitialWebUIProfileService* service = GetService();
  ASSERT_TRUE(service);
  ASSERT_TRUE(WaitForPrewarmedToolbar(service));

  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper(
      !ui::TouchUiController::Get()->touch_ui());

  base::HistogramTester histogram_tester;
  BrowserWindowInterface* new_browser = CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(new_browser);
  EXPECT_TRUE(GetToolbarContents(new_browser));
  histogram_tester.ExpectUniqueSample(kTakeResultHistogram,
                                      TakeResult::kDroppedStale, 1);
}

// Verifies that a toolbar prewarmed while the profile shows the home button
// has the button in its initial state, so it shows from the first frame, and
// that turning the button on drops a toolbar prewarmed without it.
IN_PROC_BROWSER_TEST_F(InitialWebUIProfilePrewarmRefillBrowserTest,
                       PrewarmedToolbarStartsWithHomeButton) {
  Profile* profile = browser()->GetProfile();
  InitialWebUIProfileService* service = GetService();
  ASSERT_TRUE(service);
  ASSERT_TRUE(WaitForPrewarmedToolbar(service));
  ASSERT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kShowHomeButton));

  profile->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(CreateBrowser(profile));
  histogram_tester.ExpectUniqueSample(kTakeResultHistogram,
                                      TakeResult::kDroppedStale, 1);

  // The refill builds the next toolbar with the home button. Wait for it to
  // commit, so that it reads its initial state before a window adopts it.
  content::WebContents* prewarmed_toolbar = WaitForPrewarmedToolbar(service);
  ASSERT_TRUE(prewarmed_toolbar);
  ASSERT_TRUE(content::WaitForLoadStop(prewarmed_toolbar));
  BrowserWindowInterface* new_browser = CreateBrowser(profile);
  ASSERT_TRUE(new_browser);
  ASSERT_EQ(prewarmed_toolbar, GetToolbarContents(new_browser));
  histogram_tester.ExpectBucketCount(kTakeResultHistogram, TakeResult::kAdopted,
                                     1);
  EXPECT_EQ(true, content::EvalJs(
                      prewarmed_toolbar,
                      "JSON.parse(chrome.getVariableValue('initialState'))"
                      ".homeButtonShouldBeShown"));
}

// Verifies that a window builds its own toolbar instead of adopting a
// prewarmed one whose renderer is gone, which the window would not recover.
IN_PROC_BROWSER_TEST_F(InitialWebUIProfilePrewarmRefillBrowserTest,
                       DropsToolbarWithCrashedRenderer) {
  InitialWebUIProfileService* service = GetService();
  ASSERT_TRUE(service);
  content::WebContents* prewarmed_toolbar = WaitForPrewarmedToolbar(service);
  ASSERT_TRUE(prewarmed_toolbar);

  content::RenderProcessHost* process =
      prewarmed_toolbar->GetPrimaryMainFrame()->GetProcess();
  content::ScopedAllowRendererCrashes allow_renderer_crashes(process);
  content::RenderProcessHostWatcher exit_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(content::RESULT_CODE_KILLED);
  exit_observer.Wait();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return prewarmed_toolbar->IsCrashed(); }));

  base::HistogramTester histogram_tester;
  BrowserWindowInterface* new_browser = CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(new_browser);
  WebUIToolbarWebView* webview =
      ToolbarButtonProvider::From(new_browser)->GetWebUIToolbarViewForTesting();
  ASSERT_TRUE(webview);
  ASSERT_TRUE(webview->web_contents());
  EXPECT_FALSE(webview->web_contents()->IsCrashed());
  histogram_tester.ExpectUniqueSample(kTakeResultHistogram,
                                      TakeResult::kDroppedCrashed, 1);

  // The window's own toolbar paints.
  if (!webview->web_contents()->CompletedFirstVisuallyNonEmptyPaint()) {
    base::RunLoop first_paint_loop;
    webview->SetDidFirstNonEmptyPaintCallbackForTesting(
        first_paint_loop.QuitClosure());
    first_paint_loop.Run();
  }
}

// Verifies that an incognito window does not take the profile's prewarmed
// toolbar, and runs its own.
IN_PROC_BROWSER_TEST_F(InitialWebUIProfilePrewarmRefillBrowserTest,
                       IncognitoWindowDoesNotTakePrewarmedToolbar) {
  InitialWebUIProfileService* service = GetService();
  ASSERT_TRUE(service);
  content::WebContents* prewarmed_toolbar = WaitForPrewarmedToolbar(service);
  ASSERT_TRUE(prewarmed_toolbar);

  base::HistogramTester histogram_tester;
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito_browser);
  EXPECT_EQ(prewarmed_toolbar, service->toolbar_web_contents_for_testing());
  histogram_tester.ExpectTotalCount(kTakeResultHistogram, 0);
  content::WebContents* incognito_toolbar =
      GetToolbarContents(incognito_browser);
  ASSERT_TRUE(incognito_toolbar);
  EXPECT_EQ(browser()->GetProfile()->GetPrimaryOTRProfile(
                /*create_if_needed=*/false),
            incognito_toolbar->GetBrowserContext());
}

class InitialWebUIProfilePrewarmRefillOmniboxBrowserTest
    : public InitialWebUIProfilePrewarmRefillBrowserTest {
 public:
  InitialWebUIProfilePrewarmRefillOmniboxBrowserTest()
      : InitialWebUIProfilePrewarmRefillBrowserTest(
            /*additional_features=*/{{features::kWebUILocationBar, {}}},
            /*disabled_features=*/{features::kTopChromeWebUISharedRenderer}) {}
};

// Verifies that a window that adopts a refilled toolbar, whose WebUI committed
// before the window existed, wires the toolbar to the window's omnibox
// controller (b/563226016).
IN_PROC_BROWSER_TEST_F(InitialWebUIProfilePrewarmRefillOmniboxBrowserTest,
                       RefilledToolbarInitializesOmniboxController) {
  InitialWebUIProfileService* service = GetService();
  ASSERT_TRUE(service);
  // The startup window built its own toolbar; this is the first refill.
  content::WebContents* refilled_toolbar = WaitForPrewarmedToolbar(service);
  ASSERT_TRUE(refilled_toolbar);
  ASSERT_TRUE(content::WaitForLoadStop(refilled_toolbar));

  base::HistogramTester histogram_tester;
  BrowserWindowInterface* new_browser = CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(new_browser);
  histogram_tester.ExpectUniqueSample(kTakeResultHistogram,
                                      TakeResult::kAdopted, 1);
  WebUIToolbarWebView* webview =
      ToolbarButtonProvider::From(new_browser)->GetWebUIToolbarViewForTesting();
  ASSERT_TRUE(webview);
  ASSERT_EQ(refilled_toolbar, webview->web_contents());
  auto* webui =
      refilled_toolbar->GetWebUI()->GetController()->GetAs<WebUIToolbarUI>();
  ASSERT_TRUE(webui);
  ASSERT_TRUE(base::test::RunUntil([&]() { return webui->is_initialized(); }));
  EXPECT_NE(nullptr, webui->omnibox_controller_for_testing());
  EXPECT_EQ(webview->GetOmniboxController(),
            webui->omnibox_controller_for_testing());
}

// Profile keep-alives behave differently on ChromeOS (Ash), and profile
// creation and destruction need a full ChromeOS user session there.
#if !BUILDFLAG(IS_CHROMEOS)
// Verifies that a profile kept alive without windows, as in background mode,
// keeps no prewarmed toolbar, and that its next window restarts the refill.
IN_PROC_BROWSER_TEST_F(InitialWebUIProfilePrewarmRefillBrowserTest,
                       DropsToolbarWhenLastWindowCloses) {
  Profile* profile = browser()->GetProfile();
  InitialWebUIProfileService* service = GetService();
  ASSERT_TRUE(service);
  ASSERT_TRUE(WaitForPrewarmedToolbar(service));

  ScopedKeepAlive keep_alive(KeepAliveOrigin::BACKGROUND_MODE_MANAGER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBackgroundMode);
  CloseBrowserSynchronously(browser());
  EXPECT_FALSE(service->has_toolbar_contents_for_testing());

  ASSERT_TRUE(CreateBrowser(profile));
  EXPECT_TRUE(WaitForPrewarmedToolbar(service));
}

// Verifies that a profile whose last window closes while it holds a refilled
// toolbar is destroyed cleanly, and that another profile keeps its own
// prewarmed toolbar.
IN_PROC_BROWSER_TEST_F(InitialWebUIProfilePrewarmRefillBrowserTest,
                       DestroysProfileWithRefilledToolbar) {
  InitialWebUIProfileService* default_service = GetService();
  ASSERT_TRUE(default_service);
  ASSERT_TRUE(WaitForPrewarmedToolbar(default_service));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile& profile = profiles::testing::CreateProfileSync(
      profile_manager,
      profile_manager->user_data_dir().Append(FILE_PATH_LITERAL("Secondary")));
  InitialWebUIProfileService* service =
      InitialWebUIProfileServiceFactory::GetForProfile(&profile);
  ASSERT_TRUE(service);
  BrowserWindowInterface* secondary_browser = CreateBrowser(&profile);
  ASSERT_TRUE(secondary_browser);
  // The refill builds the next toolbar while the window is open.
  ASSERT_TRUE(WaitForPrewarmedToolbar(service));

  ProfileDestructionWaiter waiter(&profile);
  CloseBrowserSynchronously(secondary_browser);
  waiter.Wait();

  EXPECT_TRUE(default_service->has_toolbar_contents_for_testing());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

class InitialWebUISharedRendererProfilePrewarmRefillBrowserTest
    : public InitialWebUIProfilePrewarmRefillBrowserTest {
 public:
  InitialWebUISharedRendererProfilePrewarmRefillBrowserTest()
      : InitialWebUIProfilePrewarmRefillBrowserTest(
            {{features::kTopChromeWebUISharedRenderer, {}}},
            /*disabled_features=*/{}) {}
};

// Verifies that with kTopChromeWebUISharedRenderer and prerendering enabled,
// window 2 adopts the spare in TopChromeWebUIProfile with its user profile, and
// a new spare appears.
IN_PROC_BROWSER_TEST_F(
    InitialWebUISharedRendererProfilePrewarmRefillBrowserTest,
    SharedRendererSpareAdoptedAndRefilled) {
  Profile* profile = browser()->GetProfile();
  InitialWebUIProfileService* service = GetService();
  ASSERT_TRUE(service);

  // With prerendering enabled, no toolbar is built at profile creation. The
  // startup window finds no spare, builds its own toolbar, and refill builds
  // the first spare once that window paints or hits the deadline.
  content::WebContents* spare = WaitForPrewarmedToolbar(service);
  ASSERT_TRUE(spare);

  // Window 2 of the same profile adopts the spare.
  BrowserWindowInterface* second_browser = CreateBrowser(profile);
  ASSERT_TRUE(second_browser);
  content::WebContents* second_toolbar = GetToolbarContents(second_browser);
  ASSERT_TRUE(second_toolbar);
  EXPECT_EQ(spare, second_toolbar);
  EXPECT_EQ(webui::GetTopChromeWebUIProfile(),
            second_toolbar->GetBrowserContext());
  EXPECT_EQ(profile, webui::GetUserProfile(second_toolbar));

  // A new spare appears after window 2 adopts the previous one.
  content::WebContents* next_spare = WaitForPrewarmedToolbar(service);
  ASSERT_TRUE(next_spare);
  EXPECT_NE(spare, next_spare);
  EXPECT_EQ(webui::GetTopChromeWebUIProfile(), next_spare->GetBrowserContext());
  EXPECT_EQ(profile, webui::GetUserProfile(next_spare));
}

// Creating and destroying a profile needs a full user session on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
// Verifies that with kTopChromeWebUISharedRenderer and prerendering enabled, a
// window of a second regular profile never adopts the first profile's spare,
// and that the second profile is destroyed cleanly while TopChromeWebUIProfile
// holds its spare.
IN_PROC_BROWSER_TEST_F(
    InitialWebUISharedRendererProfilePrewarmRefillBrowserTest,
    SharedRendererSpareIsolatedAcrossProfiles) {
  InitialWebUIProfileService* service = GetService();
  ASSERT_TRUE(service);
  content::WebContents* spare = WaitForPrewarmedToolbar(service);
  ASSERT_TRUE(spare);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath secondary_path = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("SecondarySharedRefill"));
  Profile& secondary_profile =
      profiles::testing::CreateProfileSync(profile_manager, secondary_path);

  InitialWebUIProfileService* secondary_service =
      InitialWebUIProfileServiceFactory::GetForProfile(&secondary_profile);
  ASSERT_TRUE(secondary_service);

  BrowserWindowInterface* secondary_browser = CreateBrowser(&secondary_profile);
  ASSERT_TRUE(secondary_browser);
  content::WebContents* secondary_toolbar =
      GetToolbarContents(secondary_browser);
  ASSERT_TRUE(secondary_toolbar);

  // The first profile's service still holds its own spare after the second
  // profile's window opens.
  EXPECT_EQ(spare, service->toolbar_web_contents_for_testing());
  EXPECT_EQ(webui::GetTopChromeWebUIProfile(),
            secondary_toolbar->GetBrowserContext());
  EXPECT_EQ(&secondary_profile, webui::GetUserProfile(secondary_toolbar));

  // The refill builds the second profile's spare while its window is open.
  ASSERT_TRUE(WaitForPrewarmedToolbar(secondary_service));
  ProfileDestructionWaiter waiter(&secondary_profile);
  CloseBrowserSynchronously(secondary_browser);
  waiter.Wait();

  EXPECT_EQ(spare, service->toolbar_web_contents_for_testing());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace waap
