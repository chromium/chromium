// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/service_worker_registration_waiter.h"
#include "chrome/browser/web_applications/test/web_app_icon_waiter.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#endif

using ::testing::ElementsAre;

namespace {

constexpr char kHistogramClosingReason[] =
    "WebApp.DefaultOffline.ClosingReason";
constexpr char kHistogramDurationShown[] =
    "WebApp.DefaultOffline.DurationShown";

}  // namespace

namespace web_app {

enum class PageFlagParam {
  kWithDefaultPageFlag = 0,
  kWithoutDefaultPageFlag = 1,
  kMaxValue = kWithoutDefaultPageFlag
};

class WebAppOfflineTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    base::ScopedAllowBlockingForTesting allow_blocking;
    override_registration_ =
        OsIntegrationTestOverrideImpl::OverrideForTesting();
  }
  void TearDownOnMainThread() override {
    test::UninstallAllWebApps(browser()->profile());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      override_registration_.reset();
    }
  }

  // Start a web app without a service worker and disconnect.
  web_app::AppId StartWebAppAndDisconnect(content::WebContents* web_contents,
                                          base::StringPiece relative_url) {
    GURL target_url(embedded_test_server()->GetURL(relative_url));
    web_app::NavigateToURLAndWait(browser(), target_url);
    web_app::AppId app_id = web_app::test::InstallPwaForCurrentUrl(browser());
    WebAppIconWaiter(browser()->profile(), app_id).Wait();
    std::unique_ptr<content::URLLoaderInterceptor> interceptor =
        content::URLLoaderInterceptor::SetupRequestFailForURL(
            target_url, net::ERR_INTERNET_DISCONNECTED);

    content::TestNavigationObserver observer(web_contents, 1);
    web_contents->GetController().Reload(content::ReloadType::NORMAL, false);
    observer.Wait();
    return app_id;
  }

  // Start a PWA with a service worker and disconnect.
  void StartPwaAndDisconnect(content::WebContents* web_contents,
                             base::StringPiece relative_url) {
    GURL target_url(embedded_test_server()->GetURL(relative_url));
    web_app::ServiceWorkerRegistrationWaiter registration_waiter(
        browser()->profile(), target_url);
    web_app::NavigateToURLAndWait(browser(), target_url);
    registration_waiter.AwaitRegistration();
    web_app::AppId app_id = web_app::test::InstallPwaForCurrentUrl(browser());
    WebAppIconWaiter(browser()->profile(), app_id).Wait();
    std::unique_ptr<content::URLLoaderInterceptor> interceptor =
        content::URLLoaderInterceptor::SetupRequestFailForURL(
            target_url, net::ERR_INTERNET_DISCONNECTED);

    content::TestNavigationObserver observer(web_contents, 1);
    web_contents->GetController().Reload(content::ReloadType::NORMAL, false);
    observer.Wait();
  }

  void ReloadWebContents(content::WebContents* web_contents) {
    content::TestNavigationObserver observer(web_contents, 1);
    web_contents->GetController().Reload(content::ReloadType::NORMAL, false);
    observer.Wait();
  }

  void CloseBrowser(content::WebContents* web_contents) {
    Browser* app_browser = chrome::FindBrowserWithWebContents(web_contents);
    app_browser->window()->Close();
    ui_test_utils::WaitForBrowserToClose(app_browser);
  }

 private:
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
};

class WebAppOfflinePageTest
    : public WebAppOfflineTest,
      public ::testing::WithParamInterface<PageFlagParam> {
 public:
  WebAppOfflinePageTest() {
    if (GetParam() == PageFlagParam::kWithDefaultPageFlag) {
      feature_list_.InitAndEnableFeature(features::kPWAsDefaultOfflinePage);
    } else {
      feature_list_.InitAndDisableFeature(features::kPWAsDefaultOfflinePage);
    }
  }

  void SyncHistograms() {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  }

  // Expect that the histogram has been updated.
  void ExpectUniqueSample(net::Error error, int samples) {
    SyncHistograms();
    histogram_tester_.ExpectUniqueSample(
        "Net.ErrorPageCounts.WebAppAlternativeErrorPage", -error, samples);
  }

  base::HistogramTester* histogram() { return &histogram_tester_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

// When a web app with a manifest and no service worker is offline it should
// display the default offline page rather than the dino.
// When the exact same conditions are applied with the feature flag disabled
// expect that the default offline page is not shown.
IN_PROC_BROWSER_TEST_P(WebAppOfflinePageTest, WebAppOfflinePageIsDisplayed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  if (GetParam() == PageFlagParam::kWithDefaultPageFlag) {
    ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
    // Expect that the default offline page is showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') !== null")
            .ExtractBool());
  } else {
    ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
    // Expect that the default offline page is not showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') === null")
            .ExtractBool());
  }
}

// When a web app with a manifest and service worker that doesn't handle being
// offline it should display the default offline page rather than the dino.
IN_PROC_BROWSER_TEST_P(WebAppOfflinePageTest,
                       WebAppOfflineWithEmptyServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartPwaAndDisconnect(web_contents, "/banners/background-color.html");

  if (GetParam() == PageFlagParam::kWithDefaultPageFlag) {
    ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
    // Expect that the default offline page is showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') !== null")
            .ExtractBool());
  } else {
    ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
    // Expect that the default offline page is not showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') === null")
            .ExtractBool());
  }
}

// When a web app with a manifest and service worker that handles being offline
// it should not display the default offline page.
IN_PROC_BROWSER_TEST_P(WebAppOfflinePageTest, WebAppOfflineWithServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartPwaAndDisconnect(web_contents, "/banners/theme-color.html");

  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  // Expect that the default offline page is not showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') === null")
                  .ExtractBool());
}

// Default offline page icon test.
IN_PROC_BROWSER_TEST_P(WebAppOfflinePageTest, WebAppOfflinePageIconShowing) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  StartWebAppAndDisconnect(web_contents,
                           "/banners/no_sw_fetch_handler_test_page.html");
  WaitForLoadStop(web_contents);

  constexpr char kExpectedIconUrl[] =
      "data:image/png;base64,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"
      "uDwDL2AAAAAElFTkSuQmCC";

  if (GetParam() == PageFlagParam::kWithDefaultPageFlag) {
    // Ensure that we don't proceed until the icon loading is finished.
    ASSERT_EQ(
        true,
        EvalJs(web_contents,
               "var promiseResolve;"
               "var imageLoadedPromise = new Promise(resolve => {"
               "  promiseResolve = resolve;"
               "});"
               "function mutatedCallback(mutations) {"
               "  let mutation = mutations[0];"
               "  if (mutation.attributeName == 'src' &&"
               "      mutation.target.src.startsWith('data:image/png')) {"
               "    console.log('Change in src observed, resolving promise');"
               "    promiseResolve();"
               "  }"
               "}"
               "let observer = new MutationObserver(mutatedCallback);"
               "observer.observe(document.getElementById('icon'),"
               "                 {attributes: true});"
               "if (document.getElementById('icon').src.startsWith("
               "    'data:image/png')) {"
               "  console.log('Inline src already set, resolving promise');"
               "  promiseResolve();"
               "}"
               "imageLoadedPromise.then(function(e) {"
               "  return true;"
               "});")
            .ExtractBool());

    // Expect that the icon on the default offline page is showing.
    EXPECT_EQ(
        "You're offline",
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg').textContent")
            .ExtractString());
    EXPECT_EQ("Manifest test app",
              EvalJs(web_contents, "document.title").ExtractString());
    EXPECT_EQ(kExpectedIconUrl,
              EvalJs(web_contents, "document.getElementById('icon').src")
                  .ExtractString());
    EXPECT_EQ("inline",
              EvalJs(web_contents,
                     "document.getElementById('offlineIcon').style.display")
                  .ExtractString());
  } else {
    // Expect that the default offline page is not showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') === null")
            .ExtractBool());
  }
}

IN_PROC_BROWSER_TEST_P(WebAppOfflinePageTest, WebAppOfflineMetricsNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 0);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 0);

  if (GetParam() == PageFlagParam::kWithDefaultPageFlag) {
    ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
    // Expect that the default offline page is showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') !== null")
            .ExtractBool());

    // Navigate somewhere else (anywhere else but the current page will do).
    EXPECT_TRUE(NavigateToURL(web_contents, GURL("about:blank")));

    SyncHistograms();
    histogram()->ExpectTotalCount(kHistogramDurationShown, 1);
    histogram()->ExpectTotalCount(kHistogramClosingReason, 1);
    EXPECT_THAT(histogram()->GetAllSamples(kHistogramClosingReason),
                ElementsAre(base::Bucket(/* min= */ 1, /* count= */ 1)));
  } else {
    ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
    // Expect that the default offline page is not showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') === null")
            .ExtractBool());

    // Navigate somewhere else (anywhere else but the current page will do).
    EXPECT_TRUE(NavigateToURL(web_contents, GURL("about:blank")));

    // There should be no histograms still.
    SyncHistograms();
    histogram()->ExpectTotalCount(kHistogramDurationShown, 0);
    histogram()->ExpectTotalCount(kHistogramClosingReason, 0);
  }
}

IN_PROC_BROWSER_TEST_P(WebAppOfflinePageTest, WebAppOfflineMetricsBackOnline) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 0);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 0);

  if (GetParam() == PageFlagParam::kWithDefaultPageFlag) {
    ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
    // Expect that the default offline page is showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') !== null")
            .ExtractBool());

    // The URL interceptor only blocks the first navigation. This one should
    // go through.
    ReloadWebContents(web_contents);

    // Expect that the default offline page is not showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') === null")
            .ExtractBool());

    SyncHistograms();
    histogram()->ExpectTotalCount(kHistogramDurationShown, 1);
    histogram()->ExpectTotalCount(kHistogramClosingReason, 1);
    EXPECT_THAT(histogram()->GetAllSamples(kHistogramClosingReason),
                ElementsAre(base::Bucket(/* min= */ 0, /* count= */ 1)));
  } else {
    ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
    // Expect that the default offline page is not showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') === null")
            .ExtractBool());

    // The URL interceptor only blocks the first navigation. This one should
    // go through.
    ReloadWebContents(web_contents);

    // There should be no histograms still.
    SyncHistograms();
    histogram()->ExpectTotalCount(kHistogramDurationShown, 0);
    histogram()->ExpectTotalCount(kHistogramClosingReason, 0);
  }
}

IN_PROC_BROWSER_TEST_P(WebAppOfflinePageTest, WebAppOfflineMetricsPwaClosing) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  web_app::AppId app_id =
      StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 0);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 0);

  if (GetParam() == PageFlagParam::kWithDefaultPageFlag) {
    ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
    // Expect that the default offline page is showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') !== null")
            .ExtractBool());

    CloseBrowser(web_contents);

    SyncHistograms();
    histogram()->ExpectTotalCount(kHistogramDurationShown, 1);
    histogram()->ExpectTotalCount(kHistogramClosingReason, 1);
    EXPECT_THAT(histogram()->GetAllSamples(kHistogramClosingReason),
                ElementsAre(base::Bucket(/* min= */ 2, /* count= */ 1)));
  } else {
    ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
    // Expect that the default offline page is not showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') === null")
            .ExtractBool());

    CloseBrowser(web_contents);

    // There should be no histograms still.
    SyncHistograms();
    histogram()->ExpectTotalCount(kHistogramDurationShown, 0);
    histogram()->ExpectTotalCount(kHistogramClosingReason, 0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppOfflinePageTest,
    ::testing::Values(PageFlagParam::kWithDefaultPageFlag,
                      PageFlagParam::kWithoutDefaultPageFlag));

class WebAppOfflineDarkModeTest
    : public WebAppOfflineTest,
      public testing::WithParamInterface<blink::mojom::PreferredColorScheme> {
 public:
  WebAppOfflineDarkModeTest() {
    std::vector<base::test::FeatureRef> disabled_features;
    feature_list_.InitWithFeatures({features::kPWAsDefaultOfflinePage,
                                    blink::features::kWebAppEnableDarkMode},
                                   {disabled_features});
  }

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    InProcessBrowserTest::SetUp();
#elif BUILDFLAG(IS_MAC)
    // TODO(crbug.com/1298658): Get this test suite working.
    GTEST_SKIP();
#else
    InProcessBrowserTest::SetUp();
#endif  // BUILDFLAG(IS_MAC)
  }

  void SetUpOnMainThread() override {
    WebAppOfflineTest::SetUpOnMainThread();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Explicitly set dark mode in ChromeOS or we can't get light mode after
    // sunset (due to dark mode auto-scheduling).
    ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(
        GetParam() == blink::mojom::PreferredColorScheme::kDark);
#endif
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // ShellContentBrowserClient::OverrideWebkitPrefs() overrides the
    // prefers-color-scheme according to switches::kForceDarkMode
    // command line.
    if (GetParam() == blink::mojom::PreferredColorScheme::kDark)
      command_line->AppendSwitch(switches::kForceDarkMode);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Testing offline page in dark mode for a web app with a manifest and no
// service worker.
// TODO(crbug.com/1373750): tests are flaky on Lacros and Linux.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_WebAppOfflineDarkModeNoServiceWorker \
  DISABLED_WebAppOfflineDarkModeNoServiceWorker
#else
#define MAYBE_WebAppOfflineDarkModeNoServiceWorker \
  WebAppOfflineDarkModeNoServiceWorker
#endif
IN_PROC_BROWSER_TEST_P(WebAppOfflineDarkModeTest,
                       MAYBE_WebAppOfflineDarkModeNoServiceWorker) {
#if BUILDFLAG(IS_WIN)
  if (GetParam() == blink::mojom::PreferredColorScheme::kLight &&
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()) {
    GTEST_SKIP() << "Host is in dark mode; skipping test";
  }
#endif  // BUILDFLAG(IS_WIN)

  ASSERT_TRUE(embedded_test_server()->Start());

  // ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  StartWebAppAndDisconnect(
      web_contents, "/web_apps/get_manifest.html?color_scheme_dark.json");

  // Expect that the default offline page is showing with dark mode colors.
  if (GetParam() == blink::mojom::PreferredColorScheme::kDark) {
    EXPECT_TRUE(
        EvalJs(web_contents,
               "window.matchMedia('(prefers-color-scheme: dark)').matches")
            .ExtractBool());
    EXPECT_EQ(
        EvalJs(web_contents,
               "window.getComputedStyle(document.querySelector('div')).color")
            .ExtractString(),
        "rgb(227, 227, 227)");
    EXPECT_EQ(EvalJs(web_contents,
                     "window.getComputedStyle(document.querySelector('body'))."
                     "backgroundColor")
                  .ExtractString(),
              "rgb(31, 31, 31)");
  } else {
    EXPECT_TRUE(
        EvalJs(web_contents,
               "window.matchMedia('(prefers-color-scheme: light)').matches")
            .ExtractBool());
    EXPECT_EQ(
        EvalJs(web_contents,
               "window.getComputedStyle(document.querySelector('div')).color")
            .ExtractString(),
        "rgb(31, 31, 31)");
    EXPECT_EQ(EvalJs(web_contents,
                     "window.getComputedStyle(document.querySelector('body'))."
                     "backgroundColor")
                  .ExtractString(),
              "rgb(255, 255, 255)");
  }
}

// Testing offline page in dark mode for a web app with a manifest and service
// worker that does not handle offline error.
// TODO(crbug.com/1373750): tests are flaky on Lacros and Linux.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_WebAppOfflineDarkModeEmptyServiceWorker \
  DISABLED_WebAppOfflineDarkModeEmptyServiceWorker
#else
#define MAYBE_WebAppOfflineDarkModeEmptyServiceWorker \
  WebAppOfflineDarkModeEmptyServiceWorker
#endif
IN_PROC_BROWSER_TEST_P(WebAppOfflineDarkModeTest,
                       MAYBE_WebAppOfflineDarkModeEmptyServiceWorker) {
#if BUILDFLAG(IS_WIN)
  if (GetParam() == blink::mojom::PreferredColorScheme::kLight &&
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()) {
    GTEST_SKIP() << "Host is in dark mode; skipping test";
  }
#endif  // BUILDFLAG(IS_WIN)

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  StartPwaAndDisconnect(
      web_contents,
      "/banners/manifest_test_page_empty_fetch_handler.html?manifest=../"
      "web_apps/color_scheme_dark.json");
  if (GetParam() == blink::mojom::PreferredColorScheme::kDark) {
    // Expect that the default offline page is showing with dark mode colors.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "window.matchMedia('(prefers-color-scheme: dark)').matches")
            .ExtractBool());
    EXPECT_EQ(
        EvalJs(web_contents,
               "window.getComputedStyle(document.querySelector('div')).color")
            .ExtractString(),
        "rgb(227, 227, 227)");
    EXPECT_EQ(EvalJs(web_contents,
                     "window.getComputedStyle(document.querySelector('body'))."
                     "backgroundColor")
                  .ExtractString(),
              "rgb(31, 31, 31)");
  } else {
    // Expect that the default offline page is showing with light mode colors.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "window.matchMedia('(prefers-color-scheme: light)').matches")
            .ExtractBool());
    EXPECT_EQ(
        EvalJs(web_contents,
               "window.getComputedStyle(document.querySelector('div')).color")
            .ExtractString(),
        "rgb(31, 31, 31)");
    EXPECT_EQ(EvalJs(web_contents,
                     "window.getComputedStyle(document.querySelector('body'))."
                     "backgroundColor")
                  .ExtractString(),
              "rgb(255, 255, 255)");
  }
}

// Testing offline page in dark mode for a web app with a manifest that has not
// provided dark mode colors.
// TODO(crbug.com/1373750): tests are flaky on Lacros and Linux.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_WebAppOfflineNoDarkModeColorsProvided \
  DISABLED_WebAppOfflineNoDarkModeColorsProvided
#else
#define MAYBE_WebAppOfflineNoDarkModeColorsProvided \
  WebAppOfflineNoDarkModeColorsProvided
#endif
IN_PROC_BROWSER_TEST_P(WebAppOfflineDarkModeTest,
                       MAYBE_WebAppOfflineNoDarkModeColorsProvided) {
#if BUILDFLAG(IS_WIN)
  if (GetParam() == blink::mojom::PreferredColorScheme::kLight &&
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()) {
    GTEST_SKIP() << "Host is in dark mode; skipping test";
  }
#endif  // BUILDFLAG(IS_WIN)

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  if (GetParam() == blink::mojom::PreferredColorScheme::kDark) {
    // Expect that the default offline page is showing with dark mode colors.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "window.matchMedia('(prefers-color-scheme: dark)').matches")
            .ExtractBool());
    EXPECT_EQ(
        EvalJs(web_contents,
               "window.getComputedStyle(document.querySelector('div')).color")
            .ExtractString(),
        "rgb(227, 227, 227)");
    EXPECT_EQ(EvalJs(web_contents,
                     "window.getComputedStyle(document.querySelector('body'))."
                     "backgroundColor")
                  .ExtractString(),
              "rgb(31, 31, 31)");
  } else {
    // Expect that the default offline page is showing with light mode colors.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "window.matchMedia('(prefers-color-scheme: light)').matches")
            .ExtractBool());
    EXPECT_EQ(
        EvalJs(web_contents,
               "window.getComputedStyle(document.querySelector('div')).color")
            .ExtractString(),
        "rgb(31, 31, 31)");
    EXPECT_EQ(EvalJs(web_contents,
                     "window.getComputedStyle(document.querySelector('body'))."
                     "backgroundColor")
                  .ExtractString(),
              "rgb(255, 255, 255)");
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebAppOfflineDarkModeTest,
    ::testing::Values(blink::mojom::PreferredColorScheme::kDark,
                      blink::mojom::PreferredColorScheme::kLight));
}  // namespace web_app
