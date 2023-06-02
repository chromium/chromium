// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame.h"

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/views/views_delegate.h"

class BrowserFrameBoundsChecker : public ChromeViewsDelegate {
 public:
  BrowserFrameBoundsChecker() {}

  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override {
    ChromeViewsDelegate::OnBeforeWidgetInit(params, delegate);
    if (params->name == "BrowserFrame")
      EXPECT_FALSE(params->bounds.IsEmpty());
  }
};

class BrowserFrameTest : public InProcessBrowserTest {
 public:
  BrowserFrameTest()
      : InProcessBrowserTest(std::make_unique<BrowserFrameBoundsChecker>()) {}
};

// Verifies that the tools are loaded with initial bounds.
IN_PROC_BROWSER_TEST_F(BrowserFrameTest, DevToolsHasBoundsOnOpen) {
  // Open undocked tools.
  DevToolsWindow* devtools_ =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_);
}

// Verifies that the web app is loaded with initial bounds.
IN_PROC_BROWSER_TEST_F(BrowserFrameTest, WebAppsHasBoundsOnOpen) {
  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = GURL("http://example.org/");
  web_app::AppId app_id = web_app::test::InstallWebApp(browser()->profile(),
                                                       std::move(web_app_info));

  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_TRUE(app_browser->is_type_app());
  app_browser->window()->Close();
}

class BrowserFrameColorModeTest : public BrowserFrameTest {
 public:
  static constexpr SkColor kLightColor = SK_ColorWHITE;
  static constexpr SkColor kDarkColor = SK_ColorBLACK;

  // BrowserFrameTest:
  void SetUpOnMainThread() override {
    BrowserFrameTest::SetUpOnMainThread();

    // Force a light / dark color to be returned for `kColorSysPrimary`
    // depending on the ColorMode.
    ui::ColorProviderManager::ResetForTesting();
    ui::ColorProviderManager::GetForTesting().AppendColorProviderInitializer(
        base::BindRepeating(&AddColor));

    // Set the default browser pref to follow system color mode.
    profile()->GetPrefs()->SetInteger(
        prefs::kBrowserColorScheme,
        static_cast<int>(ThemeService::BrowserColorScheme::kSystem));
  }

 protected:
  static void AddColor(ui::ColorProvider* provider,
                       const ui::ColorProviderManager::Key& key) {
    // Add a postprocessing mixer to ensure it is appended to the end of the
    // pipeline.
    ui::ColorMixer& mixer = provider->AddPostprocessingMixer();
    mixer[ui::kColorSysPrimary] = {
        key.color_mode == ui::ColorProviderManager::ColorMode::kDark
            ? kDarkColor
            : kLightColor};
  }

  // Sets the `kBrowserColorScheme` pref for the `profile`.
  void SetBrowserColorScheme(Profile* profile,
                             ThemeService::BrowserColorScheme color_scheme) {
    profile->GetPrefs()->SetInteger(prefs::kBrowserColorScheme,
                                    static_cast<int>(color_scheme));
  }

  Profile* profile() { return browser()->profile(); }
};

// Verifies the BrowserFrame honors the BrowserColorScheme pref.
IN_PROC_BROWSER_TEST_F(BrowserFrameColorModeTest, TracksBrowserColorScheme) {
  // Assert the browser follows the system color mode. Simulate the system color
  // mode by setting the widget level color mode override.
  views::Widget* browser_frame =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  browser_frame->SetColorModeOverride(
      ui::ColorProviderManager::ColorMode::kLight);
  EXPECT_EQ(kLightColor,
            browser_frame->GetColorProvider()->GetColor(ui::kColorSysPrimary));

  browser_frame->SetColorModeOverride(
      ui::ColorProviderManager::ColorMode::kDark);
  EXPECT_EQ(kDarkColor,
            browser_frame->GetColorProvider()->GetColor(ui::kColorSysPrimary));

  // Set the BrowserColorScheme pref. The BrowserFrame should ignore the system
  // color mode.
  browser_frame->SetColorModeOverride(
      ui::ColorProviderManager::ColorMode::kLight);
  SetBrowserColorScheme(profile(), ThemeService::BrowserColorScheme::kDark);
  EXPECT_EQ(kDarkColor,
            browser_frame->GetColorProvider()->GetColor(ui::kColorSysPrimary));

  browser_frame->SetColorModeOverride(
      ui::ColorProviderManager::ColorMode::kDark);
  SetBrowserColorScheme(profile(), ThemeService::BrowserColorScheme::kLight);
  EXPECT_EQ(kLightColor,
            browser_frame->GetColorProvider()->GetColor(ui::kColorSysPrimary));
}

// Verifies incognito browsers will always use the dark ColorMode.
IN_PROC_BROWSER_TEST_F(BrowserFrameColorModeTest, IncognitoAlwaysDarkMode) {
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  views::Widget* incognito_browser_frame =
      BrowserView::GetBrowserViewForBrowser(incognito_browser)->GetWidget();

  // The incognito browser should reflect the dark color mode irrespective of
  // the current BrowserColorScheme.
  SetBrowserColorScheme(incognito_browser->profile(),
                        ThemeService::BrowserColorScheme::kLight);
  EXPECT_EQ(kDarkColor, incognito_browser_frame->GetColorProvider()->GetColor(
                            ui::kColorSysPrimary));

  SetBrowserColorScheme(incognito_browser->profile(),
                        ThemeService::BrowserColorScheme::kDark);
  EXPECT_EQ(kDarkColor, incognito_browser_frame->GetColorProvider()->GetColor(
                            ui::kColorSysPrimary));
}
