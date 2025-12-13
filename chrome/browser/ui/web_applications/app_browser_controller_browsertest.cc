// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/app_browser_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/os_settings_provider.h"

namespace web_app {

using AppBrowserControllerBrowserTest = WebAppBrowserTestBase;

IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest,
                       HighContrastThemeColor) {
  const AppBrowserController* const controller =
      AppBrowserController::From(web_app::LaunchWebAppBrowser(
          profile(), InstallPWA(GURL("https://app.site.test/example/index"))));

  // Enable high contrast theme.
  static constexpr SkColor kWindowColor = SK_ColorBLUE;
  ui::MockOsSettingsProvider os_settings_provider;
  os_settings_provider.SetColor(ui::OsSettingsProvider::ColorId::kWindow,
                                kWindowColor);
  os_settings_provider.SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kMore);
  EXPECT_EQ(controller->GetThemeColor(), kWindowColor);
}

}  // namespace web_app
