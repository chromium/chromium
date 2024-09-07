// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame.h"

#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
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
#include "ui/native_theme/test_native_theme.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/views_delegate.h"

namespace {

ui::mojom::BrowserColorVariant GetColorVariant(
    ui::ColorProviderKey::SchemeVariant scheme_variant) {
  using BCV = ui::mojom::BrowserColorVariant;
  using SV = ui::ColorProviderKey::SchemeVariant;
  static constexpr auto kColorMap = base::MakeFixedFlatMap<SV, BCV>({
      {SV::kTonalSpot, BCV::kTonalSpot},
      {SV::kNeutral, BCV::kNeutral},
      {SV::kVibrant, BCV::kVibrant},
      {SV::kExpressive, BCV::kExpressive},
  });
  return kColorMap.at(scheme_variant);
}

}  // namespace

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

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
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
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("http://example.org/"));
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
IN_PROC_BROWSER_TEST_F(BrowserFrameTest, ChildWidgetsReceiveThemeUpdates) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // Create a child popup Widget for the BrowserFrame.
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

  // Propagate a browser theme change notification to the root BrowserFrame
  // widget and ensure the child widget is forwarded the theme change
  // notification.
  EXPECT_CALL(widget_child_observer, OnWidgetThemeChanged(testing::_)).Times(1);
  static_cast<BrowserFrame*>(browser_view->GetWidget())
      ->UserChangedTheme(BrowserThemeChangeType::kBrowserTheme);
}

// Regression test for crbug.com/1476462. Ensures that browser theme change
// notifications are always propagated correctly by the BrowserFrame with a
// default frame type.
IN_PROC_BROWSER_TEST_F(BrowserFrameTest,
                       ReceivesBrowserThemeUpdatesForDefaultFrames) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserFrame* browser_frame =
      static_cast<BrowserFrame*>(browser_view->GetWidget());

  // Enure the BrowserFrame is set to the default (non-system) frame.
  browser_frame->set_frame_type(views::Widget::FrameType::kDefault);

  // Propagate a browser theme change notification to the root BrowserFrame
  // widget and ensure ThemeChanged() is called.
  MockThemeObserver widget_observer(browser_frame);
  EXPECT_CALL(widget_observer, OnWidgetThemeChanged(testing::_)).Times(1);
  browser_frame->UserChangedTheme(BrowserThemeChangeType::kBrowserTheme);
}

class BrowserFrameColorProviderTest : public BrowserFrameTest {
 public:
  BrowserFrameColorProviderTest() = default;

  // BrowserFrameTest:
  void SetUpOnMainThread() override {
    BrowserFrameTest::SetUpOnMainThread();

    test_native_theme_.SetDarkMode(false);
    // TODO(tluk): BrowserFrame may update the NativeTheme when a theme update
    // event is received, which may unset the test NativeTheme. There should be
    // a way to prevent updates resetting the test NativeTheme when set.
    GetBrowserFrame(browser())->SetNativeThemeForTest(&test_native_theme_);

    // Set the default browser pref to follow system color mode.
    profile()->GetPrefs()->SetInteger(
        GetThemePrefNameInMigration(ThemePrefInMigration::kBrowserColorScheme),
        static_cast<int>(ThemeService::BrowserColorScheme::kSystem));
  }

 protected:
  ui::ColorProviderKey GetColorProviderKey(Browser* browser) {
    return GetBrowserFrame(browser)->GetColorProviderKeyForTesting();
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

  BrowserFrame* GetBrowserFrame(Browser* browser) {
    return static_cast<BrowserFrame*>(
        BrowserView::GetBrowserViewForBrowser(browser)->GetWidget());
  }

  Profile* profile() { return browser()->profile(); }

  ThemeService* GetThemeService(Profile* profile) {
    return ThemeServiceFactory::GetForProfile(profile);
  }

  ui::TestNativeTheme test_native_theme_;
};

// Verifies the BrowserFrame honors the BrowserColorScheme pref.
IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest,
                       TracksBrowserColorScheme) {
  SetFollowDevice(profile(), false);

  // Assert the browser follows the system color scheme (i.e. the color scheme
  // set on the associated native theme)
  views::Widget* browser_frame = GetBrowserFrame(browser());
  test_native_theme_.SetDarkMode(false);
  EXPECT_EQ(ui::ColorProviderKey::ColorMode::kLight,
            GetColorProviderKey(browser()).color_mode);

  test_native_theme_.SetDarkMode(true);
  EXPECT_EQ(ui::ColorProviderKey::ColorMode::kDark,
            GetColorProviderKey(browser()).color_mode);

  // Set the BrowserColorScheme pref. The BrowserFrame should ignore the system
  // color scheme.
  test_native_theme_.SetDarkMode(false);
  SetBrowserColorScheme(profile(), ThemeService::BrowserColorScheme::kDark);
  browser_frame->SetNativeThemeForTest(&test_native_theme_);
  EXPECT_EQ(ui::ColorProviderKey::ColorMode::kDark,
            GetColorProviderKey(browser()).color_mode);

  test_native_theme_.SetDarkMode(true);
  SetBrowserColorScheme(profile(), ThemeService::BrowserColorScheme::kLight);
  browser_frame->SetNativeThemeForTest(&test_native_theme_);
  EXPECT_EQ(ui::ColorProviderKey::ColorMode::kLight,
            GetColorProviderKey(browser()).color_mode);
}

// Verifies incognito browsers will always use the dark ColorMode.
IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest, IncognitoAlwaysDarkMode) {
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  views::Widget* incognito_browser_frame = GetBrowserFrame(incognito_browser);
  incognito_browser_frame->SetNativeThemeForTest(&test_native_theme_);

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

// Verifies the BrowserFrame's user_color tracks the autogenerated theme color.
IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest,
                       UserColorTracksAutogeneratedThemeColor) {
#if BUILDFLAG(IS_CHROMEOS)
  // Follow Device will override Autogenerated theme if allowed (so set it to
  // false). It is true by default on ChromeOS.
  SetFollowDevice(profile(), false);
#endif

  // The Browser should initially have its user_color unset, tracking the user
  // color of its NativeTheme.
  EXPECT_FALSE(GetColorProviderKey(browser()).user_color.has_value());
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kBaseline,
            GetColorProviderKey(browser()).user_color_source);

  // Install an autogenerated them and verify that the browser's user_color has
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

// Verifies BrowserFrame tracks the profile kUserColor pref correctly.
IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest,
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

// Verifies incognito browsers will ignore the user_color set on their
// NativeTheme and are configured to source colors from the grayscale palette.
IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest,
                       IncognitoAlwaysIgnoresUserColor) {
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  views::Widget* incognito_browser_frame = GetBrowserFrame(incognito_browser);
  incognito_browser_frame->SetNativeThemeForTest(&test_native_theme_);

  // Set the user color override on both the NativeTheme and the profile pref.
  test_native_theme_.set_user_color(SK_ColorBLUE);
  SetUserColor(incognito_browser->profile(), SK_ColorGREEN);
  incognito_browser_frame->ThemeChanged();

  // The ingognito browser should always set the user_color_source to grayscale.
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kGrayscale,
            GetColorProviderKey(incognito_browser).user_color_source);
}

// Verifies the BrowserFrame's user_color tracks the is_grayscale theme pref.
IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest,
                       BrowserFrameTracksIsGrayscale) {
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

IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest,
                       GrayscaleIgnoresUserColor) {
  SetFollowDevice(profile(), false);

  // Set native theme to an obviously different color.
  test_native_theme_.set_user_color(SK_ColorMAGENTA);
  test_native_theme_.set_scheme_variant(
      ui::ColorProviderKey::SchemeVariant::kVibrant);

  views::Widget* browser_frame = GetBrowserFrame(browser());
  browser_frame->SetNativeThemeForTest(&test_native_theme_);
  SetIsGrayscale(profile(), true);

  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kGrayscale,
            GetColorProviderKey(browser()).user_color_source);
}

// Verifies incognito browsers always force the grayscale palette.
IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest,
                       IncognitoIsAlwaysGrayscale) {
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());

  // Set the is_grayscale pref to false. The ingognito browser should force the
  // is_grayscale setting to true.
  SetIsGrayscale(incognito_browser->profile(), false);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kGrayscale,
            GetColorProviderKey(incognito_browser).user_color_source);

  // Set the is_grayscale pref to true. The ingognito browser should continue to
  // force the is_grayscale setting to true.
  SetIsGrayscale(incognito_browser->profile(), true);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kGrayscale,
            GetColorProviderKey(incognito_browser).user_color_source);
}

// Verifies the BrowserFrame's ColorProviderKey tracks the kBrowserColorVariant
// pref.
IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest,
                       BrowserFrameTracksBrowserColorVariant) {
  SetFollowDevice(profile(), false);
  using BCV = ui::mojom::BrowserColorVariant;

  // Set the scheme_variant pref to kSystem. The browser should honor this pref.
  views::Widget* browser_frame = GetBrowserFrame(browser());
  SetBrowserColorVariant(profile(), BCV::kSystem);
  browser_frame->GetNativeTheme()->set_scheme_variant(std::nullopt);
  EXPECT_FALSE(GetColorProviderKey(browser()).scheme_variant.has_value());

  // The browser should honor the browser overrides of the scheme variant pref
  // when set.
  for (BCV color_variant :
       {BCV::kTonalSpot, BCV::kNeutral, BCV::kVibrant, BCV::kExpressive}) {
    SetBrowserColorVariant(profile(), color_variant);
    ASSERT_TRUE(GetColorProviderKey(browser()).scheme_variant.has_value());
    EXPECT_EQ(
        color_variant,
        GetColorVariant(GetColorProviderKey(browser()).scheme_variant.value()));
  }
}

// Verifies the BrowserFrame's ColorProviderKey tracks the
// `NativeTheme::user_color` on ChromeOS and the color from `ThemeService` on
// platforms where the toggle is not available.
IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest, UseDeviceIgnoresTheme) {
  const SkColor native_theme_color = SK_ColorMAGENTA;
  const SkColor theme_service_color = SK_ColorGREEN;

  views::Widget* browser_frame = GetBrowserFrame(browser());
  // Set native theme to an obviously different color.
  ui::NativeTheme* native_theme = browser_frame->GetNativeTheme();
  native_theme->set_user_color(native_theme_color);
  native_theme->set_scheme_variant(
      ui::ColorProviderKey::SchemeVariant::kVibrant);

  // Set the color in `ThemeService`.
  SetUserColor(profile(), theme_service_color);
  // Prefer color from NativeTheme.
  SetFollowDevice(profile(), true);

  // Platforms that do not support the follow device pref instead use the user
  // color.
  EXPECT_EQ((BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)) ? native_theme_color
                                                          : theme_service_color,
            GetColorProviderKey(browser()).user_color);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kAccent,
            GetColorProviderKey(browser()).user_color_source);
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
// Verifies the BrowserFrame's ColorProviderKey tracks device even if
// AutogeneratedTheme is used.
IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest,
                       UseDeviceIgnoresAutogeneratedTheme) {
  constexpr SkColor kNativeThemeColor = SK_ColorMAGENTA;

  views::Widget* browser_frame = GetBrowserFrame(browser());
  ui::NativeTheme* native_theme = browser_frame->GetNativeTheme();
  native_theme->set_user_color(kNativeThemeColor);
  native_theme->set_scheme_variant(
      ui::ColorProviderKey::SchemeVariant::kVibrant);

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
IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest,
                       UseDeviceThemeIgnoresGrayscale) {
  views::Widget* browser_frame = GetBrowserFrame(browser());
  // Set native theme to an obviously different color.
  ui::NativeTheme* native_theme = browser_frame->GetNativeTheme();
  native_theme->set_user_color(SK_ColorMAGENTA);
  native_theme->set_scheme_variant(
      ui::ColorProviderKey::SchemeVariant::kVibrant);

  SetIsGrayscale(profile(), true);
  // Prefer color from NativeTheme.
  SetFollowDevice(profile(), true);

  EXPECT_EQ(SK_ColorMAGENTA, GetColorProviderKey(browser()).user_color);
  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kAccent,
            GetColorProviderKey(browser()).user_color_source);
}
#endif

IN_PROC_BROWSER_TEST_F(BrowserFrameColorProviderTest,
                       BaselineThemeIgnoresNativeThemeColor) {
  views::Widget* browser_frame = GetBrowserFrame(browser());
  // Set native theme to an obviously different color.
  ui::NativeTheme* native_theme = browser_frame->GetNativeTheme();
  native_theme->set_user_color(SK_ColorMAGENTA);
  native_theme->set_scheme_variant(
      ui::ColorProviderKey::SchemeVariant::kVibrant);

  // Set the color in `ThemeService` to nullopt to indicate the Baseline theme.
  SetUserColor(profile(), std::nullopt);
  // Prevent follow pref from overriding theme.
  SetFollowDevice(profile(), false);

  EXPECT_EQ(ui::ColorProviderKey::UserColorSource::kBaseline,
            GetColorProviderKey(browser()).user_color_source);
}
