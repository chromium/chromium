// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/metrics/crc32.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_icon_waiter.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/webapps/browser/test/service_worker_registration_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
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
  webapps::AppId StartWebAppAndDisconnect(content::WebContents* web_contents,
                                          std::string_view relative_url) {
    GURL target_url(embedded_test_server()->GetURL(relative_url));
    web_app::NavigateViaLinkClickToURLAndWait(browser(), target_url);
    webapps::AppId app_id = web_app::test::InstallPwaForCurrentUrl(browser());
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
                             std::string_view relative_url) {
    GURL target_url(embedded_test_server()->GetURL(relative_url));
    web_app::ServiceWorkerRegistrationWaiter registration_waiter(
        browser()->profile(), target_url);
    web_app::NavigateViaLinkClickToURLAndWait(browser(), target_url);
    registration_waiter.AwaitRegistration();
    webapps::AppId app_id = web_app::test::InstallPwaForCurrentUrl(browser());
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
    Browser* app_browser = chrome::FindBrowserWithTab(web_contents);
    app_browser->window()->Close();
    ui_test_utils::WaitForBrowserToClose(app_browser);
  }

 private:
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
};

class WebAppOfflinePageTest : public WebAppOfflineTest {
 public:
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
  base::HistogramTester histogram_tester_;
};

// When a web app with a manifest and no service worker is offline it should
// display the default offline page rather than the dino.
// When the exact same conditions are applied with the feature flag disabled
// expect that the default offline page is not shown.
IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest, WebAppOfflinePageIsDisplayed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
  // Expect that the default offline page is showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') !== null")
                  .ExtractBool());
}

// When a web app with a manifest and service worker that doesn't handle being
// offline it should display the default offline page rather than the dino.
IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest,
                       WebAppOfflineWithEmptyServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartPwaAndDisconnect(web_contents, "/banners/background-color.html");

  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
  // Expect that the default offline page is showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') !== null")
                  .ExtractBool());
}

// When a web app with a manifest and service worker that handles being offline
// it should not display the default offline page.
IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest, WebAppOfflineWithServiceWorker) {
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
IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest, WebAppOfflinePageIconShowing) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  StartWebAppAndDisconnect(web_contents,
                           "/banners/no_sw_fetch_handler_test_page.html");
  WaitForLoadStop(web_contents);

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
  EXPECT_EQ("You're offline",
            EvalJs(web_contents,
                   "document.getElementById('default-web-app-msg').textContent")
                .ExtractString());
  EXPECT_EQ("Manifest test app",
            EvalJs(web_contents, "document.title").ExtractString());
  EXPECT_EQ("inline",
            EvalJs(web_contents,
                   "document.getElementById('offlineIcon').style.display")
                .ExtractString());

  std::string actual_url =
      EvalJs(web_contents, "document.getElementById('icon').src")
          .ExtractString();
  constexpr std::string_view kDataUrlPrefix = "data:image/png;base64,";
  ASSERT_THAT(actual_url, testing::StartsWith(kDataUrlPrefix));
  std::string_view base64 =
      std::string_view(actual_url).substr(kDataUrlPrefix.size());
  std::optional<std::vector<uint8_t>> png_bytes = base::Base64Decode(base64);
  ASSERT_TRUE(png_bytes.has_value());
  SkBitmap bitmap = gfx::PNGCodec::Decode(*png_bytes);
  ASSERT_FALSE(bitmap.isNull());
  EXPECT_EQ(bitmap.width(), 160);
  EXPECT_EQ(bitmap.height(), 160);
  // SAFETY: `bitmap.isNull()` has been checked above.  span's data and size
  // come from the same container.
  base::span<const uint8_t> image_bytes = UNSAFE_BUFFERS(
      base::span(static_cast<const uint8_t*>(bitmap.pixmap().addr()),
                 bitmap.computeByteSize()));
  EXPECT_EQ(4172094509u, base::Crc32(0, image_bytes));
}

IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest, WebAppOfflineMetricsNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 0);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 0);

  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
  // Expect that the default offline page is showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') !== null")
                  .ExtractBool());

  // Navigate somewhere else (anywhere else but the current page will do).
  EXPECT_TRUE(NavigateToURL(web_contents, GURL("about:blank")));

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 1);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 1);
  EXPECT_THAT(histogram()->GetAllSamples(kHistogramClosingReason),
              ElementsAre(base::Bucket(/* min= */ 1, /* count= */ 1)));
}

IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest, WebAppOfflineMetricsBackOnline) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 0);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 0);

  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
  // Expect that the default offline page is showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') !== null")
                  .ExtractBool());

  // The URL interceptor only blocks the first navigation. This one should
  // go through.
  ReloadWebContents(web_contents);

  // Expect that the default offline page is not showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') === null")
                  .ExtractBool());

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 1);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 1);
  EXPECT_THAT(histogram()->GetAllSamples(kHistogramClosingReason),
              ElementsAre(base::Bucket(/* min= */ 0, /* count= */ 1)));
}

IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest, WebAppOfflineMetricsPwaClosing) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  webapps::AppId app_id =
      StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 0);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 0);

  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
  // Expect that the default offline page is showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') !== null")
                  .ExtractBool());

  CloseBrowser(web_contents);

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 1);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 1);
  EXPECT_THAT(histogram()->GetAllSamples(kHistogramClosingReason),
              ElementsAre(base::Bucket(/* min= */ 2, /* count= */ 1)));
}

class WebAppOfflineDarkModeTest
    : public WebAppOfflineTest,
      public testing::WithParamInterface<blink::mojom::PreferredColorScheme> {
 public:
  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    InProcessBrowserTest::SetUp();
#elif BUILDFLAG(IS_MAC)
    // TODO(crbug.com/40215627): Get this test suite working.
    GTEST_SKIP();
#else
    InProcessBrowserTest::SetUp();
#endif  // BUILDFLAG(IS_MAC)
  }

  void SetUpOnMainThread() override {
    WebAppOfflineTest::SetUpOnMainThread();
#if BUILDFLAG(IS_CHROMEOS)
    // Explicitly set dark mode in ChromeOS or we can't get light mode after
    // sunset (due to dark mode auto-scheduling).
    ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(
        GetParam() == blink::mojom::PreferredColorScheme::kDark);
#endif
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // ShellContentBrowserClient::OverrideWebPreferences() overrides the
    // prefers-color-scheme according to switches::kForceDarkMode
    // command line.
    if (GetParam() == blink::mojom::PreferredColorScheme::kDark)
      command_line->AppendSwitch(switches::kForceDarkMode);
  }
};

// Testing offline page in dark mode for a web app with a manifest and no
// service worker.
// TODO(crbug.com/40871921): tests are flaky on Linux.
#if BUILDFLAG(IS_LINUX)
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

// Testing offline page in dark mode for a web app with a manifest and service
// worker that does not handle offline error.
// TODO(crbug.com/40871921): tests are flaky on Linux.
#if BUILDFLAG(IS_LINUX)
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
  StartPwaAndDisconnect(web_contents,
                        "/banners/manifest_test_page_empty_fetch_handler.html");
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
