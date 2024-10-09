// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/devtools/protocol/browser_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/background_color_change_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/base_window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/color_helpers.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "components/metrics/structured/test/test_structured_metrics_recorder.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
// TODO(crbug.com/40147906): Enable gn check once it handles conditional
// includes
#include "chromeos/constants/chromeos_features.h"
#include "components/metrics/structured/structured_events.h"  // nogncheck
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/strings/strcat.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/windows_version.h"
#include "chrome/browser/web_applications/os_integration/web_app_handler_registration_utils_win.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu_win.h"
#include "chrome/browser/win/jumplist_updater.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/shell_util.h"
#endif

namespace webapps {
enum class InstallResultCode;
}

namespace {

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;
#endif

constexpr const char kExampleURL[] = "http://example.org/";
constexpr const char16_t kExampleURL16[] = u"http://example.org/";
constexpr const char kExampleManifestURL[] = "http://example.org/manifest";

constexpr char kLaunchWebAppDisplayModeHistogram[] = "Launch.WebAppDisplayMode";

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

// Waits for a Browser to be set to last active.
class BrowserActivationWaiter : public BrowserListObserver {
 public:
  explicit BrowserActivationWaiter(const Browser* browser) : browser_(browser) {
    BrowserList::AddObserver(this);
    // When the active browser closes, the next "last active browser" in the
    // BrowserList might not be immediately activated. So we need to wait for
    // the "last active browser" to actually be active.
    if (chrome::FindLastActive() == browser_ &&
        browser_->window()->IsActive()) {
      observed_ = true;
    }
  }

  BrowserActivationWaiter(const BrowserActivationWaiter&) = delete;
  BrowserActivationWaiter& operator=(const BrowserActivationWaiter&) = delete;

  ~BrowserActivationWaiter() override { BrowserList::RemoveObserver(this); }

  // Runs a message loop until the `browser_` supplied to the constructor is
  // activated, or returns immediately if `browser_` has already become active.
  // Should only be called once.
  void WaitForActivation() {
    if (observed_) {
      return;
    }
    run_loop_.Run();
  }

 private:
  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override {
    if (browser != browser_) {
      return;
    }

    observed_ = true;
    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }

  const raw_ptr<const Browser> browser_;
  bool observed_ = false;
  base::RunLoop run_loop_;
};

// Returns whether `window` roughly matches expected `bounds`.
bool CheckForBounds(ui::BaseWindow* window, const gfx::Rect& bounds) {
  const gfx::Rect& actual = window->GetBounds();
  // Tolerances were empirically derived, and should be reduced.
  constexpr int kOffsetTolerance = 5, kSizeTolerance = 50;
  return (bounds.origin() - actual.origin()).Length() < kOffsetTolerance &&
         std::abs(bounds.width() - actual.width()) < kSizeTolerance &&
         std::abs(bounds.height() - actual.height()) < kSizeTolerance;
}

// Waits until `window` roughly matches expected `bounds`, or fails after 1s.
bool WaitForBounds(ui::BaseWindow* window, const gfx::Rect& bounds) {
  ui_test_utils::CheckWaiter(
      base::BindRepeating(CheckForBounds, base::Unretained(window), bounds),
      /*expected=*/true, base::Seconds(1))
      .Wait();
  return CheckForBounds(window, bounds);
}

}  // namespace

namespace web_app {

using ::base::BucketsAre;

class WebAppBrowserTest : public WebAppBrowserTestBase {
 public:
  GURL GetSecureAppURL() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  GURL GetURLForPath(const std::string& path) {
    return https_server()->GetURL("app.com", path);
  }

  bool HasMinimalUiButtons(DisplayMode display_mode,
                           std::optional<DisplayMode> display_override_mode,
                           bool open_as_window) {
    static int index = 0;

    base::HistogramTester tester;
    const GURL app_url = https_server()->GetURL(
        base::StringPrintf("/web_apps/basic.html?index=%d", index++));
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
    web_app_info->scope = app_url;
    web_app_info->display_mode = display_mode;
    web_app_info->user_display_mode = open_as_window
                                          ? mojom::UserDisplayMode::kStandalone
                                          : mojom::UserDisplayMode::kBrowser;
    if (display_override_mode) {
      web_app_info->display_override.push_back(*display_override_mode);
    }

    webapps::AppId app_id = InstallWebApp(std::move(web_app_info));
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

    const bool result = app_browser->app_controller()->HasMinimalUiButtons();
    EXPECT_EQ(
        result,
        EvalJs(web_contents,
               "window.matchMedia('(display-mode: minimal-ui)').matches"));
    CloseAndWait(app_browser);
    UninstallWebApp(app_id);

    return result;
  }
};

// A dedicated test fixture for Borderless, which requires a command
// line switch to enable manifest parsing.
class WebAppBrowserTest_Borderless : public WebAppBrowserTest {
 public:
  WebAppBrowserTest_Borderless() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppBorderless};
};

// A dedicated test fixture for tabbed display override, which requires a
// command line switch to enable manifest parsing.
class WebAppBrowserTest_Tabbed : public WebAppBrowserTest {
 public:
  WebAppBrowserTest_Tabbed() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kDesktopPWAsTabStrip};
};

using WebAppBrowserTest_DetailedInstallDialog = WebAppBrowserTest;

#if BUILDFLAG(IS_WIN)
using WebAppBrowserTest_ShortcutMenu = WebAppBrowserTest;
#endif

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ThemeColor) {
  GURL start_url = https_server()->GetURL("/web_apps/no_manifest.html");
  {
    const SkColor theme_color = SkColorSetA(SK_ColorBLUE, 0xF0);
    blink::mojom::Manifest manifest;
    manifest.manifest_url = GURL(kExampleManifestURL);
    manifest.start_url = start_url;
    manifest.id = GenerateManifestIdFromStartUrlOnly(start_url);
    manifest.scope = manifest.start_url.GetWithoutFilename();
    manifest.has_theme_color = true;
    manifest.theme_color = theme_color;
    auto web_app_info =
        std::make_unique<WebAppInstallInfo>(manifest.id, manifest.start_url);
    web_app::UpdateWebAppInfoFromManifest(manifest, web_app_info.get());

    webapps::AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* app_browser = LaunchWebAppBrowser(app_id);

    EXPECT_EQ(GetAppIdFromApplicationName(app_browser->app_name()), app_id);
    EXPECT_EQ(SkColorSetA(theme_color, SK_AlphaOPAQUE),
              app_browser->app_controller()->GetThemeColor());
    test::UninstallWebApp(profile(), app_id);
  }
  {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->theme_color = std::optional<SkColor>();
    webapps::AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* app_browser = LaunchWebAppBrowser(app_id);

    EXPECT_EQ(GetAppIdFromApplicationName(app_browser->app_name()), app_id);
    EXPECT_EQ(std::nullopt, app_browser->app_controller()->GetThemeColor());
  }
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, BackgroundColor) {
  blink::mojom::Manifest manifest;
  manifest.manifest_url = GURL(kExampleManifestURL);
  manifest.start_url = GURL(kExampleURL);
  manifest.id = GenerateManifestIdFromStartUrlOnly(manifest.start_url);
  manifest.scope = GURL(kExampleURL);
  manifest.has_background_color = true;
  manifest.background_color = SkColorSetA(SK_ColorBLUE, 0xF0);
  auto web_app_info =
      std::make_unique<WebAppInstallInfo>(manifest.id, manifest.start_url);
  web_app::UpdateWebAppInfoFromManifest(manifest, web_app_info.get());
  webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

  auto* provider = WebAppProvider::GetForTest(profile());
  EXPECT_EQ(provider->registrar_unsafe().GetAppBackgroundColor(app_id),
            SK_ColorBLUE);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ShortcutBackgroundColor) {
  const GURL app_url = https_server()->GetURL("/banners/background-color.html");
  const webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
  auto* provider = WebAppProvider::GetForTest(profile());

  EXPECT_EQ(provider->registrar_unsafe().GetAppBackgroundColor(app_id),
            SK_ColorBLUE);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ManifestWithColor) {
  const GURL app_url =
      https_server()->GetURL("/banners/no-sw-with-colors.html");
  const webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
  auto* provider = WebAppProvider::GetForTest(profile());

  EXPECT_EQ(provider->registrar_unsafe().GetAppBackgroundColor(app_id),
            SK_ColorYELLOW);
  EXPECT_EQ(provider->registrar_unsafe().GetAppThemeColor(app_id),
            SK_ColorGREEN);
}

// Also see BackgroundColorChangeSystemWebAppBrowserTest.BackgroundColorChange
// below.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, BackgroundColorChange) {
  const GURL app_url = GetSecureAppURL();
  auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->theme_color = SK_ColorWHITE;
  web_app_info->dark_mode_theme_color = SK_ColorBLACK;
  web_app_info->background_color = SK_ColorWHITE;
  web_app_info->dark_mode_background_color = SK_ColorBLACK;

  const webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

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

  // Changing background color should update the active tab color.
  {
    content::BackgroundColorChangeWaiter waiter(web_contents);
    EXPECT_TRUE(content::ExecJs(
        web_contents, "document.body.style.backgroundColor = 'cyan';"));
    waiter.Wait();
    EXPECT_EQ(app_browser->app_controller()->GetBackgroundColor().value(),
              SK_ColorCYAN);
    SkColor active_tab_color;
    app_browser->app_controller()->GetThemeSupplier()->GetColor(
        ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE,
        &active_tab_color);
    EXPECT_EQ(active_tab_color, SK_ColorCYAN);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class ColorSystemWebAppBrowserTest : public WebAppBrowserTest {
 public:
  ColorSystemWebAppBrowserTest() {
    system_web_app_installation_ =
        ash::TestSystemWebAppInstallation::SetUpAppWithColors(
            /*theme_color=*/SK_ColorWHITE,
            /*dark_mode_theme_color=*/SK_ColorBLACK,
            /*background_color=*/SK_ColorWHITE,
            /*dark_mode_background_color=*/SK_ColorBLACK);
  }

  // Installs the web app under test, blocking until installation is complete,
  // and returning the `webapps::AppId` for the installed web app.
  webapps::AppId WaitForSwaInstall() {
    system_web_app_installation_->WaitForAppInstall();
    return system_web_app_installation_->GetAppId();
  }

 protected:
  std::unique_ptr<ash::TestSystemWebAppInstallation>
      system_web_app_installation_;
};

class DynamicColorSystemWebAppBrowserTest
    : public ColorSystemWebAppBrowserTest,
      public testing::WithParamInterface</*use_system_theme_color=*/bool> {
 public:
  DynamicColorSystemWebAppBrowserTest() {
    auto* delegate = static_cast<ash::UnittestingSystemAppDelegate*>(
        system_web_app_installation_->GetDelegate());

    delegate->SetUseSystemThemeColor(GetParam());
  }

  // Returns whether the web app under test wants to use a system sourced theme
  // color.
  bool UseSystemThemeColor() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kJelly};
};

INSTANTIATE_TEST_SUITE_P(All,
                         DynamicColorSystemWebAppBrowserTest,
                         /*use_system_theme_color=*/::testing::Bool(),
                         [](const testing::TestParamInfo<
                             /*use_system_theme_color=*/bool>& info) {
                           return info.param ? "WithUseSystemThemeColor"
                                             : "WithoutUseSystemThemeColor";
                         });

// TODO(crbug.com/364153631): Fix flaky test on ChromeOS
IN_PROC_BROWSER_TEST_P(DynamicColorSystemWebAppBrowserTest, DISABLED_Colors) {
  const webapps::AppId app_id = WaitForSwaInstall();
  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  auto* app_controller = app_browser->app_controller();
  auto theme_color = app_controller->GetThemeColor().value();
  auto bg_color = app_controller->GetBackgroundColor().value();
  if (UseSystemThemeColor()) {
    // Ensure app controller is pulling the color from the OS.
    EXPECT_EQ(theme_color, ash::GetSystemThemeColor());
    // TODO(crbug.com/359650452): Determine desired behavior here. This use to
    // test for ash::GetSystemBackgroundColor(), but that becomes overridden
    // after the web contents fully loads.
    EXPECT_TRUE(bg_color == SK_ColorWHITE || bg_color == SK_ColorBLACK);
  } else {
    // If SWA has opted out, theme and bg color should default to white or black
    // depending on launch context.
    EXPECT_TRUE(theme_color == SK_ColorWHITE || theme_color == SK_ColorBLACK);
    EXPECT_TRUE(bg_color == SK_ColorWHITE || bg_color == SK_ColorBLACK);
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// This tests that we don't crash when launching a PWA window with an
// autogenerated user theme set.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, AutoGeneratedUserThemeCrash) {
  ThemeServiceFactory::GetForProfile(browser()->profile())
      ->BuildAutogeneratedThemeFromColor(SK_ColorBLUE);

  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(GURL(kExampleURL));
  webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

  LaunchWebAppBrowser(app_id);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, DiyAppLaunchHistogram) {
  base::HistogramTester tester;

  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(GURL(kExampleURL));
  web_app_info->is_diy_app = true;
  webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

  Browser* app_browser = LaunchWebAppBrowser(app_id);
  EXPECT_NE(app_browser, nullptr);
  EXPECT_THAT(tester.GetAllSamples("Launch.WebApp.DiyOrCrafted"),
              BucketsAre(base::Bucket(LaunchedAppType::kDiy, 1)));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, CraftedAppLaunchHistogram) {
  base::HistogramTester tester;

  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(GURL(kExampleURL));
  web_app_info->is_diy_app = false;
  webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

  Browser* app_browser = LaunchWebAppBrowser(app_id);
  EXPECT_NE(app_browser, nullptr);
  EXPECT_THAT(tester.GetAllSamples("Launch.WebApp.DiyOrCrafted"),
              BucketsAre(base::Bucket(LaunchedAppType::kCrafted, 1)));
}

// Check the 'Open in Chrome' menu button for web app windows.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, OpenInChrome) {
  const GURL app_url(kExampleURL);
  const webapps::AppId app_id = InstallPWA(app_url);

  {
    Browser* const app_browser = LaunchWebAppBrowser(app_id);

    EXPECT_EQ(1, app_browser->tab_strip_model()->count());
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
    ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

    ui_test_utils::BrowserChangeObserver on_close(
        app_browser,
        ui_test_utils::BrowserChangeObserver::ChangeType::kRemoved);
    chrome::ExecuteCommand(app_browser, IDC_OPEN_IN_CHROME);

    // The browser frame is closed next event loop so it's still safe to access
    // here.
    EXPECT_EQ(0, app_browser->tab_strip_model()->count());

    // Wait until the browser actually gets closed. This invalidates
    // |app_browser|.
    on_close.Wait();
    EXPECT_EQ(2, browser()->tab_strip_model()->count());
    EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
    EXPECT_EQ(
        app_url,
        browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
  }

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
  const webapps::AppId app_id = InstallPWA(app_url);
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
  const webapps::AppId app_id = InstallPWA(app_url);
  auto* provider = WebAppProvider::GetForTest(profile());

  // last_launch_time is not set before launch
  EXPECT_TRUE(
      provider->registrar_unsafe().GetAppLastLaunchTime(app_id).is_null());

  auto before_launch = base::Time::Now();
  LaunchWebAppBrowser(app_id);

  EXPECT_TRUE(provider->registrar_unsafe().GetAppLastLaunchTime(app_id) >=
              before_launch);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, WithMinimalUiButtons) {
  EXPECT_TRUE(HasMinimalUiButtons(DisplayMode::kBrowser, std::nullopt,
                                  /*open_as_window=*/true));
  EXPECT_TRUE(HasMinimalUiButtons(DisplayMode::kMinimalUi, std::nullopt,
                                  /*open_as_window=*/true));

  EXPECT_TRUE(HasMinimalUiButtons(DisplayMode::kBrowser, std::nullopt,
                                  /*open_as_window=*/false));
  EXPECT_TRUE(HasMinimalUiButtons(DisplayMode::kMinimalUi, std::nullopt,
                                  /*open_as_window=*/false));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, WithoutMinimalUiButtons) {
  EXPECT_FALSE(HasMinimalUiButtons(DisplayMode::kStandalone, std::nullopt,
                                   /*open_as_window=*/true));
  EXPECT_FALSE(HasMinimalUiButtons(DisplayMode::kFullscreen, std::nullopt,
                                   /*open_as_window=*/true));

  EXPECT_FALSE(HasMinimalUiButtons(DisplayMode::kStandalone, std::nullopt,
                                   /*open_as_window=*/false));
  EXPECT_FALSE(HasMinimalUiButtons(DisplayMode::kFullscreen, std::nullopt,
                                   /*open_as_window=*/false));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, DisplayOverride) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_display_override.json");
  NavigateViaLinkClickToURLAndWait(browser(), test_url);

  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());

  std::vector<DisplayMode> app_display_mode_override =
      provider->registrar_unsafe().GetAppDisplayModeOverride(app_id);

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
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);
  NavigateViaLinkClickToURLAndWait(app_browser, app_url);
  ASSERT_TRUE(app_browser->app_controller());
  NavigateAndCheckForToolbar(app_browser, GURL(kExampleURL), true);
}

// Tests that desktop PWAs open links in a new tab at the end of the tabstrip of
// the last active browser.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, DesktopPWAsOpenLinksInNewTab) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);
  NavigateViaLinkClickToURLAndWait(app_browser, app_url);
  ui_test_utils::WaitForBrowserSetLastActive(app_browser);
  ASSERT_TRUE(app_browser->app_controller());

  EXPECT_EQ(chrome::GetTotalBrowserCount(), 2u);
  Browser* browser2 = ui_test_utils::OpenNewEmptyWindowAndWaitUntilActivated(
      app_browser->profile());
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
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  EXPECT_TRUE(AppBrowserController::IsWebApp(app_browser));
  NavigateViaLinkClickToURLAndWait(app_browser, app_url);

  const gfx::Rect bounds = gfx::Rect(50, 50, 550, 500);
  ui_test_utils::SetAndWaitForBounds(*app_browser, bounds);
  CloseAndWait(app_browser);

  Browser* const new_browser = LaunchWebAppBrowser(app_id);
  EXPECT_EQ(new_browser->window()->GetBounds(), bounds);
}

// Tests that using window.open to create a popup window out of scope results in
// a correctly sized window.
// TODO(crbug.com/40781381): Stabilize the test.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_OffScopePWAPopupsHaveCorrectSize \
  DISABLED_OffScopePWAPopupsHaveCorrectSize
#else
#define MAYBE_OffScopePWAPopupsHaveCorrectSize OffScopePWAPopupsHaveCorrectSize
#endif
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       MAYBE_OffScopePWAPopupsHaveCorrectSize) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
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
// TODO(crbug.com/40781381): Stabilize the test.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_InScopePWAPopupsHaveCorrectSize \
  DISABLED_InScopePWAPopupsHaveCorrectSize
#else
#define MAYBE_InScopePWAPopupsHaveCorrectSize InScopePWAPopupsHaveCorrectSize
#endif
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       MAYBE_InScopePWAPopupsHaveCorrectSize) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
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

// Test navigating to an out of scope url on the same origin causes the url
// to be shown to the user.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       LocationBarIsVisibleOffScopeOnSameOrigin) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  // Toolbar should not be visible in the app.
  ASSERT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // The installed PWA's scope is app.com:{PORT}/ssl,
  // so app.com:{PORT}/accessibility_fail.html is out of scope.
  const GURL out_of_scope = GetURLForPath("/accessibility_fail.html");
  NavigateViaLinkClickToURLAndWait(app_browser, out_of_scope);

  // Location should be visible off scope.
  ASSERT_TRUE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, UpgradeWithoutCustomTabBar) {
  const GURL secure_app_url =
      https_server()->GetURL("app.site.test", "/empty.html");
  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpScheme);
  const GURL app_url = secure_app_url.ReplaceComponents(rep);

  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  NavigateViaLinkClickToURLAndWait(app_browser, secure_app_url);

  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  const GURL off_origin_url =
      https_server()->GetURL("example.org", "/empty.html");
  NavigateViaLinkClickToURLAndWait(app_browser, off_origin_url);
  EXPECT_EQ(app_browser->app_controller()->ShouldShowCustomTabBar(), true);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, OverscrollEnabled) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
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
  const webapps::AppId app_id = InstallPWA(app_url);
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
  const webapps::AppId app_id = InstallPWA(app_url);

  Browser* const incognito_browser = OpenURLOffTheRecord(profile(), app_url);
  auto app_menu_model =
      std::make_unique<AppMenuModel>(nullptr, incognito_browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  size_t index = 0;
  ASSERT_TRUE(app_menu_model->GetModelAndIndexForCommandId(
      IDC_OPEN_IN_PWA_WINDOW, &model, &index));
  EXPECT_FALSE(model->IsEnabledAt(index));
}

// Tests that web app menus don't crash when no tabs are selected.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, NoTabSelectedMenuCrash) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  app_browser->tab_strip_model()->CloseAllTabs();
  auto app_menu_model = std::make_unique<WebAppMenuModel>(
      /*provider=*/nullptr, app_browser);
  app_menu_model->Init();
}

// Tests that PWA menus have an uninstall option.
// TODO(crbug.com/40805374): Flaky on mac arm64.
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
#define MAYBE_UninstallMenuOption DISABLED_UninstallMenuOption
#else
#define MAYBE_UninstallMenuOption UninstallMenuOption
#endif
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, MAYBE_UninstallMenuOption) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  auto app_menu_model = std::make_unique<WebAppMenuModel>(
      /*provider=*/nullptr, app_browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  size_t index = 0;
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
  NavigateViaLinkClickToURLAndWait(incognito_browser, GetInstallableAppURL());

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
        tab_contents->GetPrimaryMainFrame());
    tab_contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(1);
    crash_observer.WaitUntilDeleted();
  }
  ASSERT_TRUE(tab_contents->IsCrashed());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kDisabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), kNotPresent);
}

// Tests that an installed PWA is not used when out of scope by one path level.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, MenuOptionsOutsideInstalledPwaScope) {
  NavigateViaLinkClickToURLAndWait(
      browser(),
      https_server()->GetURL("/banners/scope_is_start_url/index.html"));
  test::InstallPwaForCurrentUrl(browser());

  // Open a page that is one directory up from the installed PWA.
  Browser* const new_browser = NavigateInNewWindowAndAwaitInstallabilityCheck(
      https_server()->GetURL("/banners/no_manifest_test_page.html"));

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  AppMenuCommandState install_pwa_state =
      base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)
          ? kEnabled
          : kNotPresent;
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser),
            install_pwa_state);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, InstallInstallableSite) {
  base::Time before_install_time = base::Time::Now();
  base::UserActionTester user_action_tester;
  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());

  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());
  EXPECT_EQ(provider->registrar_unsafe().GetAppShortName(app_id),
            GetInstallableAppName());

  // Installed PWAs should launch in their own window.
  EXPECT_EQ(provider->registrar_unsafe().GetAppUserDisplayMode(app_id),
            web_app::mojom::UserDisplayMode::kStandalone);

  // Installed PWAs should have install time set.
  EXPECT_TRUE(provider->registrar_unsafe().GetAppFirstInstallTime(app_id) >=
              before_install_time);

  EXPECT_EQ(1, user_action_tester.GetActionCount("InstallWebAppFromMenu"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("CreateShortcut"));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Apps on Chrome OS should not be pinned after install.
  EXPECT_FALSE(ChromeShelfController::instance()->IsAppPinned(app_id));
#endif
}

#if BUILDFLAG(IS_CHROMEOS)
class WebAppBrowserCrOSEventsTest : public WebAppBrowserTest {
 protected:
  GURL GetUrlWithScreenshots() {
    return https_server()->GetURL(
        "/banners/manifest_test_page_screenshots.html");
  }
};

IN_PROC_BROWSER_TEST_F(WebAppBrowserCrOSEventsTest,
                       CorrectEventsOnBrowserTabPwaInstall) {
  auto test_recorder =
      std::make_unique<metrics::structured::TestStructuredMetricsRecorder>();
  test_recorder->Initialize();

  base::Time before_install_time = base::Time::Now();
  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());

  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());
  EXPECT_EQ(provider->registrar_unsafe().GetAppShortName(app_id),
            GetInstallableAppName());

  // Installed PWAs should launch in their own window.
  EXPECT_EQ(provider->registrar_unsafe().GetAppUserDisplayMode(app_id),
            web_app::mojom::UserDisplayMode::kStandalone);

  // Installed PWAs should have install time set.
  EXPECT_TRUE(provider->registrar_unsafe().GetAppFirstInstallTime(app_id) >=
              before_install_time);

  const std::vector<metrics::structured::Event>& events =
      test_recorder->GetEvents();
  ASSERT_EQ(events.size(), 3U);

  // Events that should be recorded will be ClickInstallAppFromMenu ->
  // AppInstallDialogShown -> AppInstallDialogResult (Accepted).
  cros_events::AppDiscovery_Browser_ClickInstallAppFromMenu event1;
  event1.SetAppId(app_id);

  EXPECT_EQ(events[0].event_name(), event1.event_name());
  EXPECT_EQ(events[0].metric_values(), event1.metric_values());

  cros_events::AppDiscovery_Browser_AppInstallDialogShown event2;
  event2.SetAppId(app_id);

  EXPECT_EQ(events[1].event_name(), event2.event_name());
  EXPECT_EQ(events[1].metric_values(), event2.metric_values());

  cros_events::AppDiscovery_Browser_AppInstallDialogResult event3;
  event3.SetAppId(app_id).SetWebAppInstallStatus(
      static_cast<int64_t>(web_app::WebAppInstallStatus::kAccepted));

  EXPECT_EQ(events[2].event_name(), event3.event_name());
  EXPECT_EQ(events[2].metric_values(), event3.metric_values());
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserCrOSEventsTest,
                       DetailedInstallDialogSupport) {
  auto test_recorder =
      std::make_unique<metrics::structured::TestStructuredMetricsRecorder>();
  test_recorder->Initialize();

  base::Time before_install_time = base::Time::Now();
  NavigateViaLinkClickToURLAndWait(browser(), GetUrlWithScreenshots());

  // Wait until the screenshots are in the app banner manager.
  webapps::AppBannerManager* app_banner_manager =
      webapps::AppBannerManager::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    std::optional<webapps::WebAppBannerData> banner_data =
        app_banner_manager->GetCurrentWebAppBannerData();
    return banner_data.has_value() && !banner_data->screenshots.empty();
  })) << "Screenshots were never loaded for current tab.";

  // Wait for the detailed install dialog to show up post install, and accept
  // it.
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppDetailedInstallDialog");

  WebAppTestInstallWithOsHooksObserver observer(browser()->profile());
  observer.BeginListening();
  WebAppProvider* provider = WebAppProvider::GetForTest(browser()->profile());
  CHECK(chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA));
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  EXPECT_NE(widget, nullptr);
  views::test::AcceptDialog(widget);
  const webapps::AppId app_id = observer.Wait();

  EXPECT_EQ(provider->registrar_unsafe().GetAppShortName(app_id),
            "PWA Bottom Sheet");

  // Installed PWAs should launch in their own window.
  EXPECT_EQ(provider->registrar_unsafe().GetAppUserDisplayMode(app_id),
            web_app::mojom::UserDisplayMode::kStandalone);

  // Installed PWAs should have install time set.
  EXPECT_TRUE(provider->registrar_unsafe().GetAppFirstInstallTime(app_id) >=
              before_install_time);

  const std::vector<metrics::structured::Event>& events =
      test_recorder->GetEvents();
  ASSERT_EQ(events.size(), 3U);

  // Events that should be recorded will be ClickInstallAppFromMenu ->
  // AppInstallDialogShown -> AppInstallDialogResult (Accepted).
  cros_events::AppDiscovery_Browser_ClickInstallAppFromMenu event1;
  event1.SetAppId(app_id);

  EXPECT_EQ(events[0].event_name(), event1.event_name());
  EXPECT_EQ(events[0].metric_values(), event1.metric_values());

  cros_events::AppDiscovery_Browser_AppInstallDialogShown event2;
  event2.SetAppId(app_id);

  EXPECT_EQ(events[1].event_name(), event2.event_name());
  EXPECT_EQ(events[1].metric_values(), event2.metric_values());

  cros_events::AppDiscovery_Browser_AppInstallDialogResult event3;
  event3.SetAppId(app_id).SetWebAppInstallStatus(
      static_cast<int64_t>(web_app::WebAppInstallStatus::kAccepted));

  EXPECT_EQ(events[2].event_name(), event3.event_name());
  EXPECT_EQ(events[2].metric_values(), event3.metric_values());
}
#endif

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, CanInstallOverBrowserTabPwa) {
  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());
  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());

  // Change display mode to open in tab.
  auto* provider = WebAppProvider::GetForTest(profile());
  provider->sync_bridge_unsafe().SetAppUserDisplayModeForTesting(
      app_id, web_app::mojom::UserDisplayMode::kBrowser);

  Browser* const new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, CannotInstallOverWindowPwa) {
  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());
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
  webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);

  // Change display mode to open in tab.
  auto* provider = WebAppProvider::GetForTest(profile());
  provider->sync_bridge_unsafe().SetAppUserDisplayModeForTesting(
      app_id, web_app::mojom::UserDisplayMode::kBrowser);

  NavigateViaLinkClickToURLAndWait(browser(), app_url);
  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kEnabled);

  // Even though the app doesn't meet promotability criteria (manifest has
  // display:browser), the installation option should still show up as `Install
  // Page as App`.
  AppMenuCommandState install_pwa_state =
      base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)
          ? kEnabled
          : kNotPresent;
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()),
            install_pwa_state);
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

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_DetailedInstallDialog,
                       OpenDetailedInstallDialogOnlyOnce) {
  base::UserActionTester user_action_tester;
  NavigateViaLinkClickToURLAndWait(
      browser(),
      https_server()->GetURL("/banners/manifest_test_page_screenshots.html"));

  WebAppTestInstallObserver observer(profile());
  // The IDC_INSTALL_PWA is executed twice, but the dialog
  // must be shown only once.
  ASSERT_TRUE(chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA));
  ASSERT_TRUE(chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA));

  EXPECT_EQ(1u, provider()
                    .command_manager()
                    .GetCommandsInstallingForWebContentsForTesting());
}

// Test that a second web app window launches with similar size to the first.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, SecondWindowSizeMatches) {
  const webapps::AppId app_id = InstallPWA(GURL(kExampleURL));
  Browser* first_browser = LaunchWebAppBrowserAndWait(app_id);
  const gfx::Size first_size = first_browser->window()->GetBounds().size();
  Browser* second_browser = LaunchWebAppBrowserAndWait(app_id);
  const gfx::Size second_size = second_browser->window()->GetBounds().size();
  constexpr int kTolerance = 50;  // Empirically derived, should be reduced.
  EXPECT_LT(std::abs(first_size.width() - second_size.width()), kTolerance);
  EXPECT_LT(std::abs(first_size.height() - second_size.height()), kTolerance);
}

// Test that a second web app window launches with bounds offset from the first.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, SecondWindowOffset) {
  const webapps::AppId app_id = InstallPWA(GURL(kExampleURL));
  Browser* first_browser = LaunchWebAppBrowserAndWait(app_id);
  Browser* second_browser = LaunchWebAppBrowserAndWait(app_id);
  EXPECT_NE(first_browser->window()->GetBounds().OffsetFromOrigin(),
            second_browser->window()->GetBounds().OffsetFromOrigin());
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, SetBounds) {
  const webapps::AppId app_id = InstallPWA(GURL(kExampleURL));
  Browser* browser = LaunchWebAppBrowserAndWait(app_id);
  ui::BaseWindow* window = browser->window();
  const gfx::Rect bounds = gfx::Rect(50, 50, 550, 500);
  ui_test_utils::SetAndWaitForBounds(*browser, bounds);
  // Expect that the window bounds roughly match the requested bounds.
  EXPECT_TRUE(WaitForBounds(window, bounds))
      << window->GetBounds().ToString() << " != " << bounds.ToString();
}

// Test that offsets for newly launched web app windows are clamped on-screen.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, WindowOffsetsClampedToScreen) {
  const webapps::AppId app_id = InstallPWA(GURL(kExampleURL));
  Browser* browser = LaunchWebAppBrowserAndWait(app_id);
  ui::BaseWindow* window = browser->window();
  gfx::Rect bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  // Make the window fill the display, so subsequent new windows quickly hit the
  // edge of the screen when offset.
  ui_test_utils::SetAndWaitForBounds(*browser, bounds);
  ASSERT_TRUE(WaitForBounds(window, bounds))
      << window->GetBounds().ToString() << " != " << bounds.ToString();

  // Open windows until they yield bounds that match the prior window's bounds,
  // which happens if windows start stacking at the bottom right of the screen.
  bool bounds_match = false;
  std::vector<ui::BaseWindow*> windows = {window};
  while (windows.size() < 10 && !bounds_match) {
    window = LaunchWebAppBrowserAndWait(app_id)->window();
    bounds_match |= CheckForBounds(window, windows.back()->GetBounds());
    windows.push_back(window);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  // On-screen clamping is not strictly enforced on these platforms.
  GTEST_SKIP() << "Skipping bounds stacking check on incompatible platforms";
#endif

  EXPECT_TRUE(bounds_match)
      << windows.back()->GetBounds().ToString()
      << " != " << (*std::prev(windows.end(), 2))->GetBounds().ToString();
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest,
                       CannotUninstallPolicyWebAppAfterUserInstall) {
  GURL install_url = GetInstallableAppURL();
  ExternalInstallOptions options = CreateInstallOptions(install_url);
  options.install_source = ExternalInstallSource::kExternalPolicy;
  ExternallyManagedAppManagerInstall(profile(), options);

  auto* provider = WebAppProvider::GetForTest(browser()->profile());
  webapps::AppId app_id =
      provider->registrar_unsafe().LookupExternalAppId(install_url).value();

  EXPECT_FALSE(provider->registrar_unsafe().CanUserUninstallWebApp(app_id));

  InstallWebAppFromPage(browser(), install_url);

  // Performing a user install on the page should not override the "policy"
  // install source.
  EXPECT_FALSE(provider->registrar_unsafe().CanUserUninstallWebApp(app_id));
  const WebApp& web_app = *provider->registrar_unsafe().GetAppById(app_id);
  if (base::FeatureList::IsEnabled(
          features::kWebAppDontAddExistingAppsToSync)) {
    EXPECT_TRUE(web_app.GetSources().Has(WebAppManagement::kUserInstalled));
  } else {
    EXPECT_TRUE(web_app.IsSynced());
  }
  EXPECT_TRUE(web_app.IsPolicyInstalledApp());
}

// Tests that the command for OpenActiveTabInPwaWindow is available for secure
// pages in an app's scope.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ReparentWebAppForSecureActiveTab) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(tab_contents->GetLastCommittedURL(), app_url);

  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kEnabled);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ShortcutIconCorrectColor) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server()->GetURL(
          "/banners/manifest_test_page.html?manifest=manifest_one_icon.json")));

  // Wait for OS hooks and installation to complete and the app to launch.
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id) {
        run_loop_install.Quit();
      }));
  content::CreateAndLoadWebContentsObserver app_loaded_observer;
  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  run_loop_install.Run();
  app_loaded_observer.Wait();

  base::FilePath shortcut_path;
  auto* provider = WebAppProvider::GetForTest(profile());
  std::vector<SkColor> expected_pixel_colors = {SkColorSetRGB(92, 92, 92)};
  std::optional<SkColor> icon_pixel_color = std::nullopt;
#if BUILDFLAG(IS_MAC)
  icon_pixel_color = os_integration_override().GetShortcutIconTopLeftColor(
      profile(), os_integration_override().chrome_apps_folder(), app_id,
      provider->registrar_unsafe().GetAppShortName(app_id));
#elif BUILDFLAG(IS_WIN)
  icon_pixel_color = os_integration_override().GetShortcutIconTopLeftColor(
      profile(), os_integration_override().application_menu(), app_id,
      provider->registrar_unsafe().GetAppShortName(app_id));
  expected_pixel_colors.push_back(SkColorSetRGB(91, 91, 91));
  expected_pixel_colors.push_back(SkColorSetRGB(90, 90, 90));
#endif
  EXPECT_TRUE(icon_pixel_color.has_value());
  EXPECT_THAT(expected_pixel_colors,
              testing::Contains(icon_pixel_color.value()))
      << "Actual color (RGB) is: "
      << color_utils::SkColorToRgbString(icon_pixel_color.value());

  base::test::TestFuture<webapps::UninstallResultCode> future;
  provider->scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu, future.GetCallback());
  EXPECT_TRUE(UninstallSucceeded(future.Get()));
}
#endif

#if BUILDFLAG(IS_WIN)

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

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_ShortcutMenu, ShortcutsMenuSuccess) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  NavigateViaLinkClickToURLAndWait(
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
  base::HistogramTester tester;
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id) {
        EXPECT_THAT(
            tester.GetAllSamples("WebApp.ShortcutsMenuRegistration.Result"),
            BucketsAre(base::Bucket(true, 1)));
        run_loop_install.Quit();
      }));
  content::CreateAndLoadWebContentsObserver app_loaded_observer;
  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
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

  base::test::TestFuture<webapps::UninstallResultCode> future;
  provider().scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu, future.GetCallback());
  EXPECT_TRUE(UninstallSucceeded(future.Get()));
  EXPECT_THAT(tester.GetAllSamples("WebApp.ShortcutsMenuUnregistered.Result"),
              BucketsAre(base::Bucket(true, 1)));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_ShortcutMenu,
                       ShortcutsMenuRegistrationWithNoShortcuts) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  NavigateViaLinkClickToURLAndWait(
      browser(),
      https_server()->GetURL("/banners/"
                             "manifest_test_page.html?manifest=manifest.json"));

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
  base::HistogramTester tester;
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id) {
        // Verify that since the shortcuts menu items are not registered,
        // none of the buckets are filled.
        EXPECT_THAT(
            tester.GetAllSamples("WebApp.ShortcutsMenuRegistered.Result"),
            BucketsAre(base::Bucket(true, 0), base::Bucket(false, 0)));
        run_loop_install.Quit();
      }));
  content::CreateAndLoadWebContentsObserver app_loaded_observer;
  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  run_loop_install.Run();
  app_loaded_observer.Wait();

  // No shortcuts should be read.
  EXPECT_TRUE(shortcuts_menu_items.empty());

  base::test::TestFuture<webapps::UninstallResultCode> future;
  provider().scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu, future.GetCallback());
  EXPECT_TRUE(UninstallSucceeded(future.Get()));
  EXPECT_THAT(tester.GetAllSamples("WebApp.ShortcutsMenuUnregistered.Result"),
              BucketsAre(base::Bucket(true, 0), base::Bucket(false, 0)));
}

#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, WebAppCreateAndDeleteShortcut) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  auto* provider = WebAppProvider::GetForTest(profile());

  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());

  // Wait for OS hooks and installation to complete and the app to launch.
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id) {
        run_loop_install.Quit();
      }));
  content::CreateAndLoadWebContentsObserver app_loaded_observer;
  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  run_loop_install.Run();
  app_loaded_observer.Wait();

  EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(app_id));
  EXPECT_EQ(provider->registrar_unsafe().GetAppShortName(app_id),
            GetInstallableAppName());

  EXPECT_TRUE(os_integration_override().IsShortcutCreated(
      profile(), app_id, provider->registrar_unsafe().GetAppShortName(app_id)));

  // Uninstall the web app
  base::test::TestFuture<webapps::UninstallResultCode> future;
  provider->scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu, future.GetCallback());
  EXPECT_TRUE(UninstallSucceeded(future.Get()));

#if BUILDFLAG(IS_WIN)
  base::FilePath desktop_shortcut_path =
      os_integration_override().GetShortcutPath(
          profile(), os_integration_override().desktop(), app_id,
          provider->registrar_unsafe().GetAppShortName(app_id));
  base::FilePath app_menu_shortcut_path =
      os_integration_override().GetShortcutPath(
          profile(), os_integration_override().application_menu(), app_id,
          provider->registrar_unsafe().GetAppShortName(app_id));
  EXPECT_FALSE(base::PathExists(desktop_shortcut_path));
  EXPECT_FALSE(base::PathExists(app_menu_shortcut_path));
#elif BUILDFLAG(IS_MAC)
  base::FilePath app_shortcut_path = os_integration_override().GetShortcutPath(
      profile(), os_integration_override().chrome_apps_folder(), app_id,
      provider->registrar_unsafe().GetAppShortName(app_id));
  EXPECT_FALSE(base::PathExists(app_shortcut_path));
#elif BUILDFLAG(IS_LINUX)
  base::FilePath desktop_shortcut_path =
      os_integration_override().GetShortcutPath(
          profile(), os_integration_override().desktop(), app_id,
          provider->registrar_unsafe().GetAppShortName(app_id));
  EXPECT_FALSE(base::PathExists(desktop_shortcut_path));
#endif
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, RunOnOsLoginMetrics) {
  GURL pwa_url("https://test-app.com");

  base::ScopedAllowBlockingForTesting allow_blocking;

  auto* provider = WebAppProvider::GetForTest(profile());
  const webapps::AppId& app_id = InstallPWA(pwa_url);

  ASSERT_TRUE(provider->registrar_unsafe().IsInstalled(app_id));

  base::HistogramTester tester;
  base::RunLoop run_loop;
  provider->scheduler().SetRunOnOsLoginMode(
      app_id, RunOnOsLoginMode::kWindowed, base::BindLambdaForTesting([&]() {
        // Unregister happens before register.
        EXPECT_THAT(
            tester.GetAllSamples("WebApp.RunOnOsLogin.Unregistration.Result"),
            BucketsAre(base::Bucket(true, 1)));
        EXPECT_THAT(
            tester.GetAllSamples("WebApp.RunOnOsLogin.Registration.Result"),
            BucketsAre(base::Bucket(true, 1)));
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(os_integration_override().IsRunOnOsLoginEnabled(
      profile(), app_id, provider->registrar_unsafe().GetAppShortName(app_id)));

  test::UninstallAllWebApps(profile());
  EXPECT_FALSE(os_integration_override().IsRunOnOsLoginEnabled(
      profile(), app_id, provider->registrar_unsafe().GetAppShortName(app_id)));
  EXPECT_THAT(tester.GetAllSamples("WebApp.RunOnOsLogin.Unregistration.Result"),
              BucketsAre(base::Bucket(true, 2)));
}
#endif

// Tests that reparenting the last browser tab doesn't close the browser window.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ReparentLastBrowserTab) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  Browser* const app_browser = ReparentWebAppForActiveTab(browser());
  ASSERT_EQ(app_browser->app_controller()->app_id(), app_id);

  ASSERT_TRUE(IsBrowserOpen(browser()));
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
}

using WebAppBrowserTestUpdateShortcutResult = WebAppBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppBrowserTestUpdateShortcutResult, UpdateShortcut) {
  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());

  WebAppProvider* provider = WebAppProvider::GetForTest(profile());

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  provider->scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      base::BindOnce(test::TestAcceptDialogCallback),
      install_future.GetCallback(), FallbackBehavior::kCraftedManifestOnly);

  const webapps::AppId& app_id = install_future.Get<0>();
  EXPECT_EQ(provider->registrar_unsafe().GetAppShortName(app_id),
            GetInstallableAppName());

  {
    ScopedRegistryUpdate update = provider->sync_bridge_unsafe().BeginUpdate();
    update->UpdateApp(app_id)->SetName("test_app_2");
  }

  base::HistogramTester tester;
  base::test::TestFuture<void> test_future;
  provider->os_integration_manager().Synchronize(app_id,
                                                 test_future.GetCallback());
  ASSERT_TRUE(test_future.Wait());
  EXPECT_THAT(tester.GetAllSamples("WebApp.Shortcuts.Update.Result"),
              BucketsAre(base::Bucket(true, 1)));

  base::test::TestFuture<std::unique_ptr<ShortcutInfo>> shortcut_future;
  provider->os_integration_manager().GetShortcutInfoForAppFromRegistrar(
      app_id, shortcut_future.GetCallback());
  auto shortcut_info = shortcut_future.Take();
  EXPECT_NE(shortcut_info, nullptr);
  EXPECT_EQ(shortcut_info->title, u"test_app_2");

  test::UninstallAllWebApps(profile());
  EXPECT_FALSE(provider->registrar_unsafe().IsInstalled(app_id));
}

// Tests that reparenting a display: browser app tab results in a minimal-ui
// app window.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, ReparentDisplayBrowserApp) {
  const GURL app_url = GetSecureAppURL();
  auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->display_mode = DisplayMode::kBrowser;
  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  web_app_info->title = u"A Non installable App";
  const webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

  base::HistogramTester tester;
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(tab_contents->GetLastCommittedURL(), app_url);

  ui_test_utils::BrowserChangeObserver new_app_browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kEnabled);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_OPEN_IN_PWA_WINDOW));
  ui_test_utils::WaitForBrowserSetLastActive(new_app_browser_observer.Wait());

  Browser* const app_browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_EQ(app_browser->app_controller()->app_id(), app_id);
  EXPECT_TRUE(app_browser->app_controller()->HasMinimalUiButtons());

  auto* provider = WebAppProvider::GetForTest(profile());
  EXPECT_EQ(provider->registrar_unsafe().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_EQ(provider->registrar_unsafe().GetAppEffectiveDisplayMode(app_id),
            DisplayMode::kMinimalUi);
  EXPECT_FALSE(
      provider->registrar_unsafe().GetAppLastLaunchTime(app_id).is_null());
  tester.ExpectUniqueSample("WebApp.LaunchContainer",
                            apps::LaunchContainer::kLaunchContainerWindow, 1);
  tester.ExpectUniqueSample("WebApp.LaunchSource",
                            apps::LaunchSource::kFromReparenting, 1);
}

// Tests that the manifest name of the current installable site is used in the
// installation menu text.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, InstallToShelfContainsAppName) {
  EXPECT_TRUE(
      NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL()));

  auto app_menu_model = std::make_unique<AppMenuModel>(nullptr, browser());
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  size_t index = 0;
  EXPECT_TRUE(app_menu_model->GetModelAndIndexForCommandId(IDC_INSTALL_PWA,
                                                           &model, &index));

  // PWA install entry is in the Save and Share submenu in CR2023.
  ui::MenuModel* save_and_share_submenu = app_menu_model.get();
  size_t save_and_share_index = 0;
  EXPECT_TRUE(app_menu_model->GetModelAndIndexForCommandId(
      IDC_SAVE_AND_SHARE_MENU, &save_and_share_submenu, &save_and_share_index));
  save_and_share_submenu =
      save_and_share_submenu->GetSubmenuModelAt(save_and_share_index);
  EXPECT_EQ(save_and_share_submenu, model);

  const std::u16string label = model->GetLabelAt(index);
  EXPECT_NE(std::u16string::npos, label.find(u"Manifest test"));
}

// Check that no assertions are hit when showing a permission request bubble.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, PermissionBubble) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  content::RenderFrameHost* const render_frame_host =
      app_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame();
  EXPECT_TRUE(content::ExecJs(
      render_frame_host,
      "navigator.geolocation.getCurrentPosition(function(){});"));
}

using WebAppBrowserTest_PrefixInTitle = WebAppBrowserTest;

// Ensure that web app windows don't duplicate the app name in the title, when
// the page's title already starts with the app name.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_PrefixInTitle, PrefixExistsInTitle) {
  const GURL app_url =
      https_server()->GetURL("app.com", "/web_apps/title_appname_prefix.html");
  const std::u16string app_title = u"A Web App";

  auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->title = app_title;
  const webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

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
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_PrefixInTitle,
                       WebAppWindowTitleForEmptyAndSimpleWebContentTitles) {
  // Ensure web app windows show the expected title when the contents have an
  // empty or simple title.
  const GURL app_url = https_server()->GetURL("app.site.test", "/empty.html");
  const std::u16string app_title = u"A Web App";
  auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->title = app_title;
  const webapps::AppId app_id = InstallWebApp(std::move(web_app_info));
  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(app_title, app_browser->GetWindowTitleForCurrentTab(false));
  NavigateViaLinkClickToURLAndWait(
      app_browser, https_server()->GetURL("app.site.test", "/simple.html"));
  EXPECT_EQ(u"A Web App - OK", app_browser->GetWindowTitleForCurrentTab(false));
}

// Ensure that web app windows display the app title instead of the page
// title when off scope.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_PrefixInTitle,
                       OffScopeUrlsDisplayAppTitle) {
  const GURL app_url = GetSecureAppURL();
  const std::u16string app_title = u"A Web App";

  auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->title = app_title;
  const webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // When we are within scope, show the page title.
  EXPECT_EQ(u"A Web App - Google",
            app_browser->GetWindowTitleForCurrentTab(false));
  NavigateViaLinkClickToURLAndWait(
      app_browser, https_server()->GetURL("app.site.test", "/simple.html"));

  // When we are off scope, show the app title.
  EXPECT_EQ(app_title, app_browser->GetWindowTitleForCurrentTab(false));
}

// Ensure that web app windows display the app title instead of the page
// title when using http.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, InScopeHttpUrlsDisplayAppTitle) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("app.site.test", "/simple.html");
  const std::u16string app_title = u"A Web App";

  auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  web_app_info->title = app_title;
  const webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

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
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);
  EXPECT_FALSE(app_browser->app_controller()->HasTitlebarAppOriginText());
}

// Check that a subframe on a regular web page can navigate to a URL that
// redirects to a web app.  https://crbug.com/721949.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, SubframeRedirectsToWebApp) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set up a web app which covers app.com URLs.
  GURL app_url = embedded_test_server()->GetURL("app.com", "/title1.html");
  const webapps::AppId app_id = InstallPWA(app_url);

  // Navigate a regular tab to a page with a subframe.
  const GURL url = embedded_test_server()->GetURL("foo.com", "/iframe.html");
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigateViaLinkClickToURLAndWait(browser(), url);

  // Navigate the subframe to a URL that redirects to a URL in the web app's
  // web extent.
  const GURL redirect_url = embedded_test_server()->GetURL(
      "bar.com", "/server-redirect?" + app_url.spec());
  EXPECT_TRUE(NavigateIframeToURL(tab, "test", redirect_url));

  // Ensure that the frame navigated successfully and that it has correct
  // content.
  content::RenderFrameHost* const subframe =
      content::ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(app_url, subframe->GetLastCommittedURL());
  EXPECT_EQ(
      "This page has no title.",
      EvalJs(subframe, "document.body.innerText.trim();").ExtractString());
}

#if BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, NewAppWindow) {
  BrowserList* const browser_list = BrowserList::GetInstance();
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  EXPECT_EQ(browser_list->size(), 2U);

  ui_test_utils::BrowserChangeObserver browser_change_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  EXPECT_TRUE(chrome::ExecuteCommand(app_browser, IDC_NEW_WINDOW));
  Browser* const new_browser = browser_change_observer.Wait();

  EXPECT_EQ(new_browser, browser_list->GetLastActive());
  EXPECT_EQ(browser_list->size(), 3U);
  EXPECT_NE(new_browser, browser());
  EXPECT_NE(new_browser, app_browser);
  EXPECT_TRUE(new_browser->is_type_app());
  EXPECT_EQ(new_browser->app_controller()->app_id(), app_id);

  WebAppProvider::GetForTest(profile())
      ->sync_bridge_unsafe()
      .SetAppUserDisplayModeForTesting(
          app_id, web_app::mojom::UserDisplayMode::kBrowser);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);

  ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
  EXPECT_TRUE(chrome::ExecuteCommand(app_browser, IDC_NEW_WINDOW));
  content::WebContents* new_tab = tab_waiter.Wait();

  ASSERT_TRUE(new_tab);
  EXPECT_EQ(browser_list->GetLastActive(), browser());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(new_tab, browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(new_tab->GetVisibleURL(), app_url);
}

#endif

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, PopupLocationBar) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  const GURL app_url = GetSecureAppURL();
  const GURL in_scope =
      https_server()->GetURL("app.com", "/ssl/page_with_subresource.html");
  const webapps::AppId app_id = InstallPWA(app_url);

  Browser::CreateParams params = web_app::CreateParamsForApp(
      app_id,
      /*is_popup*/ true,
      /*trusted_source=*/true, /*window_bounds=*/gfx::Rect(), profile(),
      /*user_gesture=*/true);
  Browser* popup_browser =
      web_app::CreateWebAppWindowMaybeWithHomeTab(app_id, params);
  BrowserActivationWaiter activation_waiter(popup_browser);
  popup_browser->window()->Show();
  activation_waiter.WaitForActivation();

  EXPECT_TRUE(
      popup_browser->CanSupportWindowFeature(Browser::FEATURE_LOCATIONBAR));
  EXPECT_TRUE(
      popup_browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR));

  ui_test_utils::ToggleFullscreenModeAndWait(popup_browser);

  EXPECT_TRUE(
      popup_browser->CanSupportWindowFeature(Browser::FEATURE_LOCATIONBAR));
}

// Make sure chrome://web-app-internals page loads fine.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, WebAppInternalsPage) {
  // Loads with no web app.
  NavigateViaLinkClickToURLAndWait(browser(),
                                   GURL("chrome://web-app-internals"));

  const GURL app_url = GetSecureAppURL();
  InstallPWA(app_url);
  // Loads with one web app.
  NavigateViaLinkClickToURLAndWait(browser(),
                                   GURL("chrome://web-app-internals"));

  // Install a non-promotable web app.
  NavigateViaLinkClickToURLAndWait(
      browser(), https_server()->GetURL("/banners/no_manifest_test_page.html"));

  if (base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)) {
    // Install as DIY App.
    SetAutoAcceptDiyAppsInstallDialogForTesting(/*auto_accept=*/true);
    WebAppTestInstallObserver observer(profile());
    observer.BeginListening();
    CHECK(chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA));
    observer.Wait();
    SetAutoAcceptDiyAppsInstallDialogForTesting(/*auto_accept=*/false);
  } else {
    // Install using Create Shortcut flow until universal install is globally
    // enabled by default.
    SetAutoAcceptWebAppDialogForTesting(/*auto_accept=*/true,
                                        /*auto_open_in_window=*/false);
    WebAppTestInstallObserver observer(profile());
    observer.BeginListening();
    CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
    observer.Wait();
    SetAutoAcceptWebAppDialogForTesting(false, false);
  }

  // Loads with two apps.
  NavigateViaLinkClickToURLAndWait(browser(),
                                   GURL("chrome://web-app-internals"));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, BrowserDisplayNotInstallable) {
  GURL url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_display_browser.json");
  NavigateAndAwaitInstallabilityCheck(browser(), url);

  bool universal_install_enabled =
      base::FeatureList::IsEnabled(features::kWebAppUniversalInstall);

  AppMenuCommandState install_state =
      universal_install_enabled ? kEnabled : kNotPresent;
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), install_state);

  if (universal_install_enabled) {
    // Install as DIY App.
    SetAutoAcceptDiyAppsInstallDialogForTesting(/*auto_accept=*/true);
    WebAppTestInstallObserver observer(profile());
    observer.BeginListening();
    CHECK(chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA));
    observer.Wait();
    SetAutoAcceptDiyAppsInstallDialogForTesting(/*auto_accept=*/false);
  } else {
    // Install using Create Shortcut flow until universal install is globally
    // enabled by default.
    SetAutoAcceptWebAppDialogForTesting(/*auto_accept=*/true,
                                        /*auto_open_in_window=*/false);
    WebAppTestInstallObserver observer(profile());
    observer.BeginListening();
    CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
    observer.Wait();
    SetAutoAcceptWebAppDialogForTesting(false, false);
  }

  // Navigate to this site again and install should not show up if universal
  // install is enabled.
  Browser* new_browser = NavigateInNewWindowAndAwaitInstallabilityCheck(url);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, WindowControlsOverlay) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_window_controls_overlay.json");
  NavigateViaLinkClickToURLAndWait(browser(), test_url);

  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());

  std::vector<DisplayMode> app_display_mode_override =
      provider->registrar_unsafe().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(1u, app_display_mode_override.size());
  EXPECT_EQ(DisplayMode::kWindowControlsOverlay, app_display_mode_override[0]);

  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  EXPECT_EQ(true,
            app_browser->app_controller()->AppUsesWindowControlsOverlay());
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_Borderless, Borderless) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_borderless.json");
  NavigateViaLinkClickToURLAndWait(browser(), test_url);

  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());

  std::vector<DisplayMode> app_display_mode_override =
      provider->registrar_unsafe().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(1u, app_display_mode_override.size());
  EXPECT_EQ(DisplayMode::kBorderless, app_display_mode_override[0]);

  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  app_browser->app_controller()->SetIsolatedWebAppTrueForTesting();

  EXPECT_TRUE(app_browser->app_controller()->AppUsesBorderlessMode());
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_Tabbed, TabbedDisplayOverride) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_tabbed_display_override.json");
  NavigateViaLinkClickToURLAndWait(browser(), test_url);

  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());

  std::vector<DisplayMode> app_display_mode_override =
      provider->registrar_unsafe().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(1u, app_display_mode_override.size());
  EXPECT_EQ(DisplayMode::kTabbed, app_display_mode_override[0]);
  EXPECT_EQ(true,
            provider->registrar_unsafe().IsTabbedWindowModeEnabled(app_id));
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
  const webapps::AppId app_id = InstallPWA(app_url);
  apps::AppLaunchParams params(
      app_id, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);

  BrowserHandler handler(nullptr, std::string());
  handler.Close();
  ui_test_utils::WaitForBrowserToClose();

  content::WebContents* const web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(std::move(params));
  EXPECT_EQ(web_contents, nullptr);
}

using WebAppBrowserTest_ManifestId = WebAppBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_ManifestId, NoManifestId) {
  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());

  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());
  auto* app = provider->registrar_unsafe().GetAppById(app_id);

  EXPECT_EQ(web_app::GenerateAppId(
                /*manifest_id=*/std::nullopt,
                provider->registrar_unsafe().GetAppStartUrl(app_id)),
            app_id);
  EXPECT_EQ(app->start_url(), app->manifest_id());
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_ManifestId, ManifestIdSpecified) {
  NavigateAndAwaitInstallabilityCheck(
      browser(),
      https_server()->GetURL(
          "/banners/manifest_test_page.html?manifest=manifest_with_id.json"));

  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  auto* provider = WebAppProvider::GetForTest(profile());
  auto* app = provider->registrar_unsafe().GetAppById(app_id);

  EXPECT_EQ(web_app::GenerateAppIdFromManifestId(app->manifest_id()), app_id);
  EXPECT_NE(
      web_app::GenerateAppId(/*manifest_id=*/std::nullopt, app->start_url()),
      app_id);
}

#if !BUILDFLAG(IS_CHROMEOS)
using WebAppBrowserTest_FileHandler = WebAppBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_FileHandler, FileAssociation) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::HistogramTester tester;
  std::vector<std::string> expected_extensions{"bar", "baz", "foo", "foobar"};

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL app_url(embedded_test_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_with_file_handlers.json"));

  NavigateViaLinkClickToURLAndWait(browser(), app_url);

  // Wait for OS hooks and installation to complete.
  SetAutoAcceptWebAppDialogForTesting(true, true);
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id) {
        EXPECT_THAT(
            tester.GetAllSamples("WebApp.FileHandlersRegistration.Result"),
            BucketsAre(base::Bucket(true, 1)));
        run_loop_install.Quit();
      }));
  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  run_loop_install.Run();
  content::RunAllTasksUntilIdle();
  SetAutoAcceptWebAppDialogForTesting(false, false);

  for (auto extension : expected_extensions) {
    EXPECT_TRUE(os_integration_override().IsFileExtensionHandled(
        browser()->profile(), app_id, "Manifest with file handlers",
        "." + extension));
  }
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
      const std::string extension = base::WideToUTF8(file_extension.substr(1));
      EXPECT_TRUE(base::Contains(expected_extensions, extension))
          << "Missing file extension: " << extension;
      const std::wstring reg_key =
          L"Software\\Classes\\" + file_extension + L"\\OpenWithProgids";
      reg_key_prog_id_map[reg_key] = file_handler_prog_id;
      ASSERT_EQ(ERROR_SUCCESS,
                key.Open(HKEY_CURRENT_USER, reg_key.data(), KEY_READ));
      EXPECT_TRUE(key.HasValue(file_handler_prog_id.data()));
    }
  }
#endif

  // Uninstall the web app
  base::test::TestFuture<webapps::UninstallResultCode> future;
  provider().scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppsPage, future.GetCallback());
  EXPECT_TRUE(UninstallSucceeded(future.Get()));
  EXPECT_THAT(tester.GetAllSamples("WebApp.FileHandlersUnregistration.Result"),
              BucketsAre(base::Bucket(true, 1)));

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

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_FileHandler,
                       UserDenyFileHandlingPermission) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::vector<std::string> expected_extensions{"bar", "baz", "foo", "foobar"};

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL app_url(embedded_test_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_with_file_handlers.json"));

  NavigateViaLinkClickToURLAndWait(browser(), app_url);

  // Wait for OS hooks and installation to complete.
  SetAutoAcceptWebAppDialogForTesting(true, true);
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id) {
        run_loop_install.Quit();
      }));
  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  run_loop_install.Run();
  content::RunAllTasksUntilIdle();
  SetAutoAcceptWebAppDialogForTesting(false, false);

  auto is_handling_extension = [&](const std::string& extension) {
    return os_integration_override().IsFileExtensionHandled(
        browser()->profile(), app_id, "Manifest with file handlers",
        "." + extension);
  };
  // Sanity check that the app is in fact currently handling a file type,
  // and is_handling_extension doesn't already return false.
  ASSERT_TRUE(is_handling_extension(expected_extensions[0]));

  // Simulate the user permanently denying file handling permission. Regression
  // test for crbug.com/1269387
  base::RunLoop run_loop_remove_file_handlers;
  provider().scheduler().PersistFileHandlersUserChoice(
      app_id, /*allowed=*/false, run_loop_remove_file_handlers.QuitClosure());
  run_loop_remove_file_handlers.Run();

  for (auto extension : expected_extensions) {
    while (is_handling_extension(extension)) {
      base::RunLoop delay_loop;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, delay_loop.QuitClosure(), base::Milliseconds(100));
      delay_loop.Run();
    }
  }
  // Uninstall the web app
  base::test::TestFuture<webapps::UninstallResultCode> future;
  provider().scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppsPage, future.GetCallback());
  EXPECT_TRUE(UninstallSucceeded(future.Get()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest, PRE_UninstallIncompleteUninstall) {
  auto* provider = WebAppProvider::GetForTest(profile());

  NavigateViaLinkClickToURLAndWait(browser(), GetInstallableAppURL());

  // Wait for OS hooks and installation to complete and the app to launch.
  base::RunLoop run_loop_install;
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppInstalledWithOsHooksDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id) {
        run_loop_install.Quit();
      }));
  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  run_loop_install.Run();

  EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(app_id));
  EXPECT_EQ(provider->registrar_unsafe().GetAppShortName(app_id),
            GetInstallableAppName());
  // This does NOT uninstall the web app, it just flags it for uninstall on
  // startup.
  {
    ScopedRegistryUpdate update = provider->sync_bridge_unsafe().BeginUpdate();
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
  std::set<webapps::AppId> apps;
  for (const auto& web_app :
       provider->registrar_unsafe().GetAppsIncludingStubs()) {
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
      provider->registrar_unsafe().GetAppsIncludingStubs();
  for (auto it = app_set.begin(); it != app_set.end(); ++it) {
    ++app_count;
  }
  EXPECT_EQ(app_count, 0);
}

// Verifies the behavior of the App/site settings link in the page info bubble.
class WebAppBrowserTest_PageInfoManagementLink : public WebAppBrowserTest {
 public:
  bool ShowingAppManagementLink(Browser* browser) {
    int unused_id, unused_id2;
    return GetLabelIdsForAppManagementLinkInPageInfo(
        browser->tab_strip_model()->GetActiveWebContents(), &unused_id,
        &unused_id2);
  }
};

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_PageInfoManagementLink, Reparenting) {
  const GURL app_url = GetSecureAppURL();
  InstallPWA(app_url);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(tab_contents->GetLastCommittedURL(), app_url);

  // After a normal (e.g. typed) navigation, should show the app settings
  // link.
  EXPECT_TRUE(ShowingAppManagementLink(browser()));
  // Reparent into app browser window.
  Browser* const app_browser = ReparentWebAppForActiveTab(browser());
  // The leftover tab in the tabbed browser window should not be appy.
  EXPECT_FALSE(ShowingAppManagementLink(browser()));
  // After reparenting into an app browser, should show the app settings link.
  EXPECT_TRUE(ShowingAppManagementLink(app_browser));

  // Move back into tabbed browser: should keep showing the app settings link.
  Browser* tabbed_browser = chrome::OpenInChrome(app_browser);
  EXPECT_TRUE(ShowingAppManagementLink(tabbed_browser));
}

// Verifies behavior when an app window is opened by navigating with
// `open_pwa_window_if_possible` set to true.
IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_PageInfoManagementLink,
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

  EXPECT_TRUE(ShowingAppManagementLink(params.browser));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTest_PageInfoManagementLink, LaunchAsTab) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);

  // A non appy tab is showing, so the app settings link should not be visible.
  EXPECT_FALSE(ShowingAppManagementLink(browser()));

  // *Launch* the app as a tab in a normal browser window. The app settings link
  // should be visible.
  Browser* tabbed_browser = LaunchBrowserForWebAppInTab(app_id);
  EXPECT_EQ(browser(), tabbed_browser);
  EXPECT_TRUE(ShowingAppManagementLink(tabbed_browser));
}

}  // namespace web_app
