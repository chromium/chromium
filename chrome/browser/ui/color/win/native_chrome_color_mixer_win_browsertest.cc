// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/color/color_provider.h"
#include "ui/color/win/accent_color_observer.h"
#include "ui/native_theme/native_theme.h"

class NativeChromeColorMixerWinBrowsertest : public InProcessBrowserTest {
 public:
  NativeChromeColorMixerWinBrowsertest() = default;

 protected:
  void SetFollowDevice(bool follow_device) {
    ThemeServiceFactory::GetForProfile(browser()->profile())
        ->UseDeviceTheme(follow_device);
  }
};

// Tests that windows header colors track the accent color when configured to
// use dwm frame colors.
IN_PROC_BROWSER_TEST_F(NativeChromeColorMixerWinBrowsertest,
                       HeaderColorsFollowAccentColor) {
  // Ensure the accent color starts unset and we are not following device
  // colors.
  auto* accent_color_observer = ui::AccentColorObserver::Get();
  accent_color_observer->SetAccentColorForTesting(std::nullopt);
  accent_color_observer->SetUseDwmFrameColorForTesting(false);
  SetFollowDevice(false);

  auto get_header_color = [&]() {
    return browser()->window()->GetColorProvider()->GetColor(
        ui::kColorSysHeader);
  };

  // Query the window header color.
  const auto initial_header_color = get_header_color();

  // Configure the observer to follow dwm colors.
  constexpr SkColor kAccentColor = SK_ColorMAGENTA;
  accent_color_observer->SetAccentColorForTesting(kAccentColor);
  accent_color_observer->SetUseDwmFrameColorForTesting(true);

  // The header color should be unaffected as the theme service has not been set
  // to follow the device theme.
  EXPECT_EQ(initial_header_color, get_header_color());

  // Set the browser to follow the device colors. The header color should be
  // updated to track the accent color.
  SetFollowDevice(true);
  EXPECT_EQ(kAccentColor, get_header_color());

  // Unset the accent color. The header color should revert to its initial value
  // even though the browser is still configured to follow the device theme.
  accent_color_observer->SetAccentColorForTesting(std::nullopt);
  accent_color_observer->SetUseDwmFrameColorForTesting(false);
  EXPECT_EQ(initial_header_color, get_header_color());
}

IN_PROC_BROWSER_TEST_F(NativeChromeColorMixerWinBrowsertest,
                       NativeThemeForWebWithAccentColor) {
  // Configure the observer with accent color for testing.
  auto* accent_color_observer = ui::AccentColorObserver::Get();
  constexpr SkColor kAccentColor = SkColorSetRGB(135, 115, 10);
  accent_color_observer->SetAccentColorForTesting(kAccentColor);

  EXPECT_EQ(kAccentColor, ui::NativeTheme::GetInstanceForWeb()->user_color());
}
