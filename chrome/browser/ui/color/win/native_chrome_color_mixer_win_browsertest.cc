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
#include "ui/native_theme/mock_os_settings_provider.h"

using NativeChromeColorMixerWinBrowserTest = InProcessBrowserTest;

namespace {
constexpr SkColor kAccentColor = SK_ColorMAGENTA;
}  // namespace

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
  const auto get_header_color = [&] {
    return browser()->window()->GetColorProvider()->GetColor(
        ui::kColorSysHeader);
  };
  const SkColor initial_header_color = get_header_color();

  // Configure the observer to use a specific accent color. The header color
  // should be unaffected as the theme service has not been set to follow the
  // device theme.
  accent_color_observer->SetAccentColorForTesting(kAccentColor);
  accent_color_observer->SetShouldUseAccentColorForWindowFrameForTesting(true);
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

IN_PROC_BROWSER_TEST_F(NativeChromeColorMixerWinBrowserTest,
                       AccentColorAvailableWithFrameColorDisabled) {
  // Test that accent colors are present in Chrome UI even when DWM frame colors
  // are disabled, but not used for titlebar/frame colors.
  ui::MockOsSettingsProvider mock_provider;
  mock_provider.SetAccentColor(kAccentColor);

  auto* const accent_color_observer = ui::AccentColorObserver::Get();
  // Disable DWM frame colors (simulates registry with ColorPrevalence=0)
  accent_color_observer->SetShouldUseAccentColorForWindowFrameForTesting(false);

  auto* const theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  theme_service->UseDeviceTheme(true);

  // Accent color should be available for internal Chrome UI and web content
  // with DWM frame colors disabled.
  EXPECT_EQ(kAccentColor,
            ui::NativeTheme::GetInstanceForNativeUi()->user_color());
  EXPECT_EQ(kAccentColor, ui::NativeTheme::GetInstanceForWeb()->user_color());

  const auto get_header_color = [&] {
    return browser()->window()->GetColorProvider()->GetColor(
        ui::kColorSysHeader);
  };
  const SkColor header_color_without_prevalence = get_header_color();
  accent_color_observer->SetShouldUseAccentColorForWindowFrameForTesting(true);
  const SkColor header_color_with_prevalence = get_header_color();

  // When DWM frame colors are disabled, the header color uses the default
  // value. When enabled, the header color is computed from the accent color.
  // The two header colors should be different.
  EXPECT_NE(header_color_without_prevalence, header_color_with_prevalence);
}
