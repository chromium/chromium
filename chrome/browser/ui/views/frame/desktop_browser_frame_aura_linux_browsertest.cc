// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"

#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/ozone/public/ozone_platform.h"

using DesktopBrowserFrameAuraLinuxTest = InProcessBrowserTest;
using SupportsForTest =
    ui::OzonePlatform::PlatformRuntimeProperties::SupportsForTest;

namespace {

gfx::Size GetWindowSize(Browser* browser) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  const NativeBrowserFrame* const frame =
      browser_view->frame()->native_browser_frame();
  gfx::Rect bounds;
  ui::mojom::WindowShowState show_state;
  frame->GetWindowPlacement(&bounds, &show_state);
  return bounds.size();
}

void VerifyColorsForFrameType(const Browser* browser, bool use_custom_frame) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser->profile());
  EXPECT_EQ(use_custom_frame, theme_service->ShouldUseCustomFrame());

  ui::ColorProviderManager::ResetForTesting();

  bool initialized_color_provider_for_custom_frame;
  ui::ColorProviderManager::GetForTesting().AppendColorProviderInitializer(
      base::BindLambdaForTesting(
          [&initialized_color_provider_for_custom_frame](
              ui::ColorProvider* provider, const ui::ColorProviderKey& key) {
            initialized_color_provider_for_custom_frame =
                key.frame_type == ui::ColorProviderKey::FrameType::kChromium;
          }));
  ASSERT_NE(nullptr,
            BrowserView::GetBrowserViewForBrowser(browser)->GetColorProvider());
  EXPECT_EQ(use_custom_frame, initialized_color_provider_for_custom_frame);

  ui::ColorProviderManager::ResetForTesting();
}

}  // namespace

// Tests that DesktopBrowserFrameAuraLinux::UseCustomFrame() returns the correct
// value that respects 1) the current value of the user preference and
// 2) capabilities of the platform.
// Also tests the regressions found in crbug.com/1243937 and crbug.com/1329756.
IN_PROC_BROWSER_TEST_F(DesktopBrowserFrameAuraLinuxTest, UseCustomFrame) {
  const BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  const DesktopBrowserFrameAuraLinux* const frame =
      static_cast<DesktopBrowserFrameAuraLinux*>(
          browser_view->frame()->native_browser_frame());
  auto* pref_service = browser_view->browser()->profile()->GetPrefs();

  // Try overriding the runtime platform property that indicates whether the
  // platform supports server-side window decorations.  For each variant,
  // check what is the effective value of the property (the platform may
  // ignore the override), then pass through variants for the user preference
  // and check what DesktopBrowserFrameAuraLinux::UseCustomFrame() returns
  // finally.
  auto* const platform = ui::OzonePlatform::GetInstance();
  for (const auto ssd_support_override :
       {SupportsForTest::kYes, SupportsForTest::kNo}) {
    ui::OzonePlatform::PlatformRuntimeProperties::
        override_supports_ssd_for_test = ssd_support_override;

    if (platform->GetPlatformRuntimeProperties()
            .supports_server_side_window_decorations) {
      // This platform either supports overriding the property or supports SSD
      // always.
      // UseCustomFrame() should return what the user preference suggests.
      for (const auto setting : {true, false}) {
        pref_service->SetBoolean(prefs::kUseCustomChromeFrame, setting);
        EXPECT_EQ(frame->UseCustomFrame(), setting)
            << " when setting is " << setting;
        VerifyColorsForFrameType(browser(), frame->UseCustomFrame());
      }
    } else {
      // This platform either does not support overriding the property or does
      // not support SSD.
      // UseCustomFrame() should return true always.
      for (const auto setting : {true, false}) {
        pref_service->SetBoolean(prefs::kUseCustomChromeFrame, setting);
        EXPECT_TRUE(frame->UseCustomFrame()) << " when setting is " << setting;
        VerifyColorsForFrameType(browser(), frame->UseCustomFrame());
      }
    }
  }

  // Reset the override.
  ui::OzonePlatform::PlatformRuntimeProperties::override_supports_ssd_for_test =
      SupportsForTest::kNotSet;
}

// Tests that the new browser window restores the bounds properly: its size must
// be the same as the already existing window has.
// The regression was found in https://crbug.com/1287212.
IN_PROC_BROWSER_TEST_F(DesktopBrowserFrameAuraLinuxTest, NewWindowSize) {
  Profile* profile = browser()->profile();
  Browser::CreateParams params(profile, true /* user_gesture */);
  Browser* browser2 = Browser::Create(params);
  browser2->window()->Show();

  // The first window saves its placement on losing the active state, then the
  // second window needs to go through the initialisation, update its size and
  // frame extents.  We wait until everything calms down.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetWindowSize(browser()), GetWindowSize(browser2));
}
