// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/native_theme/native_theme.h"

namespace web_app {

class WebAppDarkModeBrowserTest : public WebAppBrowserTestBase {
 public:
  webapps::AppId InstallWebAppFromInfo() {
    // We want to hang so WebContents does not update the background color.
    GURL start_url = https_server()->GetURL("/hung");
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->title = u"A Web App";
    web_app_info->display_mode = DisplayMode::kStandalone;
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    web_app_info->theme_color = SK_ColorBLUE;
    web_app_info->background_color = SK_ColorBLUE;
    web_app_info->dark_mode_theme_color = SK_ColorRED;
    web_app_info->dark_mode_background_color = SK_ColorRED;
    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }
};

IN_PROC_BROWSER_TEST_F(WebAppDarkModeBrowserTest, DarkColors) {
  webapps::AppId app_id = InstallWebAppFromInfo();

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

}  // namespace web_app
