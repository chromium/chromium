// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "content/public/test/browser_test.h"
#include "ui/native_theme/native_theme.h"

namespace web_app {

class AppBrowserControllerBrowserTest : public WebAppControllerBrowserTest {
 public:
  AppBrowserControllerBrowserTest() = default;
  ~AppBrowserControllerBrowserTest() override = default;
};

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest,
                       HighContrastThemeColor) {
  const GURL start_url("https://app.site.test/example/index");
  const AppId app_id = InstallPWA(start_url);

  Browser* browser = web_app::LaunchWebAppBrowser(profile(), app_id);

  AppBrowserController* controller = browser->app_controller();
  // Verify theme color is null in browsertests before enabling high contrast.
  ASSERT_FALSE(controller->GetThemeColor().has_value());

  // Enable high contrast theme.
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  // Make sure high contrast is disabled initially.
  EXPECT_FALSE(native_theme->InForcedColorsMode());
  native_theme->set_forced_colors(true);

  EXPECT_TRUE(native_theme->InForcedColorsMode());

  EXPECT_TRUE(controller->GetThemeColor().has_value());
  absl::optional<SkColor> hc_theme_color = native_theme->GetSystemThemeColor(
      ui::NativeTheme::SystemThemeColor::kWindow);
  EXPECT_EQ(*controller->GetThemeColor(), hc_theme_color);
}
#endif

}  // namespace web_app
