// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
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
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/native_theme/test_native_theme.h"
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

// Runs browser color provider tests with ChromeRefresh2023 enabled and
// disabled.
class BrowserFrameColorProviderTest : public BrowserFrameTest,
                                      public testing::WithParamInterface<bool> {
 public:
  static constexpr SkColor kLightColor = SK_ColorWHITE;
  static constexpr SkColor kDarkColor = SK_ColorBLACK;
  static constexpr SkColor kGrayColor = SK_ColorGRAY;
  static constexpr SkColor kTransparentColor = SK_ColorTRANSPARENT;

  BrowserFrameColorProviderTest() {
    feature_list_.InitWithFeatureState(features::kChromeRefresh2023,
                                       GetParam());
  }

  // BrowserFrameTest:
  void SetUpOnMainThread() override {
    BrowserFrameTest::SetUpOnMainThread();

    test_native_theme_.SetDarkMode(false);
    // TODO(tluk): BrowserFrame may update the NativeTheme when a theme update
    // event is received, which may unset the test NativeTheme. There should be
    // a way to prevent updates resetting the test NativeTheme when set.
    GetBrowserFrame(browser())->SetNativeThemeForTest(&test_native_theme_);

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

    // Used to track the light/dark color mode setting.
    mixer[ui::kColorSysPrimary] = {
        key.color_mode == ui::ColorProviderManager::ColorMode::kDark
            ? kDarkColor
            : kLightColor};

    // Used to track the user color.
    mixer[ui::kColorSysSecondary] = {
        key.user_color.value_or(kTransparentColor)};

    // Used to track is_grayscale.
    mixer[ui::kColorSysTertiary] = {key.is_grayscale ? kGrayColor
                                                     : kTransparentColor};
  }

  // Sets the `kBrowserColorScheme` pref for the `profile`.
  void SetBrowserColorScheme(Profile* profile,
                             ThemeService::BrowserColorScheme color_scheme) {
    GetThemeService(profile)->SetBrowserColorScheme(color_scheme);
  }

  // Sets the `kUserColor` pref for the `profile`.
  void SetUserColor(Profile* profile, absl::optional<SkColor> user_color) {
    GetThemeService(profile)->SetUserColor(user_color);
  }

  // Sets the `kGrayscaleThemeEnabled` pref for the `profile`.
  void SetIsGrayscale(Profile* profile, bool is_grayscale) {
    GetThemeService(profile)->SetIsGrayscale(is_grayscale);
  }

  BrowserFrame* GetBrowserFrame(Browser* browser) {
    return static_cast<BrowserFrame*>(
        BrowserView::GetBrowserViewForBrowser(browser)->GetWidget());
  }

  Profile* profile() { return browser()->profile(); }

  ThemeService* GetThemeService(Profile* profile) {
    return ThemeServiceFactory::GetForProfile(profile);
  }

  ui::TestNativeTheme test_native_theme_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies the BrowserFrame honors the BrowserColorScheme pref.
IN_PROC_BROWSER_TEST_P(BrowserFrameColorProviderTest,
                       TracksBrowserColorScheme) {
  // Assert the browser follows the system color scheme (i.e. the color scheme
  // set on the associated native theme)
  views::Widget* browser_frame = GetBrowserFrame(browser());
  test_native_theme_.SetDarkMode(false);
  EXPECT_EQ(kLightColor,
            browser_frame->GetColorProvider()->GetColor(ui::kColorSysPrimary));

  test_native_theme_.SetDarkMode(true);
  EXPECT_EQ(kDarkColor,
            browser_frame->GetColorProvider()->GetColor(ui::kColorSysPrimary));

  // Set the BrowserColorScheme pref. The BrowserFrame should ignore the system
  // color scheme if running ChromeRefresh2023. Otherwise BrowserFrame should
  // track the system color scheme.
  test_native_theme_.SetDarkMode(false);
  SetBrowserColorScheme(profile(), ThemeService::BrowserColorScheme::kDark);
  browser_frame->SetNativeThemeForTest(&test_native_theme_);
  if (features::IsChromeRefresh2023()) {
    EXPECT_EQ(kDarkColor, browser_frame->GetColorProvider()->GetColor(
                              ui::kColorSysPrimary));
  } else {
    EXPECT_EQ(kLightColor, browser_frame->GetColorProvider()->GetColor(
                               ui::kColorSysPrimary));
  }

  test_native_theme_.SetDarkMode(true);
  SetBrowserColorScheme(profile(), ThemeService::BrowserColorScheme::kLight);
  browser_frame->SetNativeThemeForTest(&test_native_theme_);
  if (features::IsChromeRefresh2023()) {
    EXPECT_EQ(kLightColor, browser_frame->GetColorProvider()->GetColor(
                               ui::kColorSysPrimary));
  } else {
    EXPECT_EQ(kDarkColor, browser_frame->GetColorProvider()->GetColor(
                              ui::kColorSysPrimary));
  }
}

// Verifies incognito browsers will always use the dark ColorMode.
IN_PROC_BROWSER_TEST_P(BrowserFrameColorProviderTest, IncognitoAlwaysDarkMode) {
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  views::Widget* incognito_browser_frame = GetBrowserFrame(incognito_browser);
  incognito_browser_frame->SetNativeThemeForTest(&test_native_theme_);

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

// Verifies the BrowserFrame's user_color tracks the autogenerated theme color.
IN_PROC_BROWSER_TEST_P(BrowserFrameColorProviderTest,
                       UserColorTracksAutogeneratedThemeColor) {
  // The Browser should initially have its user_color unset, tracking the user
  // color of its NativeTheme.
  views::Widget* browser_frame = GetBrowserFrame(browser());
  EXPECT_EQ(kTransparentColor, browser_frame->GetColorProvider()->GetColor(
                                   ui::kColorSysSecondary));

  // Install an autogenerated them and verify that the browser's user_color has
  // been updated to reflect.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile());
  constexpr SkColor kAutogeneratedColor1 = SkColorSetRGB(100, 100, 100);
  theme_service->BuildAutogeneratedThemeFromColor(kAutogeneratedColor1);
  EXPECT_EQ(kAutogeneratedColor1, theme_service->GetAutogeneratedThemeColor());
  EXPECT_EQ(kAutogeneratedColor1, browser_frame->GetColorProvider()->GetColor(
                                      ui::kColorSysSecondary));

  // Install a new autogenerated theme and verify that the user_color has been
  // updated to reflect.
  constexpr SkColor kAutogeneratedColor2 = SkColorSetRGB(200, 200, 200);
  theme_service->BuildAutogeneratedThemeFromColor(kAutogeneratedColor2);
  EXPECT_EQ(kAutogeneratedColor2, theme_service->GetAutogeneratedThemeColor());
  EXPECT_EQ(kAutogeneratedColor2, browser_frame->GetColorProvider()->GetColor(
                                      ui::kColorSysSecondary));
}

// Verifies BrowserFrame tracks the profile kUserColor pref correctly.
IN_PROC_BROWSER_TEST_P(BrowserFrameColorProviderTest,
                       UserColorProfilePrefTrackedCorrectly) {
  // The Browser should initially have its user_color unset, tracking the user
  // color of its NativeTheme.
  views::Widget* browser_frame = GetBrowserFrame(browser());
  EXPECT_EQ(kTransparentColor, browser_frame->GetColorProvider()->GetColor(
                                   ui::kColorSysSecondary));

  // Set the kUserColor pref. This should be reflected in the generated colors.
  constexpr SkColor kUserColor = SkColorSetRGB(100, 100, 100);
  SetUserColor(profile(), kUserColor);
  EXPECT_EQ(kUserColor, browser_frame->GetColorProvider()->GetColor(
                            ui::kColorSysSecondary));

  // Install an autogenerated theme and verify that the browser's user_color now
  // tracks this instead of the kUserColor pref.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile());
  constexpr SkColor kAutogeneratedColor = SkColorSetRGB(150, 150, 150);
  theme_service->BuildAutogeneratedThemeFromColor(kAutogeneratedColor);
  EXPECT_EQ(kAutogeneratedColor, theme_service->GetAutogeneratedThemeColor());
  EXPECT_EQ(kAutogeneratedColor, browser_frame->GetColorProvider()->GetColor(
                                     ui::kColorSysSecondary));

  // Reset the autogenerated theme and verify the kUserColor pref is again
  // tracked by the browser.
  theme_service->UseDefaultTheme();
  EXPECT_EQ(kTransparentColor, theme_service->GetAutogeneratedThemeColor());
  EXPECT_EQ(kUserColor, browser_frame->GetColorProvider()->GetColor(
                            ui::kColorSysSecondary));
}

// Verifies incognito browsers will ignore the user_color set on their
// NativeTheme.
IN_PROC_BROWSER_TEST_P(BrowserFrameColorProviderTest,
                       IncognitoAlwaysIgnoresUserColor) {
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  views::Widget* incognito_browser_frame = GetBrowserFrame(incognito_browser);
  incognito_browser_frame->SetNativeThemeForTest(&test_native_theme_);

  // Set the user color override on both the NativeTheme and the profile pref.
  test_native_theme_.set_user_color(SK_ColorBLUE);
  SetUserColor(incognito_browser->profile(), SK_ColorGREEN);
  incognito_browser_frame->ThemeChanged();

  // The ingognito browser should unset the user color.
  EXPECT_EQ(kTransparentColor,
            incognito_browser_frame->GetColorProvider()->GetColor(
                ui::kColorSysSecondary));
}

// Verifies the BrowserFrame's user_color tracks the is_grayscale theme pref.
IN_PROC_BROWSER_TEST_P(BrowserFrameColorProviderTest,
                       BrowserFrameTracksIsGrayscale) {
  // Set the is_grayscale pref to true. The browser should honor this pref.
  views::Widget* browser_frame = GetBrowserFrame(browser());
  SetIsGrayscale(profile(), true);
  EXPECT_EQ(kGrayColor,
            browser_frame->GetColorProvider()->GetColor(ui::kColorSysTertiary));

  // Set the is_grayscale pref to false. The browser should revert to ignoring
  // the grayscale setting.
  SetIsGrayscale(profile(), false);
  EXPECT_EQ(kTransparentColor,
            browser_frame->GetColorProvider()->GetColor(ui::kColorSysTertiary));
}

// Verifies incognito browsers will ignore the is_grayscale setting of the
// ThemeService.
IN_PROC_BROWSER_TEST_P(BrowserFrameColorProviderTest,
                       IncognitoAlwaysIgnoresIsGrayscale) {
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  views::Widget* incognito_browser_frame = GetBrowserFrame(incognito_browser);

  // Set the is_grayscale pref to false. The ingognito browser should ignore the
  // is_grayscale setting.
  SetIsGrayscale(incognito_browser->profile(), false);
  EXPECT_EQ(kTransparentColor,
            incognito_browser_frame->GetColorProvider()->GetColor(
                ui::kColorSysTertiary));

  // Set the is_grayscale pref to true. The ingognito browser should ignore the
  // is_grayscale setting.
  SetIsGrayscale(incognito_browser->profile(), true);
  EXPECT_EQ(kTransparentColor,
            incognito_browser_frame->GetColorProvider()->GetColor(
                ui::kColorSysTertiary));
}

INSTANTIATE_TEST_SUITE_P(All, BrowserFrameColorProviderTest, testing::Bool());
