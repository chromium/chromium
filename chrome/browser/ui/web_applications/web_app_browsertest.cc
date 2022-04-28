// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <codecvt>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/protocol/browser_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/scoped_disable_client_side_decorations_for_test.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/common/content_features.h"
#include "content/public/test/background_color_change_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "net/base/filename_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/test/test_reg_util_win.h"
#include "base/win/windows_version.h"
#include "chrome/browser/web_applications/os_integration/web_app_handler_registration_utils_win.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu_win.h"
#include "chrome/browser/win/jumplist_updater.h"
#include "chrome/installer/util/shell_util.h"
#endif

namespace {

constexpr const char kExampleURL[] = "http://example.org/";
constexpr const char16_t kExampleURL16[] = u"http://example.org/";
constexpr const char kExampleManifestURL[] = "http://example.org/manifest";

constexpr char kLaunchWebAppDisplayModeHistogram[] = "Launch.WebAppDisplayMode";

// Represents the variety of states that can exist and which control the page
// info bubble's app settings link.
enum class WebAppSettingsState {
  kNeitherEnabled,
  kOnlyWebAppSettingsEnabled,
  kFileHandlingAlsoEnabled,
};

// Opens |url| in a new popup window with the dimensions |popup_size|.
Browser* OpenPopupAndWait(Browser* browser,
                          const GURL& url,
                          const gfx::Size& popup_size) {
  content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::BrowserChangeObserver browser_change_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  std::string open_window_script = base::StringPrintf(
      "window.open('%s', '_blank', 'toolbar=none,width=%i,height=%i')",
      url.spec().c_str(), popup_size.width(), popup_size.height());

  EXPECT_TRUE(content::ExecJs(web_contents, open_window_script));

  // The navigation should happen in a new window.
  Browser* popup_browser = browser_change_observer.Wait();
  EXPECT_NE(browser, popup_browser);

  content::WebContents* popup_contents =
      popup_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(popup_contents));
  EXPECT_EQ(popup_contents->GetLastCommittedURL(), url);

  return popup_browser;
}

#if BUILDFLAG(IS_WIN)
std::vector<std::wstring> GetFileExtensionsForProgId(
    const std::wstring& file_handler_prog_id) {
  const std::wstring prog_id_path =
      base::StrCat({ShellUtil::kRegClasses, L"\\", file_handler_prog_id});

  // Get list of handled file extensions from value FileExtensions at
  // HKEY_CURRENT_USER\Software\Classes\<file_handler_prog_id>.
  base::win::RegKey file_extensions_key(HKEY_CURRENT_USER, prog_id_path.c_str(),
                                        KEY_QUERY_VALUE);
  std::wstring handled_file_extensions;
  EXPECT_EQ(file_extensions_key.ReadValue(L"FileExtensions",
                                          &handled_file_extensions),
            ERROR_SUCCESS);
  return base::SplitString(handled_file_extensions, std::wstring(L";"),
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

namespace web_app {

class WebAppBrowserTest : public WebAppControllerBrowserTest {
 public:
  GURL GetSecureAppURL() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  GURL GetURLForPath(const std::string& path) {
    return https_server()->GetURL("app.com", path);
  }

  bool HasMinimalUiButtons(DisplayMode display_mode,
                           absl::optional<DisplayMode> display_override_mode,
                           bool open_as_window) {
    static int index = 0;

    base::HistogramTester tester;
    const GURL app_url = https_server()->GetURL(
        base::StringPrintf("/web_apps/basic.html?index=%d", index++));
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url;
    web_app_info->display_mode = display_mode;
    web_app_info->user_display_mode = open_as_window
                                          ? UserDisplayMode::kStandalone
                                          : UserDisplayMode::kBrowser;
    if (display_override_mode)
      web_app_info->display_override.push_back(*display_override_mode);

    AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* app_browser = LaunchWebAppBrowser(app_id);
    DCHECK(app_browser->is_type_app());
    DCHECK(app_browser->app_controller());
    tester.ExpectUniqueSample(
        kLaunchWebAppDisplayModeHistogram,
        display_override_mode ? *display_override_mode : display_mode, 1);

    content::WebContents* const web_contents =
        app_browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(WaitForLoadStop(web_contents));
    EXPECT_EQ(app_url, web_contents->GetVisibleURL());

    bool matches;
    const bool result = app_browser->app_controller()->HasMinimalUiButtons();
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        web_contents,
        "window.domAutomationController.send(window.matchMedia('(display-mode: "
        "minimal-ui)').matches)",
        &matches));
    EXPECT_EQ(result, matches);
    CloseAndWait(app_browser);

    return result;
  }
};

// A dedicated test fixture for WindowControlsOverlay, which requires a command
// line switch to enable manifest parsing.
class WebAppBrowserTest_WindowControlsOverlay : public WebAppBrowserTest {
 public:
  WebAppBrowserTest_WindowControlsOverlay() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kWebAppWindowControlsOverlay};
};

// A dedicated test fixture for tabbed display override, which requires a
// command line switch to enable manifest parsing.
class WebAppBrowserTest_Tabbed : public WebAppBrowserTest {
 public:
  WebAppBrowserTest_Tabbed() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kDesktopPWAsTabStrip};
};

// TODO(crbug.com/1257751): Stabilize the test.
#if BUILDFLAG(IS_POSIX)
#define DISABLE_POSIX(TEST) DISABLED_##TEST
#else
#define DISABLE_POSIX(TEST) TEST
#endif

#if BUILDFLAG(IS_WIN)
using WebAppBrowserTest_ShortcutMenu = WebAppBrowserTest;
#endif

using WebAppTabRestoreBrowserTest = WebAppBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ThemeColor) {
  {
    const SkColor theme_color = SkColorSetA(SK_ColorBLUE, 0xF0);
    blink::mojom::Manifest manifest;
    manifest.start_url = GURL(kExampleURL);
    manifest.scope = GURL(kExampleURL);
    manifest.has_theme_color = true;
    manifest.theme_color = theme_color;
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app::UpdateWebAppInfoFromManifest(manifest, GURL(kExampleManifestURL),
                                          web_app_info.get());

    AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* app_browser = LaunchWebAppBrowser(app_id);

    EXPECT_EQ(GetAppIdFromApplicationName(app_browser->app_name()), app_id);
    EXPECT_EQ(SkColorSetA(theme_color, SK_AlphaOPAQUE),
              app_browser->app_controller()->GetThemeColor());
  }
  {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = GURL("http://example.org/2");
    web_app_info->scope = GURL("http://example.org/");
    web_app_info->theme_color = absl::optional<SkColor>();
    AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* app_browser = LaunchWebAppBrowser(app_id);

    EXPECT_EQ(GetAppIdFromApplicationName(app_browser->app_name()), app_id);
    EXPECT_EQ(absl::nullopt, app_browser->app_controller()->GetThemeColor());
  }
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, BackgroundColor) {
  blink::mojom::Manifest manifest;
  manifest.start_url = GURL(kExampleURL);
  manifest.scope = GURL(kExampleURL);
  manifest.has_background_color = true;
  manifest.background_color = SkColorSetA(SK_ColorBLUE, 0xF0);
  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app::UpdateWebAppInfoFromManifest(manifest, GURL(kExampleManifestURL),
                                        web_app_info.get());
  AppId app_id = InstallWebApp(std::move(web_app_info));

  auto* provider = WebAppProvider::GetForTest(profile());
  EXPECT_EQ(provider->registrar().GetAppBackgroundColor(app_id), SK_ColorBLUE);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ShortcutBackgroundColor) {
  const GURL app_url = https_server()->GetURL("/banners/background-color.html");
  const AppId app_id = InstallWebAppFromPage(browser(), app_url);
  auto* provider = WebAppProvider::GetForTest(profile());

  EXPECT_EQ(provider->registrar().GetAppBackgroundColor(app_id), SK_ColorBLUE);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ManifestWithColor) {
  const GURL app_url =
      https_server()->GetURL("/banners/no-sw-with-colors.html");
  const AppId app_id = InstallWebAppFromPage(browser(), app_url);
  auto* provider = WebAppProvider::GetForTest(profile());

  EXPECT_EQ(provider->registrar().GetAppBackgroundColor(app_id),
            SK_ColorYELLOW);
  EXPECT_EQ(provider->registrar().GetAppThemeColor(app_id), SK_ColorGREEN);
}

// Enumeration of test modes for `BackgroundColorChangeWebAppBrowserTest`s.
enum class BackgroundColorChangeTestMode {
  kSWA,
  kNonSWA,
};

// Base class for `BackgroundColorChange` tests, parameterized by test mode and
// whether to prefer manifest background color.
class BackgroundColorChangeWebAppBrowserTest
    : public WebAppBrowserTest,
      public testing::WithParamInterface<
          std::tuple<BackgroundColorChangeTestMode,
                     /*prefer_manifest_background_color=*/bool>> {
 public:
  BackgroundColorChangeWebAppBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    web_app::EnableSystemWebAppsInLacrosForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    switch (GetBackgroundColorChangeTestMode()) {
      case BackgroundColorChangeTestMode::kSWA:
        system_web_app_installation_ =
            TestSystemWebAppInstallation::SetUpAppWithColors(
                /*theme_color=*/SK_ColorWHITE,
                /*dark_mode_theme_color=*/SK_ColorBLACK,
                /*background_color=*/SK_ColorWHITE,
                /*dark_mode_background_color=*/SK_ColorBLACK);
        static_cast<UnittestingSystemAppDelegate*>(
            system_web_app_installation_->GetDelegate())
            ->SetPreferManifestBackgroundColor(PreferManifestBackgroundColor());
        break;
      case BackgroundColorChangeTestMode::kNonSWA:
        break;
    }
  }

  // Returns test mode given test parameterization.
  BackgroundColorChangeTestMode GetBackgroundColorChangeTestMode() const {
    return std::get<0>(GetParam());
  }

  // Returns whether the web app under test prefers manifest background colors
  // over web contents background colors.
  bool PreferManifestBackgroundColor() const { return std::get<1>(GetParam()); }

  // Installs the web app under test, blocking until installation is complete,
  // and returning the `AppId` for the installed web app.
  AppId WaitForAppInstall() {
    switch (GetBackgroundColorChangeTestMode()) {
      case BackgroundColorChangeTestMode::kSWA:
        system_web_app_installation_->WaitForAppInstall();
        return system_web_app_installation_->GetAppId();
      case BackgroundColorChangeTestMode::kNonSWA: {
        const GURL app_url = GetSecureAppURL();
        auto web_app_info = std::make_unique<WebAppInstallInfo>();
        web_app_info->start_url = app_url;
        web_app_info->scope = app_url.GetWithoutFilename();
        web_app_info->theme_color = SK_ColorWHITE;
        web_app_info->dark_mode_theme_color = SK_ColorBLACK;
        web_app_info->background_color = SK_ColorWHITE;
        web_app_info->dark_mode_background_color = SK_ColorBLACK;
        return InstallWebApp(std::move(web_app_info));
      }
    }
  }

 private:
  std::unique_ptr<TestSystemWebAppInstallation> system_web_app_installation_;
};

INSTANTIATE_TEST_SUITE_P(
    Mode,
    BackgroundColorChangeWebAppBrowserTest,
    testing::Combine(testing::Values(BackgroundColorChangeTestMode::kSWA,
                                     BackgroundColorChangeTestMode::kNonSWA),
                     /*prefer_manifest_background_color=*/testing::Bool()),
    [](const testing::TestParamInfo<
        std::tuple<BackgroundColorChangeTestMode,
                   /*prefer_manifest_background_color=*/bool>>& info) {
      BackgroundColorChangeTestMode test_mode = std::get<0>(info.param);
      bool prefer_manifest_background_color = std::get<1>(info.param);

      std::stringstream name;
      switch (test_mode) {
        case BackgroundColorChangeTestMode::kSWA:
          name << "kSWA";
          break;
        case BackgroundColorChangeTestMode::kNonSWA:
          name << "kNonSWA";
          break;
      }

      if (prefer_manifest_background_color)
        name << "_PreferManifestBackgroundColor";

      return name.str();
    });

IN_PROC_BROWSER_TEST_P(BackgroundColorChangeWebAppBrowserTest,
                       BackgroundColorChange) {
  // Skip test parameterizations for non-system web apps that don't make sense.
  if (GetBackgroundColorChangeTestMode() ==
          BackgroundColorChangeTestMode::kNonSWA &&
      PreferManifestBackgroundColor()) {
    GTEST_SKIP();
  }

  const AppId app_id = WaitForAppInstall();
  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  // Wait for original background color to load.
  {
    content::BackgroundColorChangeWaiter waiter(web_contents);
    waiter.Wait();
    EXPECT_EQ(app_browser->app_controller()->GetBackgroundColor().value(),
              SK_ColorWHITE);
  }
  content::AwaitDocumentOnLoadCompleted(web_contents);

  // Changing background color should update download shelf theme unless a
  // system web app prefers manifest background colors over web contents
  // background colors.
  {
    content::BackgroundColorChangeWaiter waiter(web_contents);
    EXPECT_TRUE(content::ExecuteScript(
        web_contents, "document.body.style.backgroundColor = 'cyan';"));
    waiter.Wait();
    EXPECT_EQ(app_browser->app_controller()->GetBackgroundColor().value(),
              PreferManifestBackgroundColor() ? SK_ColorWHITE : SK_ColorCYAN);
    SkColor download_shelf_color;
    app_browser->app_controller()->GetThemeSupplier()->GetColor(
        ThemeProperties::COLOR_DOWNLOAD_SHELF, &download_shelf_color);
    EXPECT_EQ(download_shelf_color,
              PreferManifestBackgroundColor() ? SK_ColorWHITE : SK_ColorCYAN);
  }
}

// This tests that we don't crash when launching a PWA window with an
// autogenerated user theme set.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, AutoGeneratedUserThemeCrash) {
  ThemeServiceFactory::GetForProfile(browser()->profile())
      ->BuildAutogeneratedThemeFromColor(SK_ColorBLUE);

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = GURL(kExampleURL);
  AppId app_id = InstallWebApp(std::move(web_app_info));

  LaunchWebAppBrowser(app_id);
}

// Check the 'Open in Chrome' menu button for web app windows.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, OpenInChrome) {
  const GURL app_url(kExampleURL);
  const AppId app_id = InstallPWA(app_url);

  {
    Browser* const app_browser = LaunchWebAppBrowser(app_id);

    EXPECT_EQ(1, app_browser->tab_strip_model()->count());
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
    ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

    chrome::ExecuteCommand(app_browser, IDC_OPEN_IN_CHROME);

    // The browser frame is closed next event loop so it's still safe to access
    // here.
    EXPECT_EQ(0, app_browser->tab_strip_model()->count());

    EXPECT_EQ(2, browser()->tab_strip_model()->count());
    EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
    EXPECT_EQ(
        app_url,
        browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
  }

  // Wait until the browser actually gets closed. This invalidates
  // |app_browser|.
  content::RunAllPendingInMessageLoop();
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

// Check the 'App info' menu button for web app windows.
#if BUILDFLAG(IS_LINUX)
// Disabled on Linux because the test only completes unless unrelated
// events are received to wake up the message loop.
#define MAYBE_AppInfoOpensPageInfo DISABLED_AppInfoOpensPageInfo
#else
#define MAYBE_AppInfoOpensPageInfo AppInfoOpensPageInfo
#endif
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, MAYBE_AppInfoOpensPageInfo) {
  const GURL app_url(kExampleURL);
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowser(app_id);

  base::RunLoop run_loop_dialog_created;
  GetPageInfoDialogCreatedCallbackForTesting() =
      run_loop_dialog_created.QuitClosure();
  chrome::ExecuteCommand(app_browser, IDC_WEB_APP_MENU_APP_INFO);
  // Wait for dialog to be created, timeout will trigger the test to fail.
  run_loop_dialog_created.Run();

  // The test closure should have run. But clear the global in case it hasn't.
  EXPECT_FALSE(GetPageInfoDialogCreatedCallbackForTesting());
  GetPageInfoDialogCreatedCallbackForTesting().Reset();
}

// Check that last launch time is set after launch.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, AppLastLaunchTime) {
  const GURL app_url(kExampleURL);
  const AppId app_id = InstallPWA(app_url);
  auto* provider = WebAppProvider::GetForTest(profile());

  // last_launch_time is not set before launch
  EXPECT_TRUE(provider->registrar().GetAppLastLaunchTime(app_id).is_null());

  auto before_launch = base::Time::Now();
  LaunchWebAppBrowser(app_id);

  EXPECT_TRUE(provider->registrar().GetAppLastLaunchTime(app_id) >=
              before_launch);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, DISABLE_POSIX(WithMinimalUiButtons)) {
  EXPECT_TRUE(HasMinimalUiButtons(DisplayMode::kBrowser, absl::nullopt,
                                  /*open_as_window=*/true));
  EXPECT_TRUE(HasMinimalUiButtons(DisplayMode::kMinimalUi, absl::nullopt,
                                  /*open_as_window=*/true));

  EXPECT_TRUE(HasMinimalUiButtons(DisplayMode::kBrowser, absl::nullopt,
                                  /*open_as_window=*/false));
  EXPECT_TRUE(HasMinimalUiButtons(DisplayMode::kMinimalUi, absl::nullopt,
                                  /*open_as_window=*/false));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, WithoutMinimalUiButtons) {
  EXPECT_FALSE(HasMinimalUiButtons(DisplayMode::kStandalone, absl::nullopt,
                                   /*open_as_window=*/true));
  EXPECT_FALSE(HasMinimalUiButtons(DisplayMode::kFullscreen, absl::nullopt,
                                   /*open_as_window=*/true));

  EXPECT_FALSE(HasMinimalUiButtons(DisplayMode::kStandalone, absl::nullopt,
                                   /*open_as_window=*/false));
  EXPECT_FALSE(HasMinimalUiButtons(DisplayMode::kFullscreen, absl::nullopt,
                                   /*open_as_window=*/false));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, DISABLE_POSIX(DisplayOverride)) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_display_override.json");
  NavigateToURLAndWait(browser(), test_url);

  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());

  std::vector<DisplayMode> app_display_mode_override =
      provider->registrar().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(2u, app_display_mode_override.size());
  EXPECT_EQ(DisplayMode::kMinimalUi, app_display_mode_override[0]);
  EXPECT_EQ(DisplayMode::kStandalone, app_display_mode_override[1]);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       WithMinimalUiButtons_DisplayOverride) {
  EXPECT_TRUE(HasMinimalUiButtons(DisplayMode::kStandalone,
                                  DisplayMode::kBrowser,
                                  /*open_as_window=*/true));
  EXPECT_TRUE(HasMinimalUiButtons(DisplayMode::kStandalone,
                                  DisplayMode::kMinimalUi,
                                  /*open_as_window=*/true));

  EXPECT_TRUE(HasMinimalUiButtons(DisplayMode::kStandalone,
                                  DisplayMode::kBrowser,
                                  /*open_as_window=*/false));
  EXPECT_TRUE(HasMinimalUiButtons(DisplayMode::kStandalone,
                                  DisplayMode::kMinimalUi,
                                  /*open_as_window=*/false));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       WithoutMinimalUiButtons_DisplayOverride) {
  EXPECT_FALSE(HasMinimalUiButtons(DisplayMode::kMinimalUi,
                                   DisplayMode::kStandalone,
                                   /*open_as_window=*/true));
  EXPECT_FALSE(HasMinimalUiButtons(DisplayMode::kMinimalUi,
                                   DisplayMode::kFullscreen,
                                   /*open_as_window=*/true));

  EXPECT_FALSE(HasMinimalUiButtons(DisplayMode::kMinimalUi,
                                   DisplayMode::kStandalone,
                                   /*open_as_window=*/false));
  EXPECT_FALSE(HasMinimalUiButtons(DisplayMode::kMinimalUi,
                                   DisplayMode::kFullscreen,
                                   /*open_as_window=*/false));
}

// Tests that desktop PWAs open out-of-scope links with a custom toolbar.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, DesktopPWAsOpenLinksInApp) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);
  NavigateToURLAndWait(app_browser, app_url);
  ASSERT_TRUE(app_browser->app_controller());
  NavigateAndCheckForToolbar(app_browser, GURL(kExampleURL), true);
}

// Tests that desktop PWAs open links in a new tab at the end of the tabstrip of
// the last active browser.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, DesktopPWAsOpenLinksInNewTab) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);
  NavigateToURLAndWait(app_browser, app_url);
  ASSERT_TRUE(app_browser->app_controller());

  EXPECT_EQ(chrome::GetTotalBrowserCount(), 2u);
  Browser* browser2 = CreateBrowser(app_browser->profile());
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 3u);

  TabStripModel* model2 = browser2->tab_strip_model();
  chrome::AddTabAt(browser2, GURL(), -1, true);
  EXPECT_EQ(model2->count(), 2);
  model2->SelectPreviousTab();
  EXPECT_EQ(model2->active_index(), 0);

  NavigateParams param(app_browser, GURL("http://www.google.com/"),
                       ui::PAGE_TRANSITION_LINK);
  param.window_action = NavigateParams::SHOW_WINDOW;
  param.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  ui_test_utils::NavigateToURL(&param);

  EXPECT_EQ(chrome::GetTotalBrowserCount(), 3u);
  EXPECT_EQ(model2->count(), 3);
  EXPECT_EQ(param.browser, browser2);
  EXPECT_EQ(model2->active_index(), 2);
  EXPECT_EQ(param.navigated_or_inserted_contents,
            model2->GetActiveWebContents());
}

// Tests that desktop PWAs are opened at the correct size.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, PWASizeIsCorrectlyRestored) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  EXPECT_TRUE(AppBrowserController::IsWebApp(app_browser));
  NavigateToURLAndWait(app_browser, app_url);

  const gfx::Rect bounds = gfx::Rect(50, 50, 550, 500);
  app_browser->window()->SetBounds(bounds);
  app_browser->window()->Close();

  Browser* const new_browser = LaunchWebAppBrowser(app_id);
  EXPECT_EQ(new_browser->window()->GetBounds(), bounds);
}

// Tests that desktop PWAs are reopened at the correct size.
IN_PROC_BROWSER_TEST_F(WebAppTabRestoreBrowserTest,
                       ReopenedPWASizeIsCorrectlyRestored) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  EXPECT_TRUE(AppBrowserController::IsWebApp(app_browser));
  NavigateToURLAndWait(app_browser, app_url);

  const gfx::Rect bounds = gfx::Rect(50, 50, 550, 500);
  app_browser->window()->SetBounds(bounds);
  app_browser->window()->Close();

  content::WebContentsAddedObserver new_contents_observer;

  sessions::TabRestoreService* const service =
      TabRestoreServiceFactory::GetForProfile(profile());
  ASSERT_GT(service->entries().size(), 0U);
  sessions::TabRestoreService::Entry* entry = service->entries().front().get();
  ASSERT_EQ(sessions::TabRestoreService::WINDOW, entry->type);
  const auto* entry_win =
      static_cast<sessions::TabRestoreService::Window*>(entry);
  EXPECT_EQ(bounds, entry_win->bounds);

  service->RestoreMostRecentEntry(nullptr);

  content::WebContents* const restored_web_contents =
      new_contents_observer.GetWebContents();
  Browser* const restored_browser =
      chrome::FindBrowserWithWebContents(restored_web_contents);
  EXPECT_EQ(restored_browser->override_bounds(), bounds);
}

// Tests that using window.open to create a popup window out of scope results in
// a correctly sized window.
// TODO(crbug.com/1234260): Stabilize the test.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_OffScopePWAPopupsHaveCorrectSize \
  DISABLED_OffScopePWAPopupsHaveCorrectSize
#else
#define MAYBE_OffScopePWAPopupsHaveCorrectSize OffScopePWAPopupsHaveCorrectSize
#endif
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       MAYBE_OffScopePWAPopupsHaveCorrectSize) {
  // TODO(crbug.com/1240482): the test expectations fail if the window gets CSD
  // and becomes smaller because of that.  Investigate this and remove the line
  // below if possible.
  ui::ScopedDisableClientSideDecorationsForTest scoped_disabled_csd;

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowser(app_id);

  EXPECT_TRUE(AppBrowserController::IsWebApp(app_browser));

  const GURL offscope_url =
      https_server()->GetURL("offscope.site.test", "/simple.html");
  const gfx::Size size(500, 500);

  Browser* const popup_browser =
      OpenPopupAndWait(app_browser, offscope_url, size);

  // The navigation should have happened in a new window.
  EXPECT_NE(popup_browser, app_browser);

  // The popup browser should be a PWA.
  EXPECT_TRUE(AppBrowserController::IsWebApp(popup_browser));

  // Toolbar should be shown, as the popup is out of scope.
  EXPECT_TRUE(popup_browser->app_controller()->ShouldShowCustomTabBar());

  // Skip animating the toolbar visibility.
  popup_browser->app_controller()->UpdateCustomTabBarVisibility(false);

  // The popup window should be the size we specified.
  EXPECT_EQ(size, popup_browser->window()->GetContentsSize());
}

// Tests that using window.open to create a popup window in scope results in
// a correctly sized window.
// TODO(crbug.com/1234260): Stabilize the test.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_InScopePWAPopupsHaveCorrectSize \
  DISABLED_InScopePWAPopupsHaveCorrectSize
#else
#define MAYBE_InScopePWAPopupsHaveCorrectSize InScopePWAPopupsHaveCorrectSize
#endif
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       MAYBE_InScopePWAPopupsHaveCorrectSize) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowser(app_id);

  EXPECT_TRUE(AppBrowserController::IsWebApp(app_browser));

  const gfx::Size size(500, 500);
  Browser* const popup_browser = OpenPopupAndWait(app_browser, app_url, size);

  // The navigation should have happened in a new window.
  EXPECT_NE(popup_browser, app_browser);

  // The popup browser should be a PWA.
  EXPECT_TRUE(AppBrowserController::IsWebApp(popup_browser));

  // Toolbar should not be shown, as the popup is in scope.
  EXPECT_FALSE(popup_browser->app_controller()->ShouldShowCustomTabBar());

  // Skip animating the toolbar visibility.
  popup_browser->app_controller()->UpdateCustomTabBarVisibility(false);

  // The popup window should be the size we specified.
  EXPECT_EQ(size, popup_browser->window()->GetContentsSize());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Tests that app windows are correctly restored.
IN_PROC_BROWSER_TEST_F(WebAppTabRestoreBrowserTest, RestoreAppWindow) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  ASSERT_TRUE(app_browser->is_type_app());
  app_browser->window()->Close();

  content::WebContentsAddedObserver new_contents_observer;

  sessions::TabRestoreService* const service =
      TabRestoreServiceFactory::GetForProfile(profile());
  service->RestoreMostRecentEntry(nullptr);

  content::WebContents* const restored_web_contents =
      new_contents_observer.GetWebContents();
  Browser* const restored_browser =
      chrome::FindBrowserWithWebContents(restored_web_contents);

  EXPECT_TRUE(restored_browser->is_type_app());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// Test navigating to an out of scope url on the same origin causes the url
// to be shown to the user.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       LocationBarIsVisibleOffScopeOnSameOrigin) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  // Toolbar should not be visible in the app.
  ASSERT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // The installed PWA's scope is app.com:{PORT}/ssl,
  // so app.com:{PORT}/accessibility_fail.html is out of scope.
  const GURL out_of_scope = GetURLForPath("/accessibility_fail.html");
  NavigateToURLAndWait(app_browser, out_of_scope);

  // Location should be visible off scope.
  ASSERT_TRUE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, UpgradeWithoutCustomTabBar) {
  const GURL secure_app_url =
      https_server()->GetURL("app.site.test", "/empty.html");
  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpScheme);
  const GURL app_url = secure_app_url.ReplaceComponents(rep);

  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  NavigateToURLAndWait(app_browser, secure_app_url);

  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  const GURL off_origin_url =
      https_server()->GetURL("example.org", "/empty.html");
  NavigateToURLAndWait(app_browser, off_origin_url);
  EXPECT_EQ(app_browser->app_controller()->ShouldShowCustomTabBar(), true);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, OverscrollEnabled) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  // Overscroll is only enabled on Aura platforms currently.
#if defined(USE_AURA)
  EXPECT_TRUE(app_browser->CanOverscrollContent());
#else
  EXPECT_FALSE(app_browser->CanOverscrollContent());
#endif
}

// Check the 'Copy URL' menu button for Web App windows.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, CopyURL) {
  const GURL app_url(kExampleURL);
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  content::BrowserTestClipboardScope test_clipboard_scope;
  chrome::ExecuteCommand(app_browser, IDC_COPY_URL);

  ui::Clipboard* const clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string result;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &result);
  EXPECT_EQ(result, kExampleURL16);
}

// Tests that the command for popping a tab out to a PWA window is disabled in
// incognito.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, PopOutDisabledInIncognito) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);

  Browser* const incognito_browser = OpenURLOffTheRecord(profile(), app_url);
  auto app_menu_model =
      std::make_unique<AppMenuModel>(nullptr, incognito_browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  ASSERT_TRUE(app_menu_model->GetModelAndIndexForCommandId(
      IDC_OPEN_IN_PWA_WINDOW, &model, &index));
  EXPECT_FALSE(model->IsEnabledAt(index));
}

// Tests that web app menus don't crash when no tabs are selected.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, NoTabSelectedMenuCrash) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  app_browser->tab_strip_model()->CloseAllTabs();
  auto app_menu_model = std::make_unique<WebAppMenuModel>(
      /*provider=*/nullptr, app_browser);
  app_menu_model->Init();
}

// Tests that PWA menus have an uninstall option.
// TODO(crbug.com/1271118): Flaky on mac arm64.
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
#define MAYBE_UninstallMenuOption DISABLED_UninstallMenuOption
#else
#define MAYBE_UninstallMenuOption UninstallMenuOption
#endif
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, MAYBE_UninstallMenuOption) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  auto app_menu_model = std::make_unique<WebAppMenuModel>(
      /*provider=*/nullptr, app_browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  const bool found = app_menu_model->GetModelAndIndexForCommandId(
      WebAppMenuModel::kUninstallAppCommandId, &model, &index);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(found);
#else
  EXPECT_TRUE(found);
  EXPECT_TRUE(model->IsEnabledAt(index));

  base::HistogramTester tester;
  app_menu_model->ExecuteCommand(WebAppMenuModel::kUninstallAppCommandId,
                                 /*event_flags=*/0);
  tester.ExpectUniqueSample("HostedAppFrame.WrenchMenu.MenuAction",
                            MENU_ACTION_UNINSTALL_APP, 1);
  tester.ExpectUniqueSample("WrenchMenu.MenuAction", MENU_ACTION_UNINSTALL_APP,
                            1);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

// Tests that both installing a PWA and creating a shortcut app are disabled for
// incognito windows.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ShortcutMenuOptionsInIncognito) {
  Browser* const incognito_browser = CreateIncognitoBrowser(profile());
  EXPECT_EQ(webapps::AppBannerManagerDesktop::FromWebContents(
                incognito_browser->tab_strip_model()->GetActiveWebContents()),
            nullptr);
  NavigateToURLAndWait(incognito_browser, GetInstallableAppURL());

  // Wait sufficient time for an installability check to occur.
  EXPECT_TRUE(
      NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL()));

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, incognito_browser),
            kDisabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, incognito_browser),
            kNotPresent);
}

// Tests that both installing a PWA and creating a shortcut app are disabled for
// an error page.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ShortcutMenuOptionsForErrorPage) {
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(
      browser(), https_server()->GetURL("/invalid_path.html")));

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kDisabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), kNotPresent);
}

// Tests that both installing a PWA and creating a shortcut app are available
// for an installable PWA.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       ShortcutMenuOptionsForInstallablePWA) {
  EXPECT_TRUE(
      NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL()));

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), kEnabled);
}

// Tests that both installing a PWA and creating a shortcut app are disabled
// when page crashes.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ShortcutMenuOptionsForCrashedTab) {
  EXPECT_TRUE(
      NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL()));
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    content::RenderFrameDeletedObserver crash_observer(
        tab_contents->GetMainFrame());
    tab_contents->GetMainFrame()->GetProcess()->Shutdown(1);
    crash_observer.WaitUntilDeleted();
  }
  ASSERT_TRUE(tab_contents->IsCrashed());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kDisabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), kDisabled);
}

// Tests that an installed PWA is not used when out of scope by one path level.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       DISABLE_POSIX(MenuOptionsOutsideInstalledPwaScope)) {
  NavigateToURLAndWait(
      browser(),
      https_server()->GetURL("/banners/scope_is_start_url/index.html"));
  test::InstallPwaForCurrentUrl(browser());

  // Open a page that is one directory up from the installed PWA.
  Browser* const new_browser = NavigateInNewWindowAndAwaitInstallabilityCheck(
      https_server()->GetURL("/banners/no_manifest_test_page.html"));

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       DISABLE_POSIX(InstallInstallableSite)) {
  base::Time before_install_time = base::Time::Now();
  base::UserActionTester user_action_tester;
  NavigateToURLAndWait(browser(), GetInstallableAppURL());

  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());
  EXPECT_EQ(provider->registrar().GetAppShortName(app_id),
            GetInstallableAppName());

  // Installed PWAs should launch in their own window.
  EXPECT_EQ(provider->registrar().GetAppUserDisplayMode(app_id),
            web_app::UserDisplayMode::kStandalone);

  // Installed PWAs should have install time set.
  EXPECT_TRUE(provider->registrar().GetAppInstallTime(app_id) >=
              before_install_time);

  EXPECT_EQ(1, user_action_tester.GetActionCount("InstallWebAppFromMenu"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("CreateShortcut"));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Apps on Chrome OS should not be pinned after install.
  EXPECT_FALSE(ChromeShelfController::instance()->IsAppPinned(app_id));
#endif
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       DISABLE_POSIX(CanInstallOverBrowserTabPwa)) {
  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  const AppId app_id = test::InstallPwaForCurrentUrl(browser());

  // Change display mode to open in tab.
  auto* provider = WebAppProvider::GetForTest(profile());
  provider->sync_bridge().SetAppUserDisplayMode(
      app_id, web_app::UserDisplayMode::kBrowser, /*is_user_action=*/false);

  Browser* const new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       DISABLE_POSIX(CannotInstallOverWindowPwa)) {
  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  test::InstallPwaForCurrentUrl(browser());

  // Avoid any interference if active browser was changed by PWA install.
  Browser* const new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kEnabled);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, NoOpenInAppForBrowserTabPwa) {
  GURL app_url = https_server()->GetURL(
      "/web_apps/get_manifest.html?display_browser.json");
  AppId app_id = InstallWebAppFromPage(browser(), app_url);

  // Change display mode to open in tab.
  auto* provider = WebAppProvider::GetForTest(profile());
  provider->sync_bridge().SetAppUserDisplayMode(
      app_id, web_app::UserDisplayMode::kBrowser, /*is_user_action=*/false);

  NavigateToURLAndWait(browser(), app_url);
  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), kNotPresent);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, CanInstallWithPolicyPwa) {
  ExternalInstallOptions options = CreateInstallOptions(GetInstallableAppURL());
  options.install_source = ExternalInstallSource::kExternalPolicy;
  ExternallyManagedAppManagerInstall(profile(), options);

  // Avoid any interference if active browser was changed by PWA install.
  Browser* const new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kEnabled);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       CannotUninstallPolicyWebAppAfterUserInstall) {
  GURL install_url = GetInstallableAppURL();
  ExternalInstallOptions options = CreateInstallOptions(install_url);
  options.install_source = ExternalInstallSource::kExternalPolicy;
  ExternallyManagedAppManagerInstall(profile(), options);

  auto* provider = WebAppProvider::GetForTest(browser()->profile());
  ExternallyInstalledWebAppPrefs prefs(browser()->profile()->GetPrefs());
  AppId app_id = prefs.LookupAppId(install_url).value();

  EXPECT_FALSE(provider->install_finalizer().CanUserUninstallWebApp(app_id));

  InstallWebAppFromPage(browser(), install_url);

  // Performing a user install on the page should not override the "policy"
  // install source.
  EXPECT_FALSE(provider->install_finalizer().CanUserUninstallWebApp(app_id));
  const WebApp& web_app = *provider->registrar().GetAppById(app_id);
  EXPECT_TRUE(web_app.IsSynced());
  EXPECT_TRUE(web_app.IsPolicyInstalledApp());
}

// Tests that the command for OpenActiveTabInPwaWindow is available for secure
// pages in an app's scope.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ReparentWebAppForSecureActiveTab) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);

  NavigateToURLAndWait(browser(), app_url);
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(tab_contents->GetLastCommittedURL(), app_url);

  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kEnabled);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       DISABLE_POSIX(ShortcutIconCorrectColor)) {
  os_hooks_suppress_.reset();
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::unique_ptr<ScopedShortcutOverrideForTesting> shortcut_override =
      OverrideShortcutsForTesting();

  NavigateToURLAndWait(
      browser(),
      https_server()->GetURL(
          "/banners/manifest_test_page.html?manifest=manifest_one_icon.json"));

  // Wait for OS hooks and installation to complete and the app to launch.
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(base::BindLambdaForTesting(
      [&](const AppId& installed_app_id) { run_loop_install.Quit(); }));
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  run_loop_install.Run();
  app_loaded_observer.Wait();

  base::FilePath shortcut_path;
  auto* provider = WebAppProvider::GetForTest(profile());
  std::vector<SkColor> expected_pixel_colors = {SkColorSetRGB(92, 92, 92)};
#if BUILDFLAG(IS_MAC)
  shortcut_path = shortcut_override->chrome_apps_folder.GetPath().Append(
      provider->registrar().GetAppShortName(app_id) + ".app");
#elif BUILDFLAG(IS_WIN)
  shortcut_path = shortcut_override->application_menu.GetPath().AppendASCII(
      provider->registrar().GetAppShortName(app_id) + ".lnk");
  expected_pixel_colors.push_back(SkColorSetRGB(91, 91, 91));
  expected_pixel_colors.push_back(SkColorSetRGB(90, 90, 90));
#endif
  SkColor icon_pixel_color = GetIconTopLeftColor(shortcut_path);
  EXPECT_TRUE(std::find(expected_pixel_colors.begin(),
                        expected_pixel_colors.end(),
                        icon_pixel_color) != expected_pixel_colors.end())
      << "Actual color (RGB) is: "
      << color_utils::SkColorToRgbString(icon_pixel_color);

  base::RunLoop run_loop_uninstall;
  provider->install_finalizer().UninstallWebApp(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
        run_loop_uninstall.Quit();
      }));
  run_loop_uninstall.Run();
}
#endif

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_ShortcutMenu, ShortcutsMenu) {
  struct ShortcutsMenuItem {
   public:
    ShortcutsMenuItem() : command_line(base::CommandLine::NO_PROGRAM) {}

    // The string to be displayed in a shortcut menu item.
    std::u16string title;

    // Used for storing and appending command-line arguments.
    base::CommandLine command_line;

    // The absolute path to an icon to be displayed in a shortcut menu item.
    base::FilePath icon_path;
  };

  os_hooks_suppress_.reset();
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::unique_ptr<ScopedShortcutOverrideForTesting> shortcut_override =
      OverrideShortcutsForTesting();
  NavigateToURLAndWait(
      browser(),
      https_server()->GetURL(
          "/banners/"
          "manifest_test_page.html?manifest=manifest_with_shortcuts.json"));

  std::vector<ShortcutsMenuItem> shortcuts_menu_items;

  auto SaveJumpList = base::BindLambdaForTesting(
      [&](std::wstring,
          const std::vector<scoped_refptr<ShellLinkItem>>& link_items) -> bool {
        for (auto& shell_item : link_items) {
          ShortcutsMenuItem item;
          item.title = shell_item->title();
          item.icon_path = shell_item->icon_path();
          item.command_line = *shell_item->GetCommandLine();
          shortcuts_menu_items.push_back(item);
        }
        return true;
      });

  SetUpdateJumpListForTesting(SaveJumpList);

  // Wait for OS hooks and installation to complete and the app to launch.
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(base::BindLambdaForTesting(
      [&](const AppId& installed_app_id) { run_loop_install.Quit(); }));
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  run_loop_install.Run();
  app_loaded_observer.Wait();

  EXPECT_EQ(2U, shortcuts_menu_items.size());
  EXPECT_EQ(u"shortcut1", shortcuts_menu_items[0].title);
  EXPECT_EQ(u"shortcut2", shortcuts_menu_items[1].title);
  EXPECT_TRUE(base::PathExists(shortcuts_menu_items[0].icon_path));
  EXPECT_TRUE(base::PathExists(shortcuts_menu_items[1].icon_path));
  EXPECT_EQ(app_id, shortcuts_menu_items[0].command_line.GetSwitchValueASCII(
                        switches::kAppId));
  EXPECT_EQ(app_id, shortcuts_menu_items[1].command_line.GetSwitchValueASCII(
                        switches::kAppId));
  EXPECT_NE(
      std::string::npos,
      shortcuts_menu_items[0]
          .command_line
          .GetSwitchValueASCII(switches::kAppLaunchUrlForShortcutsMenuItem)
          .find("/banners/launch_url1"));
  EXPECT_NE(
      std::string::npos,
      shortcuts_menu_items[1]
          .command_line
          .GetSwitchValueASCII(switches::kAppLaunchUrlForShortcutsMenuItem)
          .find("/banners/launch_url2"));

  base::RunLoop run_loop_uninstall;
  WebAppProvider::GetForTest(profile())->install_finalizer().UninstallWebApp(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
        run_loop_uninstall.Quit();
      }));
  run_loop_uninstall.Run();
}
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, WebAppCreateAndDeleteShortcut) {
  os_hooks_suppress_.reset();

  base::ScopedAllowBlockingForTesting allow_blocking;

  std::unique_ptr<ScopedShortcutOverrideForTesting> shortcut_override =
      OverrideShortcutsForTesting();

  auto* provider = WebAppProvider::GetForTest(profile());

  NavigateToURLAndWait(browser(), GetInstallableAppURL());

  // Wait for OS hooks and installation to complete and the app to launch.
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(base::BindLambdaForTesting(
      [&](const AppId& installed_app_id) { run_loop_install.Quit(); }));
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  run_loop_install.Run();
  app_loaded_observer.Wait();

  EXPECT_TRUE(provider->registrar().IsInstalled(app_id));
  EXPECT_EQ(provider->registrar().GetAppShortName(app_id),
            GetInstallableAppName());

#if BUILDFLAG(IS_WIN)
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  std::wstring shortcut_filename = converter.from_bytes(
      provider->registrar().GetAppShortName(app_id) + ".lnk");
  base::FilePath desktop_shortcut_path =
      shortcut_override->desktop.GetPath().Append(shortcut_filename);
  base::FilePath app_menu_shortcut_path =
      shortcut_override->application_menu.GetPath().Append(shortcut_filename);
  EXPECT_TRUE(base::PathExists(desktop_shortcut_path));
  EXPECT_TRUE(base::PathExists(app_menu_shortcut_path));
#elif BUILDFLAG(IS_MAC)
  std::string shortcut_filename =
      provider->registrar().GetAppShortName(app_id) + ".app";
  base::FilePath app_shortcut_path =
      shortcut_override->chrome_apps_folder.GetPath().Append(shortcut_filename);
  EXPECT_TRUE(base::PathExists(app_shortcut_path));
#elif BUILDFLAG(IS_LINUX)
  std::string shortcut_filename = "chrome-" + app_id + "-Default.desktop";
  base::FilePath desktop_shortcut_path =
      shortcut_override->desktop.GetPath().Append(shortcut_filename);
  EXPECT_TRUE(base::PathExists(desktop_shortcut_path));
#endif

  // Unistall the web app
  base::RunLoop run_loop_uninstall;
  provider->install_finalizer().UninstallWebApp(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
        run_loop_uninstall.Quit();
      }));
  run_loop_uninstall.Run();

#if BUILDFLAG(IS_WIN)
  EXPECT_FALSE(base::PathExists(desktop_shortcut_path));
  EXPECT_FALSE(base::PathExists(app_menu_shortcut_path));
#elif BUILDFLAG(IS_MAC)
  EXPECT_FALSE(base::PathExists(app_shortcut_path));
#elif BUILDFLAG(IS_LINUX)
  EXPECT_FALSE(base::PathExists(desktop_shortcut_path));
#endif
}  // namespace web_app
#endif

// Tests that reparenting the last browser tab doesn't close the browser window.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ReparentLastBrowserTab) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  NavigateToURLAndWait(browser(), app_url);

  Browser* const app_browser = ReparentWebAppForActiveTab(browser());
  ASSERT_EQ(app_browser->app_controller()->app_id(), app_id);

  ASSERT_TRUE(IsBrowserOpen(browser()));
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
}

// Tests that reparenting a display: browser app tab results in a minimal-ui
// app window.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ReparentDisplayBrowserApp) {
  const GURL app_url = GetSecureAppURL();
  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->display_mode = DisplayMode::kBrowser;
  web_app_info->user_display_mode = UserDisplayMode::kStandalone;
  web_app_info->title = u"A Shortcut App";
  const AppId app_id = InstallWebApp(std::move(web_app_info));

  base::HistogramTester tester;
  NavigateToURLAndWait(browser(), app_url);
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(tab_contents->GetLastCommittedURL(), app_url);

  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kEnabled);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_OPEN_IN_PWA_WINDOW));

  Browser* const app_browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_EQ(app_browser->app_controller()->app_id(), app_id);
  EXPECT_TRUE(app_browser->app_controller()->HasMinimalUiButtons());

  auto* provider = WebAppProvider::GetForTest(profile());
  EXPECT_EQ(provider->registrar().GetAppUserDisplayMode(app_id),
            UserDisplayMode::kStandalone);
  EXPECT_EQ(provider->registrar().GetAppEffectiveDisplayMode(app_id),
            DisplayMode::kMinimalUi);
  EXPECT_FALSE(provider->registrar().GetAppLastLaunchTime(app_id).is_null());
  tester.ExpectUniqueSample(
      "Extensions.BookmarkAppLaunchContainer",
      apps::mojom::LaunchContainer::kLaunchContainerWindow, 1);
  tester.ExpectUniqueSample("Extensions.BookmarkAppLaunchSource",
                            extensions::AppLaunchSource::kSourceReparenting, 1);
}

// Tests that the manifest name of the current installable site is used in the
// installation menu text.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, InstallToShelfContainsAppName) {
  EXPECT_TRUE(
      NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL()));

  auto app_menu_model = std::make_unique<AppMenuModel>(nullptr, browser());
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  EXPECT_TRUE(app_menu_model->GetModelAndIndexForCommandId(IDC_INSTALL_PWA,
                                                           &model, &index));
  EXPECT_EQ(app_menu_model.get(), model);
  EXPECT_EQ(model->GetLabelAt(index), u"Install Manifest test app…");
}

// Check that no assertions are hit when showing a permission request bubble.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, PermissionBubble) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  content::RenderFrameHost* const render_frame_host =
      app_browser->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  EXPECT_TRUE(content::ExecuteScript(
      render_frame_host,
      "navigator.geolocation.getCurrentPosition(function(){});"));
}

class WebAppBrowserTest_PrefixInTitle
    : public WebAppBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  WebAppBrowserTest_PrefixInTitle() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kPrefixWebAppWindowsWithAppName);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kPrefixWebAppWindowsWithAppName);
    }
  }

  bool ExpectPrefixInTitle() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensure that web app windows don't duplicate the app name in the title, when
// the page's title already starts with the app name.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest_PrefixInTitle, PrefixExistsInTitle) {
  const GURL app_url =
      https_server()->GetURL("app.com", "/web_apps/title_appname_prefix.html");
  const std::u16string app_title = u"A Web App";

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->title = app_title;
  const AppId app_id = InstallWebApp(std::move(web_app_info));

  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // The window title should not repeat "A Web App".
  EXPECT_EQ(u"A Web App - funny cat video",
            app_browser->GetWindowTitleForCurrentTab(false));
}

// Ensure that web app windows with blank titles don't display the URL as a
// default window title.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest_PrefixInTitle,
                       WebAppWindowTitleForEmptyAndSimpleWebContentTitles) {
  // Ensure web app windows show the expected title when the contents have an
  // empty or simple title.
  const GURL app_url = https_server()->GetURL("app.site.test", "/empty.html");
  const std::u16string app_title = u"A Web App";
  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->title = app_title;
  const AppId app_id = InstallWebApp(std::move(web_app_info));
  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  if (ExpectPrefixInTitle()) {
    EXPECT_EQ(app_title, app_browser->GetWindowTitleForCurrentTab(false));
  } else {
    EXPECT_EQ(std::u16string(),
              app_browser->GetWindowTitleForCurrentTab(false));
  }
  NavigateToURLAndWait(app_browser,
                       https_server()->GetURL("app.site.test", "/simple.html"));
  if (ExpectPrefixInTitle()) {
    EXPECT_EQ(u"A Web App - OK",
              app_browser->GetWindowTitleForCurrentTab(false));
  } else {
    EXPECT_EQ(u"OK", app_browser->GetWindowTitleForCurrentTab(false));
  }
}

// Ensure that web app windows display the app title instead of the page
// title when off scope.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest_PrefixInTitle,
                       OffScopeUrlsDisplayAppTitle) {
  const GURL app_url = GetSecureAppURL();
  const std::u16string app_title = u"A Web App";

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->title = app_title;
  const AppId app_id = InstallWebApp(std::move(web_app_info));

  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // When we are within scope, show the page title.
  if (ExpectPrefixInTitle()) {
    EXPECT_EQ(u"A Web App - Google",
              app_browser->GetWindowTitleForCurrentTab(false));
  } else {
    EXPECT_EQ(u"Google", app_browser->GetWindowTitleForCurrentTab(false));
  }
  NavigateToURLAndWait(app_browser,
                       https_server()->GetURL("app.site.test", "/simple.html"));

  // When we are off scope, show the app title.
  EXPECT_EQ(app_title, app_browser->GetWindowTitleForCurrentTab(false));
}

INSTANTIATE_TEST_SUITE_P(WebAppBrowserTestTitlePrefix,
                         WebAppBrowserTest_PrefixInTitle,
                         ::testing::Values(true, false));

// Ensure that web app windows display the app title instead of the page
// title when using http.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, InScopeHttpUrlsDisplayAppTitle) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("app.site.test", "/simple.html");
  const std::u16string app_title = u"A Web App";

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = app_url;
  web_app_info->title = app_title;
  const AppId app_id = InstallWebApp(std::move(web_app_info));

  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // The page title is "OK" but the page is being served over HTTP, so the app
  // title should be used instead.
  EXPECT_EQ(app_title, app_browser->GetWindowTitleForCurrentTab(false));
}

class WebAppBrowserTest_HideOrigin : public WebAppBrowserTest {
 public:
  WebAppBrowserTest_HideOrigin() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kHideWebAppOriginText};
};

// WebApps should not have origin text with this feature on.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_HideOrigin, OriginTextRemoved) {
  const GURL app_url = GetInstallableAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);
  EXPECT_FALSE(app_browser->app_controller()->HasTitlebarAppOriginText());
}

// Check that a subframe on a regular web page can navigate to a URL that
// redirects to a web app.  https://crbug.com/721949.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, SubframeRedirectsToWebApp) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set up a web app which covers app.com URLs.
  GURL app_url = embedded_test_server()->GetURL("app.com", "/title1.html");
  const AppId app_id = InstallPWA(app_url);

  // Navigate a regular tab to a page with a subframe.
  const GURL url = embedded_test_server()->GetURL("foo.com", "/iframe.html");
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigateToURLAndWait(browser(), url);

  // Navigate the subframe to a URL that redirects to a URL in the web app's
  // web extent.
  const GURL redirect_url = embedded_test_server()->GetURL(
      "bar.com", "/server-redirect?" + app_url.spec());
  EXPECT_TRUE(NavigateIframeToURL(tab, "test", redirect_url));

  // Ensure that the frame navigated successfully and that it has correct
  // content.
  content::RenderFrameHost* const subframe =
      content::ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_EQ(app_url, subframe->GetLastCommittedURL());
  EXPECT_EQ(
      "This page has no title.",
      EvalJs(subframe, "document.body.innerText.trim();").ExtractString());
}

#if BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, NewAppWindow) {
  BrowserList* const browser_list = BrowserList::GetInstance();
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowser(app_id);

  EXPECT_EQ(browser_list->size(), 2U);
  EXPECT_TRUE(chrome::ExecuteCommand(app_browser, IDC_NEW_WINDOW));
  EXPECT_EQ(browser_list->size(), 3U);
  Browser* const new_browser = browser_list->GetLastActive();
  EXPECT_NE(new_browser, browser());
  EXPECT_NE(new_browser, app_browser);
  EXPECT_TRUE(new_browser->is_type_app());
  EXPECT_EQ(new_browser->app_controller()->app_id(), app_id);

  WebAppProvider::GetForTest(profile())->sync_bridge().SetAppUserDisplayMode(
      app_id, web_app::UserDisplayMode::kBrowser,
      /*is_user_action=*/false);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_TRUE(chrome::ExecuteCommand(app_browser, IDC_NEW_WINDOW));
  EXPECT_EQ(browser_list->GetLastActive(), browser());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      app_url);
}

#endif

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, PopupLocationBar) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  const GURL app_url = GetSecureAppURL();
  const GURL in_scope =
      https_server()->GetURL("app.com", "/ssl/page_with_subresource.html");
  const AppId app_id = InstallPWA(app_url);

  Browser* const popup_browser = web_app::CreateWebApplicationWindow(
      profile(), app_id, WindowOpenDisposition::NEW_POPUP, /*restore_id=*/0);

  EXPECT_TRUE(
      popup_browser->CanSupportWindowFeature(Browser::FEATURE_LOCATIONBAR));
  EXPECT_TRUE(
      popup_browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR));

  FullscreenNotificationObserver waiter(popup_browser);
  chrome::ToggleFullscreenMode(popup_browser);
  waiter.Wait();

  EXPECT_TRUE(
      popup_browser->CanSupportWindowFeature(Browser::FEATURE_LOCATIONBAR));
}

// Make sure chrome://web-app-internals page loads fine.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, WebAppInternalsPage) {
  // Loads with no web app.
  NavigateToURLAndWait(browser(), GURL("chrome://web-app-internals"));

  const GURL app_url = GetSecureAppURL();
  InstallPWA(app_url);
  // Loads with one web app.
  NavigateToURLAndWait(browser(), GURL("chrome://web-app-internals"));

  // Install a non-promotable web app.
  NavigateToURLAndWait(
      browser(), https_server()->GetURL("/banners/no_manifest_test_page.html"));
  chrome::SetAutoAcceptWebAppDialogForTesting(/*auto_accept=*/true,
                                              /*auto_open_in_window=*/false);
  WebAppTestInstallObserver observer(profile());
  observer.BeginListening();
  CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
  observer.Wait();
  chrome::SetAutoAcceptWebAppDialogForTesting(false, false);
  // Loads with two apps.
  NavigateToURLAndWait(browser(), GURL("chrome://web-app-internals"));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, BrowserDisplayNotInstallable) {
  GURL url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_display_browser.json");
  NavigateAndAwaitInstallabilityCheck(browser(), url);

  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), kNotPresent);

  // Install using Create Shortcut.
  chrome::SetAutoAcceptWebAppDialogForTesting(/*auto_accept=*/true,
                                              /*auto_open_in_window=*/false);
  WebAppTestInstallObserver observer(profile());
  observer.BeginListening();
  CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
  observer.Wait();
  chrome::SetAutoAcceptWebAppDialogForTesting(false, false);

  // Navigate to this site again and install should still be disabled.
  Browser* new_browser = NavigateInNewWindowAndAwaitInstallabilityCheck(url);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_WindowControlsOverlay,
                       DISABLE_POSIX(WindowControlsOverlay)) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_window_controls_overlay.json");
  NavigateToURLAndWait(browser(), test_url);

  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());

  std::vector<DisplayMode> app_display_mode_override =
      provider->registrar().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(1u, app_display_mode_override.size());
  EXPECT_EQ(DisplayMode::kWindowControlsOverlay, app_display_mode_override[0]);

  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  EXPECT_EQ(true,
            app_browser->app_controller()->AppUsesWindowControlsOverlay());
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_Tabbed,
                       DISABLE_POSIX(TabbedDisplayOverride)) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_tabbed_display_override.json");
  NavigateToURLAndWait(browser(), test_url);

  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());

  std::vector<DisplayMode> app_display_mode_override =
      provider->registrar().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(1u, app_display_mode_override.size());
  EXPECT_EQ(DisplayMode::kTabbed, app_display_mode_override[0]);
  EXPECT_EQ(true, provider->registrar().IsTabbedWindowModeEnabled(app_id));
}

class WebAppBrowserTest_RemoveStatusBar : public WebAppBrowserTest {
 public:
  WebAppBrowserTest_RemoveStatusBar() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kRemoveStatusBarInWebApps};
};

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_RemoveStatusBar,
                       DISABLE_POSIX(RemoveStatusBar)) {
  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  EXPECT_EQ(nullptr, app_browser->GetStatusBubbleForTesting());
}

class WebAppBrowserTest_NoDestroyProfile : public WebAppBrowserTest {
 public:
  WebAppBrowserTest_NoDestroyProfile() {
    // This test only makes sense when DestroyProfileOnBrowserClose is
    // disabled.
    feature_list_.InitAndDisableFeature(
        features::kDestroyProfileOnBrowserClose);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that no web app is launched during shutdown.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_NoDestroyProfile, Shutdown) {
  Profile* profile = browser()->profile();
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
  apps::AppLaunchParams params(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, apps::mojom::LaunchSource::kFromTest);

  BrowserHandler handler(nullptr, std::string());
  handler.Close();
  ui_test_utils::WaitForBrowserToClose();

  content::WebContents* const web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(std::move(params));
  EXPECT_EQ(web_contents, nullptr);
}

class WebAppBrowserTest_ManifestId : public WebAppBrowserTest {
 public:
  WebAppBrowserTest_ManifestId() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppEnableManifestId};
};

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_ManifestId,
                       DISABLE_POSIX(NoManifestId)) {
  NavigateToURLAndWait(browser(), GetInstallableAppURL());

  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());
  auto* app = provider->registrar().GetAppById(app_id);

  EXPECT_EQ(
      web_app::GenerateAppId(/*manifest_id=*/absl::nullopt,
                             provider->registrar().GetAppStartUrl(app_id)),
      app_id);
  EXPECT_EQ(app->start_url().spec().substr(
                app->start_url().DeprecatedGetOriginAsURL().spec().size()),
            app->manifest_id());
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_ManifestId, ManifestIdSpecified) {
  NavigateAndAwaitInstallabilityCheck(
      browser(),
      https_server()->GetURL(
          "/banners/manifest_test_page.html?manifest=manifest_with_id.json"));

  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());
  auto* app = provider->registrar().GetAppById(app_id);

  EXPECT_EQ(web_app::GenerateAppId(app->manifest_id(), app->start_url()),
            app_id);
  EXPECT_NE(
      web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, app->start_url()),
      app_id);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
class WebAppBrowserTest_FileHandler : public WebAppBrowserTest {
 public:
  WebAppBrowserTest_FileHandler() {
    feature_list_.InitAndEnableFeature(blink::features::kFileHandlingAPI);
  }

#if BUILDFLAG(IS_WIN)
 protected:
  void SetUp() override {
    // Don't pollute Windows registry of machine running tests.
    registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
    WebAppBrowserTest::SetUp();
  }

  registry_util::RegistryOverrideManager registry_override_manager_;
#endif  // BUILDFLAG(IS_WIN)

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/1320285): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RegKeysFileExtension DISABLED_RegKeysFileExtension
#else
#define MAYBE_RegKeysFileExtension RegKeysFileExtension
#endif
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_FileHandler,
                       MAYBE_RegKeysFileExtension) {
  os_hooks_suppress_.reset();
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::unique_ptr<ScopedShortcutOverrideForTesting> shortcut_override =
      OverrideShortcutsForTesting(base::GetHomeDir());
  std::vector<std::string> expected_extensions{"bar", "baz", "foo", "foobar"};

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL app_url(embedded_test_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_with_file_handlers.json"));

  NavigateToURLAndWait(browser(), app_url);

  // Wait for OS hooks and installation to complete.
  chrome::SetAutoAcceptWebAppDialogForTesting(true, true);
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(base::BindLambdaForTesting(
      [&](const AppId& installed_app_id) { run_loop_install.Quit(); }));
  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  run_loop_install.Run();
  content::RunAllTasksUntilIdle();
  chrome::SetAutoAcceptWebAppDialogForTesting(false, false);

#if BUILDFLAG(IS_WIN)
  const std::wstring prog_id =
      GetProgIdForApp(browser()->profile()->GetPath(), app_id);
  const std::vector<std::wstring> file_handler_prog_ids =
      ShellUtil::GetFileHandlerProgIdsForAppId(prog_id);
  base::flat_map<std::wstring, std::wstring> reg_key_prog_id_map;

  std::vector<std::wstring> file_ext_reg_keys;
  base::win::RegKey key;
  for (const auto& file_handler_prog_id : file_handler_prog_ids) {
    const std::vector<std::wstring> file_extensions =
        GetFileExtensionsForProgId(file_handler_prog_id);
    for (const auto& file_extension : file_extensions) {
      std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
      const std::string extension =
          converter.to_bytes(file_extension.substr(1));
      EXPECT_TRUE(std::find(expected_extensions.begin(),
                            expected_extensions.end(),
                            extension) != expected_extensions.end())
          << "Missing file extension: " << extension;
      const std::wstring reg_key =
          L"Software\\Classes\\" + file_extension + L"\\OpenWithProgids";
      reg_key_prog_id_map[reg_key] = file_handler_prog_id;
      ASSERT_EQ(ERROR_SUCCESS,
                key.Open(HKEY_CURRENT_USER, reg_key.data(), KEY_READ));
      EXPECT_TRUE(key.HasValue(file_handler_prog_id.data()));
    }
  }
#elif BUILDFLAG(IS_MAC)
  for (auto extension : expected_extensions) {
    const base::FilePath test_file_path =
        shortcut_override->chrome_apps_folder.GetPath().AppendASCII("test." +
                                                                    extension);
    const base::File test_file(test_file_path, base::File::FLAG_CREATE_ALWAYS |
                                                   base::File::FLAG_WRITE);
    const GURL test_file_url = net::FilePathToFileURL(test_file_path);
    EXPECT_EQ(u"Manifest with file handlers",
              shell_integration::GetApplicationNameForProtocol(test_file_url))
        << "The default app to open the file is wrong. "
        << "File extension: " + extension;
  }
  ASSERT_TRUE(shortcut_override->chrome_apps_folder.Delete());
#endif

  // Unistall the web app
  NavigateToURLAndWait(browser(), GURL(chrome::kChromeUIAppsURL));
  base::RunLoop run_loop_uninstall;
  WebAppProvider::GetForTest(profile())->install_finalizer().UninstallWebApp(
      app_id, webapps::WebappUninstallSource::kAppsPage,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
        run_loop_uninstall.Quit();
      }));
  run_loop_uninstall.Run();

#if BUILDFLAG(IS_WIN)
  // Check file associations after the web app is uninstalled.
  // Check that HKCU/Software Classes/<filext>/ doesn't have the ProgId.
  for (const auto& reg_key_prog_id : reg_key_prog_id_map) {
    ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                      reg_key_prog_id.first.data(), KEY_READ));
    EXPECT_FALSE(key.HasValue(reg_key_prog_id.second.data()));
  }
#endif
}

// TODO(crbug.com/1270961): Disabled because it is flaky on Mac.
#if BUILDFLAG(IS_MAC) && MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_11_0
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_FileHandler,
                       UserDenyFileHandlingPermission) {
  os_hooks_suppress_.reset();
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::unique_ptr<ScopedShortcutOverrideForTesting> shortcut_override =
      OverrideShortcutsForTesting(base::GetHomeDir());
  std::vector<std::string> expected_extensions{"bar", "baz", "foo", "foobar"};

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL app_url(embedded_test_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_with_file_handlers.json"));

  NavigateToURLAndWait(browser(), app_url);

  // Wait for OS hooks and installation to complete.
  chrome::SetAutoAcceptWebAppDialogForTesting(true, true);
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(base::BindLambdaForTesting(
      [&](const AppId& installed_app_id) { run_loop_install.Quit(); }));
  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  run_loop_install.Run();
  content::RunAllTasksUntilIdle();
  chrome::SetAutoAcceptWebAppDialogForTesting(false, false);

  // Simulate the user permanently denying file handling permission. Regression
  // test for crbug.com/1269387
  base::RunLoop run_loop_remove_file_handlers;
  PersistFileHandlersUserChoice(browser()->profile(), app_id, /*allowed=*/false,
                                run_loop_remove_file_handlers.QuitClosure());
  run_loop_remove_file_handlers.Run();

  for (auto extension : expected_extensions) {
    const base::FilePath test_file_path =
        shortcut_override->chrome_apps_folder.GetPath().AppendASCII("test." +
                                                                    extension);
    const base::File test_file(test_file_path, base::File::FLAG_CREATE_ALWAYS |
                                                   base::File::FLAG_WRITE);
    const GURL test_file_url = net::FilePathToFileURL(test_file_path);
    while (u"Manifest with file handlers" ==
           shell_integration::GetApplicationNameForProtocol(test_file_url)) {
      base::RunLoop delay_loop;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, delay_loop.QuitClosure(), base::Milliseconds(100));
      delay_loop.Run();
    }
  }
  ASSERT_TRUE(shortcut_override->chrome_apps_folder.Delete());

  // Unistall the web app
  NavigateToURLAndWait(browser(), GURL(chrome::kChromeUIAppsURL));
  base::RunLoop run_loop_uninstall;
  WebAppProvider::GetForTest(profile())->install_finalizer().UninstallWebApp(
      app_id, webapps::WebappUninstallSource::kAppsPage,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
        run_loop_uninstall.Quit();
      }));
  run_loop_uninstall.Run();
}
#endif  // BUILDFLAG(IS_MAC)
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, PRE_UninstallIncompleteUninstall) {
  auto* provider = WebAppProvider::GetForTest(profile());

  NavigateToURLAndWait(browser(), GetInstallableAppURL());

  // Wait for OS hooks and installation to complete and the app to launch.
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(base::BindLambdaForTesting(
      [&](const AppId& installed_app_id) { run_loop_install.Quit(); }));
  const AppId app_id = test::InstallPwaForCurrentUrl(browser());
  run_loop_install.Run();

  EXPECT_TRUE(provider->registrar().IsInstalled(app_id));
  EXPECT_EQ(provider->registrar().GetAppShortName(app_id),
            GetInstallableAppName());
  // This does NOT uninstall the web app, it just flags it for uninstall on
  // startup.
  {
    ScopedRegistryUpdate update(&provider->sync_bridge());
    WebApp* web_app = update->UpdateApp(app_id);
    ASSERT_TRUE(web_app);
    web_app->SetIsUninstalling(true);
  }
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, UninstallIncompleteUninstall) {
  auto* provider = WebAppProvider::GetForTest(profile());
  // The uninstall-on-startup code schedules tasks to uninstall flagged apps on
  // startup. For this test, either:
  // 1) The webapp was uninstalled during test startup, when it is waiting for
  //    the WebAppProvider to be ready, or
  // 2) It hasn't been uninstalled yet.
  // The test body here handles both cases, and ensures that the app has been
  // uninstalled.
  std::set<AppId> apps;
  for (const auto& web_app : provider->registrar().GetAppsIncludingStubs()) {
    EXPECT_TRUE(web_app.is_uninstalling());
    apps.insert(web_app.app_id());
  }
  EXPECT_TRUE(apps.size() == 0 || apps.size() == 1);
  if (apps.size() != 0) {
    WebAppTestUninstallObserver observer(profile());
    observer.BeginListeningAndWait(apps);
  }
  // TODO(dmurph): Remove AppSet, it's too hard to use.
  int app_count = 0;
  const web_app::WebAppRegistrar::AppSet& app_set =
      provider->registrar().GetAppsIncludingStubs();
  for (auto it = app_set.begin(); it != app_set.end(); ++it) {
    ++app_count;
  }
  EXPECT_EQ(app_count, 0);
}

// Verifies the behavior of the App/site settings link in the page info bubble.
class WebAppBrowserTest_PageInfoManagementLink
    : public WebAppBrowserTest,
      public testing::WithParamInterface<WebAppSettingsState> {
 public:
  WebAppBrowserTest_PageInfoManagementLink() {
    file_handling_feature_list_.InitWithFeatureState(
        blink::features::kFileHandlingAPI,
        GetParam() == WebAppSettingsState::kFileHandlingAlsoEnabled);
#if !BUILDFLAG(IS_CHROMEOS)
    web_app_settings_feature_list_.InitWithFeatureState(
        features::kDesktopPWAsWebAppSettingsPage,
        GetParam() != WebAppSettingsState::kNeitherEnabled);
#endif
  }

  bool ShowingAppManagementLink(Browser* browser) {
    int unused_id, unused_id2;
    return GetLabelIdsForAppManagementLinkInPageInfo(
        browser->tab_strip_model()->GetActiveWebContents(), &unused_id,
        &unused_id2);
  }

  static bool HasAppSettingsPage() {
#if BUILDFLAG(IS_CHROMEOS)
    return true;
#else
    return base::FeatureList::IsEnabled(
        features::kDesktopPWAsWebAppSettingsPage);
#endif
  }

  static bool ShowsAppSettingsLinkInTabbedBrowser() {
    return HasAppSettingsPage() &&
           base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI);
  }

 private:
  base::test::ScopedFeatureList file_handling_feature_list_;
  base::test::ScopedFeatureList web_app_settings_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest_PageInfoManagementLink, Reparenting) {
  const GURL app_url = GetSecureAppURL();
  InstallPWA(app_url);

  NavigateToURLAndWait(browser(), app_url);
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(tab_contents->GetLastCommittedURL(), app_url);

  // After a normal (e.g. typed) navigation, should not show the app settings
  // link.
  EXPECT_FALSE(ShowingAppManagementLink(browser()));
  // Reparent into app browser window.
  Browser* const app_browser = ReparentWebAppForActiveTab(browser());
  // The leftover tab in the tabbed browser window should not be appy.
  EXPECT_FALSE(ShowingAppManagementLink(browser()));
  // After reparenting into an app browser, should show the app settings link.
  EXPECT_EQ(HasAppSettingsPage(), ShowingAppManagementLink(app_browser));

  // Move back into tabbed browser: should keep showing the app settings link.
  Browser* tabbed_browser = chrome::OpenInChrome(app_browser);
  EXPECT_EQ(ShowsAppSettingsLinkInTabbedBrowser(),
            ShowingAppManagementLink(tabbed_browser));
}

// Verifies behavior when an app window is opened by navigating with
// `open_pwa_window_if_possible` set to true.
IN_PROC_BROWSER_TEST_P(WebAppBrowserTest_PageInfoManagementLink,
                       OpenAppWindowIfPossible) {
  const GURL app_url = GetSecureAppURL();
  InstallPWA(app_url);

  NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_LINK);
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.open_pwa_window_if_possible = true;
  ui_test_utils::NavigateToURL(&params);

  EXPECT_NE(browser(), params.browser);
  EXPECT_FALSE(params.browser->is_type_normal());
  EXPECT_TRUE(params.browser->is_type_app());
  EXPECT_TRUE(params.browser->is_trusted_source());

  EXPECT_EQ(HasAppSettingsPage(), ShowingAppManagementLink(params.browser));
}

IN_PROC_BROWSER_TEST_P(WebAppBrowserTest_PageInfoManagementLink, LaunchAsTab) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);

  // A non appy tab is showing, so the app settings link should not be visible.
  EXPECT_FALSE(ShowingAppManagementLink(browser()));

  // *Launch* the app as a tab in a normal browser window. The app settings link
  // should be visible.
  Browser* tabbed_browser = LaunchBrowserForWebAppInTab(app_id);
  EXPECT_EQ(browser(), tabbed_browser);
  EXPECT_EQ(ShowsAppSettingsLinkInTabbedBrowser(),
            ShowingAppManagementLink(tabbed_browser));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppBrowserTest_PageInfoManagementLink,
    ::testing::Values(WebAppSettingsState::kNeitherEnabled,
                      WebAppSettingsState::kOnlyWebAppSettingsEnabled,
                      WebAppSettingsState::kFileHandlingAlsoEnabled));

}  // namespace web_app
