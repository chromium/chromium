// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/app_browser_controller.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/native_theme.h"

namespace web_app {

using AppBrowserControllerBrowserTest = WebAppBrowserTestBase;

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest,
                       HighContrastThemeColor) {
  const AppBrowserController* const controller =
      web_app::LaunchWebAppBrowser(
          profile(), InstallPWA(GURL("https://app.site.test/example/index")))
          ->app_controller();

  // Enable high contrast theme.
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  native_theme->set_forced_colors(true);
  EXPECT_EQ(controller->GetThemeColor(),
            native_theme->GetSystemThemeColor(
                ui::NativeTheme::SystemThemeColor::kWindow));
}
#endif

}  // namespace web_app
