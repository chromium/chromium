// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/embedder_support/switches.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/native_theme/native_theme.h"

namespace web_app {

class WebAppDarkModeBrowserTest : public WebAppControllerBrowserTest {
 public:
  WebAppDarkModeBrowserTest() {
    features_.InitAndEnableFeature(blink::features::kWebAppEnableDarkMode);
  }

  WebAppDarkModeBrowserTest(const WebAppDarkModeBrowserTest&) = delete;
  WebAppDarkModeBrowserTest& operator=(const WebAppDarkModeBrowserTest&) =
      delete;
  ~WebAppDarkModeBrowserTest() override = default;

  AppId InstallWebAppFromInfo() {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    // We want to hang so WebContents does not update the background color.
    web_app_info->start_url = https_server()->GetURL("/hung");
    web_app_info->title = u"A Web App";
    web_app_info->display_mode = DisplayMode::kStandalone;
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    web_app_info->theme_color = SK_ColorBLUE;
    web_app_info->background_color = SK_ColorBLUE;
    web_app_info->dark_mode_theme_color = SK_ColorRED;
    web_app_info->dark_mode_background_color = SK_ColorRED;
    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

  AppId InstallWebAppFromPath(const char* path, bool await_metric = true) {
    GURL start_url = https_server()->GetURL(path);
    page_load_metrics::PageLoadMetricsTestWaiter metrics_waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (await_metric)
      metrics_waiter.AddWebFeatureExpectation(
          blink::mojom::WebFeature::kWebAppManifestUserPreferences);

    AppId app_id = InstallWebAppFromPage(browser(), start_url);
    if (await_metric)
      metrics_waiter.Wait();

    return app_id;
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(WebAppDarkModeBrowserTest, DarkColors) {
  AppId app_id = InstallWebAppFromInfo();

  WebAppBrowserController* controller;
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  controller = app_browser->app_controller()->AsWebAppBrowserController();

  ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(false);
  EXPECT_EQ(controller->GetThemeColor().value(), SK_ColorBLUE);
  EXPECT_EQ(controller->GetBackgroundColor().value(), SK_ColorBLUE);

  ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(true);
  EXPECT_EQ(controller->GetThemeColor().value(), SK_ColorRED);
  EXPECT_EQ(controller->GetBackgroundColor().value(), SK_ColorRED);
}

IN_PROC_BROWSER_TEST_F(WebAppDarkModeBrowserTest, ColorSchemeDarkSet) {
  AppId app_id = InstallWebAppFromPath(
      "/web_apps/get_manifest.html?color_scheme_dark.json");

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestUserPreferences, 1);

  EXPECT_EQ(provider().registrar_unsafe().GetAppThemeColor(app_id).value(),
            SK_ColorBLUE);
  EXPECT_EQ(provider().registrar_unsafe().GetAppBackgroundColor(app_id).value(),
            SK_ColorBLUE);

  EXPECT_EQ(
      provider().registrar_unsafe().GetAppDarkModeThemeColor(app_id).value(),
      SK_ColorRED);
  EXPECT_EQ(provider()
                .registrar_unsafe()
                .GetAppDarkModeBackgroundColor(app_id)
                .value(),
            SK_ColorRED);
}

IN_PROC_BROWSER_TEST_F(WebAppDarkModeBrowserTest, NoUserPreferences) {
  AppId app_id =
      InstallWebAppFromPath("/web_apps/basic.html", /*await_metric=*/false);

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestUserPreferences, 0);
}

class WebAppDarkModeOriginTrialBrowserTest : public InProcessBrowserTest {
 public:
  WebAppDarkModeOriginTrialBrowserTest() {
    feature_list_.InitAndDisableFeature(blink::features::kWebAppEnableDarkMode);
  }
  ~WebAppDarkModeOriginTrialBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Using the test public key from docs/origin_trials_integration.md#Testing.
    command_line->AppendSwitchASCII(
        embedder_support::kOriginTrialPublicKey,
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=");
  }
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(browser()->profile()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
};
namespace {

// InstallableManager requires https or localhost to load the manifest. Go with
// localhost to avoid having to set up cert servers.
constexpr char kTestWebAppUrl[] = "http://127.0.0.1:8000/";
constexpr char kTestWebAppHeaders[] =
    "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n";
constexpr char kTestWebAppBody[] = R"(
  <!DOCTYPE html>
  <head>
    <link rel="manifest" href="manifest.webmanifest">
    <meta http-equiv="origin-trial" content="$1">
  </head>
)";

constexpr char kTestIconUrl[] = "http://127.0.0.1:8000/icon.png";
constexpr char kTestManifestUrl[] =
    "http://127.0.0.1:8000/manifest.webmanifest";
constexpr char kTestManifestHeaders[] =
    "HTTP/1.1 200 OK\nContent-Type: application/json; charset=utf-8\n";
constexpr char kTestManifestBody[] = R"({
  "name": "Test app",
  "display": "standalone",
  "start_url": "/",
  "scope": "/",
  "icons": [{
    "src": "icon.png",
    "sizes": "192x192",
    "type": "image/png"
  }],
  "theme_color": "#0000FF",
  "background_color": "#0000FF",
  "theme_colors":
    [{"color": "#FF0000", "media": "(prefers-color-scheme: dark) "}],
  "background_colors":
    [{"color": "#FF0000", "media": "(prefers-color-scheme: dark) "}]
})";

// Generated from script:
// $ tools/origin_trials/generate_token.py http://127.0.0.1:8000
// "WebAppDarkModeV2" --expire-timestamp=2000000000
constexpr char kOriginTrialToken[] =
    "A5O53Hwkh/37AxAgFN9SgIEMr4QMDuI+vdiwHK5Y1sRbupzDwml5TUobj4smxm21Rk8RyjU/"
    "geQ68fYc05rZ7AwAAABYeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYX"
    "R1cmUiOiAiV2ViQXBwRGFya01vZGVWMiIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==";

}  // namespace

IN_PROC_BROWSER_TEST_F(WebAppDarkModeOriginTrialBrowserTest, OriginTrial) {
  ManifestUpdateManager::BypassWindowCloseWaitingForTesting() = true;

  bool serve_token = true;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&serve_token](
          content::URLLoaderInterceptor::RequestParams* params) -> bool {
        if (params->url_request.url.spec() == kTestWebAppUrl) {
          content::URLLoaderInterceptor::WriteResponse(
              kTestWebAppHeaders,
              base::ReplaceStringPlaceholders(
                  kTestWebAppBody, {serve_token ? kOriginTrialToken : ""},
                  nullptr),
              params->client.get());
          return true;
        }
        if (params->url_request.url.spec() == kTestManifestUrl) {
          content::URLLoaderInterceptor::WriteResponse(
              kTestManifestHeaders, kTestManifestBody, params->client.get());
          return true;
        }
        if (params->url_request.url.spec() == kTestIconUrl) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/basic-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  // Install web app with origin trial token.
  AppId app_id =
      web_app::InstallWebAppFromPage(browser(), GURL(kTestWebAppUrl));

  // Origin trial should grant the app access.
  WebAppProvider& provider = *WebAppProvider::GetForTest(browser()->profile());
  EXPECT_EQ(
      provider.registrar_unsafe().GetAppById(app_id)->dark_mode_theme_color(),
      SK_ColorRED);
  EXPECT_EQ(provider.registrar_unsafe()
                .GetAppById(app_id)
                ->dark_mode_background_color(),
            SK_ColorRED);

  // Open the page again with the token missing.
  {
    UpdateAwaiter update_awaiter(provider.install_manager());

    serve_token = false;
    NavigateToURLAndWait(browser(), GURL(kTestWebAppUrl));

    update_awaiter.AwaitUpdate();
  }

  // The app should update to no longer have dark mode colors defined without
  // the origin trial.
  EXPECT_EQ(
      provider.registrar_unsafe().GetAppById(app_id)->dark_mode_theme_color(),
      absl::nullopt);
  EXPECT_EQ(provider.registrar_unsafe()
                .GetAppById(app_id)
                ->dark_mode_background_color(),
            absl::nullopt);
}

}  // namespace web_app
