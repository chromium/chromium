// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_widget.h"

#include "base/scoped_observation.h"
#include "base/test/bind.h"
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
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/views_delegate.h"

namespace {

ui::mojom::BrowserColorVariant GetColorVariant(
    std::optional<ui::ColorProviderKey::SchemeVariant> scheme_variant) {
  using BCV = ui::mojom::BrowserColorVariant;
  if (!scheme_variant.has_value()) {
    return BCV::kSystem;
  }

  using SV = ui::ColorProviderKey::SchemeVariant;
  static constexpr auto kColorMap =
      base::MakeFixedFlatMap<SV, BCV>({{SV::kTonalSpot, BCV::kTonalSpot},
                                       {SV::kNeutral, BCV::kNeutral},
                                       {SV::kVibrant, BCV::kVibrant},
                                       {SV::kExpressive, BCV::kExpressive}});
  return kColorMap.at(scheme_variant.value());
}

}  // namespace

class BrowserWidgetBoundsChecker : public ChromeViewsDelegate {
 public:
  BrowserWidgetBoundsChecker() = default;

  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override {
    ChromeViewsDelegate::OnBeforeWidgetInit(params, delegate);
    if (params->name == "BrowserWidget") {
      EXPECT_FALSE(params->bounds.IsEmpty());
    }
  }
};

class BrowserWidgetTest : public InProcessBrowserTest {
 public:
  BrowserWidgetTest()
      : InProcessBrowserTest(std::make_unique<BrowserWidgetBoundsChecker>()) {}

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

// Verifies that the tools are loaded with initial bounds.
IN_PROC_BROWSER_TEST_F(BrowserWidgetTest, DevToolsHasBoundsOnOpen) {
  // Open undocked tools.
  DevToolsWindow* devtools_ =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_);
}

// Verifies that the web app is loaded with initial bounds.
IN_PROC_BROWSER_TEST_F(BrowserWidgetTest, WebAppsHasBoundsOnOpen) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.org/"));
  webapps::AppId app_id = web_app::test::InstallWebApp(browser()->profile(),
                                                       std::move(web_app_info));

  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_TRUE(app_browser->is_type_app());
  app_browser->window()->Close();
}

class MockThemeObserver : public views::WidgetObserver {
 public:
  explicit MockThemeObserver(views::Widget* widget) {
    widget_observation_.Observe(widget);
  }
  MOCK_METHOD(void, OnWidgetThemeChanged, (views::Widget*));

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

// Verifies that theme change notifications are propagated to child widgets for
// browser theme changes.
IN_PROC_BROWSER_TEST_F(BrowserWidgetTest, ChildWidgetsReceiveThemeUpdates) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // Create a child popup Widget for the BrowserWidget.
  const auto child_widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.shadow_elevation = 1;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.bounds = {0, 0, 200, 200};
  params.parent = browser_view->GetWidget()->GetNativeView();
  params.child = true;
  // TODO(https://crbug.com/329271186): Theme updates do not propagate to bubble
  // in a separate platform widget.
#if BUILDFLAG(IS_OZONE)
  params.use_accelerated_widget_override = false;
#endif
  child_widget->Init(std::move(params));

  // Add a bubble widget and set up the theme change observer.
  MockThemeObserver widget_child_observer(child_widget.get());

  // Propagate a browser theme change notification to the root BrowserWidget
  // widget and ensure the child widget is forwarded the theme change
  // notification.
  EXPECT_CALL(widget_child_observer, OnWidgetThemeChanged(testing::_)).Times(1);
  static_cast<BrowserWidget*>(browser_view->GetWidget())
      ->UserChangedTheme(BrowserThemeChangeType::kBrowserTheme);
}

// Regression test for crbug.com/1476462. Ensures that browser theme change
// notifications are always propagated correctly by the BrowserWidget with a
// default frame type.
IN_PROC_BROWSER_TEST_F(BrowserWidgetTest,
                       ReceivesBrowserThemeUpdatesForDefaultFrames) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserWidget* browser_widget =
      static_cast<BrowserWidget*>(browser_view->GetWidget());

  // Enure the BrowserWidget is set to the default (non-system) frame.
  browser_widget->set_frame_type(views::Widget::FrameType::kDefault);

  // Propagate a browser theme change notification to the root BrowserWidget
  // widget and ensure ThemeChanged() is called.
  MockThemeObserver widget_observer(browser_widget);
  EXPECT_CALL(widget_observer, OnWidgetThemeChanged(testing::_)).Times(1);
  browser_widget->UserChangedTheme(BrowserThemeChangeType::kBrowserTheme);
}

class BrowserWidgetColorProviderTest : public BrowserWidgetTest {
 public:
  BrowserWidgetColorProviderTest() = default;

  // BrowserWidgetTest:
  void SetUpOnMainThread() override {
    BrowserWidgetTest::SetUpOnMainThread();

    // Set the default browser pref to follow system color mode.
    profile()->GetPrefs()->SetInteger(
        prefs::kBrowserColorScheme,
        static_cast<int>(ThemeService::BrowserColorScheme::kSystem));
  }

 protected:
  ui::ColorProviderKey GetColorProviderKey(Browser* browser) {
    return GetBrowserWidget(browser)->GetColorProviderKeyForTesting();
  }

  // Sets the `kBrowserColorScheme` pref for the `profile`.
  void SetBrowserColorScheme(Profile* profile,
                             ThemeService::BrowserColorScheme color_scheme) {
    GetThemeService(profile)->SetBrowserColorScheme(color_scheme);
  }

  // Sets the `kUserColor` pref for the `profile`.
  void SetUserColor(Profile* profile, std::optional<SkColor> user_color) {
    GetThemeService(profile)->SetUserColor(user_color);
  }

  // Sets the `kGrayscaleThemeEnabled` pref for the `profile`.
  void SetIsGrayscale(Profile* profile, bool is_grayscale) {
    GetThemeService(profile)->SetIsGrayscale(is_grayscale);
  }

  // Sets the `kBrowserFollowsSystemThemeColors` pref for `profile`.
  void SetFollowDevice(Profile* profile, bool follow_device) {
    GetThemeService(profile)->UseDeviceTheme(follow_device);
  }

  // Sets the `kBrowserColorVariant` pref for the `profile`.
  void SetBrowserColorVariant(Profile* profile,
                              ui::mojom::BrowserColorVariant color_variant) {
    GetThemeService(profile)->SetBrowserColorVariant(color_variant);
  }

  BrowserWidget* GetBrowserWidget(Browser* browser) {
    return static_cast<BrowserWidget*>(
        BrowserView::GetBrowserViewForBrowser(browser)->GetWidget());
  }

  Profile* profile() { return browser()->profile(); }
  ui::MockOsSettingsProvider& os_settings_provider() {
    return os_settings_provider_;
  }

  ThemeService* GetThemeService(Profile* profile) {
    return ThemeServiceFactory::GetForProfile(profile);
  }

 private:
  ui::MockOsSettingsProvider os_settings_provider_;
};

// Verifies the BrowserWidget honors the BrowserColorScheme pref.
IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest,
                       TracksBrowserColorScheme) {
  SetFollowDevice(profile(), false);

  // Assert the browser follows the system color scheme.
  EXPECT_EQ(ui::ColorProviderKey::ColorMode::kLight,
            GetColorProviderKey(browser()).color_mode);

  os_settings_provider().SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kDark);
  EXPECT_EQ(ui::ColorProviderKey::ColorMode::kDark,
            GetColorProviderKey(browser()).color_mode);

  // Set the BrowserColorScheme pref. The BrowserWidget should ignore the system
  // color scheme.
  os_settings_provider().SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kLight);
  SetBrowserColorScheme(profile(), ThemeService::BrowserColorScheme::kDark);
  EXPECT_EQ(ui::ColorProviderKey::ColorMode::kDark,
            GetColorProviderKey(browser()).color_mode);

  os_settings_provider().SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kDark);
  SetBrowserColorScheme(profile(), ThemeService::BrowserColorScheme::kLight);
  EXPECT_EQ(ui::ColorProviderKey::ColorMode::kLight,
            GetColorProviderKey(browser()).color_mode);
}

// Verifies incognito browsers will always use the dark ColorMode.
IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest,
                       IncognitoAlwaysDarkMode) {
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());

  // The incognito browser should reflect the dark color mode irrespective of
  // the current BrowserColorScheme.
  SetBrowserColorScheme(incognito_browser->profile(),
                        ThemeService::BrowserColorScheme::kLight);
  EXPECT_EQ(ui::ColorProviderKey::ColorMode::kDark,
            GetColorProviderKey(incognito_browser).color_mode);

  SetBrowserColorScheme(incognito_browser->profile(),
                        ThemeService::BrowserColorScheme::kDark);
  EXPECT_EQ(ui::ColorProviderKey::ColorMode::kDark,
            GetColorProviderKey(incognito_browser).color_mode);
}

// Verifies the BrowserWidget's user_color tracks the autogenerated theme color.
IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest,
                       UserColorTracksAutogeneratedThemeColor) {
#if BUILDFLAG(IS_CHROMEOS)
  // Follow Device will override autogenerated theme if allowed (so set it to
  // false). It is true by default on ChromeOS.
  SetFollowDevice(profile(), false);
#endif

  // The Browser should initially have its user_color unset, tracking the user
  // color of its NativeTheme.
  EXPECT_FALSE(GetColorProviderKey(browser()).user_color.has_value());
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kBaseline,
            GetColorProviderKey(browser()).user_color_source);

  // Install an autogenerated theme and verify that the browser's user_color has
  // been updated to reflect.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile());
  constexpr SkColor kAutogeneratedColor1 = SkColorSetRGB(100, 100, 100);
  theme_service->BuildAutogeneratedThemeFromColor(kAutogeneratedColor1);
  EXPECT_EQ(kAutogeneratedColor1, theme_service->GetAutogeneratedThemeColor());
  EXPECT_EQ(kAutogeneratedColor1, GetColorProviderKey(browser()).user_color);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kAccent,
            GetColorProviderKey(browser()).user_color_source);

  // Install a new autogenerated theme and verify that the user_color has been
  // updated to reflect.
  constexpr SkColor kAutogeneratedColor2 = SkColorSetRGB(200, 200, 200);
  theme_service->BuildAutogeneratedThemeFromColor(kAutogeneratedColor2);
  EXPECT_EQ(kAutogeneratedColor2, theme_service->GetAutogeneratedThemeColor());
  EXPECT_EQ(kAutogeneratedColor2, GetColorProviderKey(browser()).user_color);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kAccent,
            GetColorProviderKey(browser()).user_color_source);
}

// Verifies BrowserWidget tracks the profile kUserColor pref correctly.
IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest,
                       UserColorProfilePrefTrackedCorrectly) {
  // The Browser should initially have its user_color unset.
  SetFollowDevice(profile(), false);
  EXPECT_FALSE(GetColorProviderKey(browser()).user_color.has_value());
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kBaseline,
            GetColorProviderKey(browser()).user_color_source);

  // Set the kUserColor pref. This should be reflected in the generated colors.
  constexpr SkColor kUserColor = SkColorSetRGB(100, 100, 100);
  SetUserColor(profile(), kUserColor);
  EXPECT_EQ(kUserColor, GetColorProviderKey(browser()).user_color);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kAccent,
            GetColorProviderKey(browser()).user_color_source);

  // Install an autogenerated theme and verify that the browser's user_color now
  // tracks this instead of the kUserColor pref.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile());
  constexpr SkColor kAutogeneratedColor = SkColorSetRGB(150, 150, 150);
  theme_service->BuildAutogeneratedThemeFromColor(kAutogeneratedColor);
  EXPECT_EQ(kAutogeneratedColor, theme_service->GetAutogeneratedThemeColor());
  EXPECT_EQ(kAutogeneratedColor, GetColorProviderKey(browser()).user_color);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kAccent,
            GetColorProviderKey(browser()).user_color_source);

  // Set kUserColor pref again and verify that the browser's user_color tracks
  // kUserColor pref again.
  SetUserColor(profile(), kUserColor);
  EXPECT_EQ(SK_ColorTRANSPARENT, theme_service->GetAutogeneratedThemeColor());
  EXPECT_EQ(kUserColor, theme_service->GetUserColor());
  EXPECT_EQ(kUserColor, GetColorProviderKey(browser()).user_color);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kAccent,
            GetColorProviderKey(browser()).user_color_source);
}

// Verifies incognito browsers will ignore any user color provided by the OS and
// source colors from the grayscale palette.
IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest,
                       IncognitoAlwaysIgnoresUserColor) {
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  views::Widget* incognito_browser_frame = GetBrowserWidget(incognito_browser);

  // Set the user color in both the OS and the profile pref.
  os_settings_provider().SetAccentColor(SK_ColorBLUE);
  SetUserColor(incognito_browser->profile(), SK_ColorGREEN);
  incognito_browser_frame->ThemeChanged();

  // The incognito browser should always set the user_color_source to grayscale.
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kGrayscale,
            GetColorProviderKey(incognito_browser).user_color_source);
}

// Verifies the BrowserWidget's user_color tracks the is_grayscale theme pref.
IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest,
                       BrowserWidgetTracksIsGrayscale) {
  SetFollowDevice(profile(), false);
  const auto initial_source = GetColorProviderKey(browser()).user_color_source;
  EXPECT_NE(ui::ColorProviderKey::UserColorSource::kGrayscale, initial_source);

  // Set the is_grayscale pref to true. The browser should honor this pref.
  SetIsGrayscale(profile(), true);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kGrayscale,
            GetColorProviderKey(browser()).user_color_source);

  // Set the is_grayscale pref to false. The browser should revert to ignoring
  // the grayscale setting.
  SetIsGrayscale(profile(), false);
  EXPECT_EQ(initial_source, GetColorProviderKey(browser()).user_color_source);
}

IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest,
                       GrayscaleIgnoresUserColor) {
  SetFollowDevice(profile(), false);

  // Set OS user color to an obviously different color.
  os_settings_provider().SetAccentColor(SK_ColorMAGENTA);

  SetIsGrayscale(profile(), true);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kGrayscale,
            GetColorProviderKey(browser()).user_color_source);
}

// Verifies incognito browsers always force the grayscale palette.
IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest,
                       IncognitoIsAlwaysGrayscale) {
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());

  // Set the is_grayscale pref to false. The incognito browser should force the
  // is_grayscale setting to true.
  SetIsGrayscale(incognito_browser->profile(), false);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kGrayscale,
            GetColorProviderKey(incognito_browser).user_color_source);

  // Set the is_grayscale pref to true. The incognito browser should continue to
  // force the is_grayscale setting to true.
  SetIsGrayscale(incognito_browser->profile(), true);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kGrayscale,
            GetColorProviderKey(incognito_browser).user_color_source);
}

// Verifies the BrowserWidget's ColorProviderKey tracks the kBrowserColorVariant
// pref.
IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest,
                       BrowserWidgetTracksBrowserColorVariant) {
  SetFollowDevice(profile(), false);
  using enum ui::mojom::BrowserColorVariant;

  // The browser should honor the scheme variant pref.
  for (const auto color_variant :
       {kSystem, kTonalSpot, kNeutral, kVibrant, kExpressive}) {
    SetBrowserColorVariant(profile(), color_variant);
    EXPECT_EQ(color_variant,
              GetColorVariant(GetColorProviderKey(browser()).scheme_variant));
  }
}

// Verifies the BrowserWidget's ColorProviderKey tracks the user color on
// ChromeOS and the color from `ThemeService` on platforms where the toggle is
// not available.
IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest, UseDeviceIgnoresTheme) {
  static constexpr SkColor kNativeThemeColor = SK_ColorMAGENTA;
  static constexpr SkColor kThemeServiceColor = SK_ColorGREEN;

  // Set OS user color to an obviously different color.
  os_settings_provider().SetAccentColor(kNativeThemeColor);

  // Set the color in `ThemeService`.
  SetUserColor(profile(), kThemeServiceColor);
  // Prefer color from NativeTheme.
  SetFollowDevice(profile(), true);

  // Platforms that do not support the follow device pref instead use the user
  // color.
  EXPECT_EQ((BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)) ? kNativeThemeColor
                                                          : kThemeServiceColor,
            GetColorProviderKey(browser()).user_color);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kAccent,
            GetColorProviderKey(browser()).user_color_source);
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
// Verifies the BrowserWidget's ColorProviderKey tracks device even if
// AutogeneratedTheme is used.
IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest,
                       UseDeviceIgnoresAutogeneratedTheme) {
  static constexpr SkColor kNativeThemeColor = SK_ColorMAGENTA;

  os_settings_provider().SetAccentColor(kNativeThemeColor);

  // Set `ThemeService` to use an autogenerated theme.
  auto* theme_service = GetThemeService(profile());
  constexpr SkColor kAutogeneratedColor1 = SkColorSetRGB(100, 100, 100);
  theme_service->BuildAutogeneratedThemeFromColor(kAutogeneratedColor1);
  ASSERT_EQ(kAutogeneratedColor1, theme_service->GetAutogeneratedThemeColor());
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kAccent,
            GetColorProviderKey(browser()).user_color_source);

  // Prefer color from NativeTheme.
  SetFollowDevice(profile(), true);

  // Device theme is preferred over autogenerated if device theme is true.
  EXPECT_EQ(kNativeThemeColor, GetColorProviderKey(browser()).user_color);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kAccent,
            GetColorProviderKey(browser()).user_color_source);
}

// Verify that that grayscale is ignored if UseDeviceTheme is true.
IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest,
                       UseDeviceThemeIgnoresGrayscale) {
  static constexpr SkColor kNativeThemeColor = SK_ColorMAGENTA;

  // Set OS user color to an obviously different color.
  os_settings_provider().SetAccentColor(kNativeThemeColor);

  SetIsGrayscale(profile(), true);
  // Prefer color from NativeTheme.
  SetFollowDevice(profile(), true);

  EXPECT_EQ(kNativeThemeColor, GetColorProviderKey(browser()).user_color);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kAccent,
            GetColorProviderKey(browser()).user_color_source);
}
#endif

IN_PROC_BROWSER_TEST_F(BrowserWidgetColorProviderTest,
                       BaselineThemeIgnoresNativeThemeColor) {
  // Set OS user color to an obviously different color.
  os_settings_provider().SetAccentColor(SK_ColorMAGENTA);

  // Set the color in `ThemeService` to nullopt to indicate the Baseline theme.
  SetUserColor(profile(), std::nullopt);
  // Prevent follow pref from overriding theme.
  SetFollowDevice(profile(), false);

  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kBaseline,
            GetColorProviderKey(browser()).user_color_source);
}
