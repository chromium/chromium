// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/win/accent_color_observer.h"

using NativeChromeColorMixerWinBrowserTest = InProcessBrowserTest;

// Tests that windows header colors track the accent color when configured to
// use DWM frame colors.
IN_PROC_BROWSER_TEST_F(NativeChromeColorMixerWinBrowserTest,
                       HeaderColorsFollowAccentColor) {
  // Get a baseline header color when the accent color observer is in a default
  // state and the theme service is not following the device theme.
  auto* const accent_color_observer = ui::AccentColorObserver::Get();
  accent_color_observer->SetAccentColorForTesting(std::nullopt);
  auto* const theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  theme_service->UseDeviceTheme(false);
  const auto get_header_color = [&]() {
    return browser()->window()->GetColorProvider()->GetColor(
        ui::kColorSysHeader);
  };
  const auto initial_header_color = get_header_color();

  // Configure the observer to use a specific accent color. The header color
  // should be unaffected as the theme service has not been set to follow the
  // device theme.
  constexpr SkColor kAccentColor = SK_ColorMAGENTA;
  accent_color_observer->SetAccentColorForTesting(kAccentColor);
  EXPECT_EQ(initial_header_color, get_header_color());

  // Configure the theme service to follow the device theme. The header color
  // should be updated to track the accent color set above.
  theme_service->UseDeviceTheme(true);
  EXPECT_EQ(kAccentColor, get_header_color());

  // Configure the observer to have no accent color. The header color should
  // revert to its initial value even though the theme service is still
  // following the device theme.
  accent_color_observer->SetAccentColorForTesting(std::nullopt);
  EXPECT_EQ(initial_header_color, get_header_color());
}
