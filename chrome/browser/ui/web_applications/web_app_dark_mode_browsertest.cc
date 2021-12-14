// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
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
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    // We want to hang so WebContents does not update the background color.
    web_app_info->start_url = https_server()->GetURL("/hung");
    web_app_info->title = u"A Web App";
    web_app_info->display_mode = DisplayMode::kStandalone;
    web_app_info->user_display_mode = DisplayMode::kStandalone;
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

  EXPECT_EQ(provider().registrar().GetAppThemeColor(app_id).value(),
            SK_ColorBLUE);
  EXPECT_EQ(provider().registrar().GetAppBackgroundColor(app_id).value(),
            SK_ColorBLUE);

  EXPECT_EQ(provider().registrar().GetAppDarkModeThemeColor(app_id).value(),
            SK_ColorRED);
  EXPECT_EQ(
      provider().registrar().GetAppDarkModeBackgroundColor(app_id).value(),
      SK_ColorRED);
}

IN_PROC_BROWSER_TEST_F(WebAppDarkModeBrowserTest, NoUserPreferences) {
  AppId app_id =
      InstallWebAppFromPath("/web_apps/basic.html", /*await_metric=*/false);

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestUserPreferences, 0);
}

}  // namespace web_app
