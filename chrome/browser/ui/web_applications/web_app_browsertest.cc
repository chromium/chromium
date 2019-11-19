// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

constexpr const char kExampleURL[] = "http://example.org/";

}  // namespace

namespace web_app {

class WebAppBrowserTest : public WebAppControllerBrowserTest {};

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, CreatedForInstalledPwaForPwa) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = GURL(kExampleURL);
  web_app_info->scope = GURL(kExampleURL);
  AppId app_id = InstallWebApp(std::move(web_app_info));
  Browser* app_browser = LaunchWebAppBrowser(app_id);

  EXPECT_TRUE(app_browser->app_controller()->CreatedForInstalledPwa());
}

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, ThemeColor) {
  {
    const SkColor theme_color = SkColorSetA(SK_ColorBLUE, 0xF0);
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = GURL(kExampleURL);
    web_app_info->scope = GURL(kExampleURL);
    web_app_info->theme_color = theme_color;
    AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* app_browser = LaunchWebAppBrowser(app_id);

    EXPECT_EQ(GetAppIdFromApplicationName(app_browser->app_name()), app_id);
    EXPECT_EQ(SkColorSetA(theme_color, SK_AlphaOPAQUE),
              app_browser->app_controller()->GetThemeColor());
  }
  {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = GURL("http://example.org/2");
    web_app_info->scope = GURL("http://example.org/");
    web_app_info->theme_color = base::Optional<SkColor>();
    AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* app_browser = LaunchWebAppBrowser(app_id);

    EXPECT_EQ(GetAppIdFromApplicationName(app_browser->app_name()), app_id);
    EXPECT_EQ(base::nullopt, app_browser->app_controller()->GetThemeColor());
  }
}

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest, HasMinimalUiButtons) {
  int index = 0;
  auto has_buttons = [this, &index](DisplayMode display_mode,
                                    bool open_as_window) -> bool {
    const std::string base_url = "https://example.com/path";
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = GURL(base_url + base::NumberToString(index++));
    web_app_info->scope = web_app_info->app_url;
    web_app_info->display_mode = display_mode;
    web_app_info->open_as_window = open_as_window;
    AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* app_browser = LaunchWebAppBrowser(app_id);
    return app_browser->app_controller()->HasMinimalUiButtons();
  };

  EXPECT_TRUE(has_buttons(DisplayMode::kBrowser,
                          /*open_as_window=*/true));
  EXPECT_TRUE(has_buttons(DisplayMode::kMinimalUi,
                          /*open_as_window=*/true));
  EXPECT_FALSE(has_buttons(DisplayMode::kStandalone,
                           /*open_as_window=*/true));
  EXPECT_FALSE(has_buttons(DisplayMode::kFullscreen,
                           /*open_as_window=*/true));

  EXPECT_FALSE(has_buttons(DisplayMode::kMinimalUi,
                           /*open_as_window=*/false));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebAppBrowserTest,
    ::testing::Values(ControllerType::kHostedAppController,
                      ControllerType::kUnifiedControllerWithBookmarkApp,
                      ControllerType::kUnifiedControllerWithWebApp),
    ControllerTypeParamToString);

}  // namespace web_app
