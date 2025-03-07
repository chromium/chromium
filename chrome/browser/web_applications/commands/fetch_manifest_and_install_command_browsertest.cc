// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

// Mock of a WebContentsDelegate that catches messages sent to the console.
class WebContentsErrorDelegate : public content::WebContentsDelegate {
 public:
  WebContentsErrorDelegate() = default;

  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override {
    CHECK_EQ(source->GetDelegate(), this);

    if (log_level == blink::mojom::ConsoleMessageLevel::kError) {
      console_errors_.push_back(message);
    }
    return false;
  }

  bool NoConsoleErrors() { return console_errors_.empty(); }

 private:
  std::vector<std::u16string> console_errors_;
};

}  // namespace

class FetchManifestAndInstallCommandTest : public WebAppBrowserTestBase {
 public:
  WebAppInstallDialogCallback CreateDialogCallback(
      bool accept = true,
      mojom::UserDisplayMode user_display_mode =
          mojom::UserDisplayMode::kStandalone) {
    return base::BindLambdaForTesting(
        [accept, user_display_mode](
            base::WeakPtr<WebAppScreenshotFetcher>,
            content::WebContents* initiator_web_contents,
            std::unique_ptr<WebAppInstallInfo> web_app_info,
            WebAppInstallationAcceptanceCallback acceptance_callback) {
          web_app_info->user_display_mode = user_display_mode;
          std::move(acceptance_callback).Run(accept, std::move(web_app_info));
        });
  }
};

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest, SuccessInstall) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::RunLoop loop;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const webapps::AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
                      provider().registrar_unsafe().GetInstallState(app_id));
            loop.Quit();
          }),
      FallbackBehavior::kCraftedManifestOnly);
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest, ReparentInTab) {
  GURL test_url = https_server()->GetURL("/web_apps/minimal_ui/basic.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      CreateDialogCallback(), install_future.GetCallback(),
      FallbackBehavior::kCraftedManifestOnly);
  base::HistogramTester tester;
  ASSERT_TRUE(install_future.Wait());
  tester.ExpectUniqueSample("WebApp.LaunchSource",
                            apps::LaunchSource::kFromReparenting, 1);
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest,
                       WontReparentFromDevtools) {
  GURL test_url = https_server()->GetURL("/web_apps/minimal_ui/basic.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::DEVTOOLS,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      CreateDialogCallback(), install_future.GetCallback(),
      FallbackBehavior::kCraftedManifestOnly);
  base::HistogramTester tester;
  ASSERT_TRUE(install_future.Wait());
  tester.ExpectUniqueSample("WebApp.LaunchSource",
                            apps::LaunchSource::kFromReparenting, 0);
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest, MultipleManifests) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "multiple_manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      CreateDialogCallback(), install_future.GetCallback(),
      FallbackBehavior::kCraftedManifestOnly);
  ASSERT_TRUE(install_future.Wait());
  EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kSuccessNewInstall);
  webapps::AppId app_id = install_future.Get<webapps::AppId>();
  EXPECT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
            provider().registrar_unsafe().GetInstallState(app_id));

  // multiple_manifest_test_page.html includes both manifest_with_id.json and
  // manifest.json. Section 4.6.7.10 of the HTML spec says the first manifest
  // should be used.
  EXPECT_EQ(provider().registrar_unsafe().GetAppManifestId(app_id),
            https_server()->GetURL("/some_id"));
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest, MultipleInstalls) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  // Schedule two installs. The second should fail because the first will cause
  // a navigation (because reparenting somehow changes visiblity, which is
  // wrong, but fine).
  base::RunLoop loop;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const webapps::AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
                      provider().registrar_unsafe().GetInstallState(app_id));
          }),
      FallbackBehavior::kCraftedManifestOnly);

  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      CreateDialogCallback(),
      base::BindLambdaForTesting([&](const webapps::AppId& app_id,
                                     webapps::InstallResultCode code) {
        EXPECT_EQ(
            code,
            webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
        EXPECT_FALSE(provider().registrar_unsafe().IsInRegistrar(app_id));
        loop.Quit();
      }),
      FallbackBehavior::kCraftedManifestOnly);
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest, InvalidManifest) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "no_manifest_test_page.html");
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::RunLoop loop;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const webapps::AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code,
                      webapps::InstallResultCode::kNotValidManifestForWebApp);
            EXPECT_FALSE(provider().registrar_unsafe().IsInRegistrar(app_id));
            loop.Quit();
          }),
      FallbackBehavior::kCraftedManifestOnly);
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest, UserDeclineInstall) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::RunLoop loop;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),

      CreateDialogCallback(/*accept=*/false),
      base::BindLambdaForTesting(
          [&](const webapps::AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kUserInstallDeclined);
            EXPECT_FALSE(provider().registrar_unsafe().IsInRegistrar(app_id));
            loop.Quit();
          }),
      FallbackBehavior::kCraftedManifestOnly);
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest,
                       HandleWebContentsDestroyed) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::RunLoop loop;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      web_contents->GetWeakPtr(), CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const webapps::AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kWebContentsDestroyed);
            EXPECT_FALSE(provider().registrar_unsafe().IsInRegistrar(app_id));
            loop.Quit();
          }),
      FallbackBehavior::kCraftedManifestOnly);

  // Create a new tab to ensure that the browser isn't destroyed with the web
  // contents closing.
  chrome::NewTab(browser());
  web_contents->Close();

  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest,
                       InstallWithFallback) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "no_manifest_test_page.html");
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::RunLoop loop;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      web_contents->GetWeakPtr(), CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const webapps::AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
                      provider().registrar_unsafe().GetInstallState(app_id));
            loop.Quit();
          }),
      FallbackBehavior::kAllowFallbackDataAlways);
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest,
                       InstallWithFallbackOverwriteInstalled) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "no_manifest_test_page.html");
  auto web_app = test::CreateWebApp(test_url);
  const webapps::AppId app_id = web_app->app_id();

  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
    web_app->SetInstallState(
        proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE);
    update->CreateApp(std::move(web_app));
  }

  EXPECT_EQ(proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE,
            provider().registrar_unsafe().GetInstallState(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppUserDisplayMode(app_id).value(),
            mojom::UserDisplayMode::kBrowser);

  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::RunLoop loop;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      web_contents->GetWeakPtr(),

      CreateDialogCallback(
          /*accept=*/true,
          /*user_display_mode=*/mojom::UserDisplayMode::kStandalone),
      base::BindLambdaForTesting(
          [&](const webapps::AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            loop.Quit();
          }),
      FallbackBehavior::kAllowFallbackDataAlways);
  loop.Run();
  EXPECT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
            provider().registrar_unsafe().GetInstallState(app_id));

  EXPECT_EQ(provider().registrar_unsafe().GetAppUserDisplayMode(app_id).value(),
            mojom::UserDisplayMode::kStandalone);
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest,
                       InstallFromOutsideScopeToolbarHasBackButton) {
  GURL test_url = https_server()->GetURL("/banners/app_with_nested/index.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      CreateDialogCallback(), install_future.GetCallback(),
      FallbackBehavior::kCraftedManifestOnly);
  ASSERT_TRUE(install_future.Wait());
  EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kSuccessNewInstall);
  webapps::AppId app_id = install_future.Get<webapps::AppId>();
  EXPECT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
            provider().registrar_unsafe().GetInstallState(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);

  Browser* app_browser =
      AppBrowserController::FindForWebApp(*profile(), app_id);
  ASSERT_TRUE(app_browser);
  EXPECT_TRUE(app_browser->app_controller()->ShouldShowCustomTabBar());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser);
  ASSERT_TRUE(app_view);
  EXPECT_TRUE(
      app_view->toolbar()->custom_tab_bar()->IsShowingOriginForTesting());
  EXPECT_TRUE(
      app_view->toolbar()->custom_tab_bar()->IsShowingCloseButtonForTesting());
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest,
                       InstallFromPwaWindowDoesNotReparent) {
  GURL test_url = https_server()->GetURL("/banners/manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::WeakPtr<content::WebContents> active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr();
  // Install the app that will be used to try to install from.
  webapps::AppId app_id;
  {
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        install_future;
    provider().scheduler().FetchManifestAndInstall(
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, active_web_contents,
        CreateDialogCallback(), install_future.GetCallback(),
        FallbackBehavior::kCraftedManifestOnly);
    ASSERT_TRUE(install_future.Wait());
    EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    app_id = install_future.Get<webapps::AppId>();
    EXPECT_EQ(WebAppTabHelper::FromWebContents(active_web_contents.get())
                  ->window_app_id(),
              app_id);
  }
  Browser* app_browser =
      AppBrowserController::FindForWebApp(*profile(), app_id);
  ASSERT_TRUE(app_browser);

  // Navigate to another installable page, and verify that installing from this
  // page (which shouldn't be possible in our UX, but is 'valid' for the
  // command) does not reparent the current web contents into the installed app.
  webapps::AppId other_app_id;
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(
      app_browser, https_server()->GetURL("/web_apps/simple/index.html")));
  {
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        install_future;
    ASSERT_TRUE(active_web_contents);
    // Ensure the web contents is visible & focused. This can be flaky, so
    // bypass visibility checks. See Mac flakiness example at
    // https://bit.ly/4gL5Eks.
    active_web_contents->Focus();
    auto bypass_visibility_check =
        FetchManifestAndInstallCommand::BypassVisibilityCheckForTesting();

    provider().scheduler().FetchManifestAndInstall(
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, active_web_contents,
        CreateDialogCallback(), install_future.GetCallback(),
        FallbackBehavior::kCraftedManifestOnly);
    ASSERT_TRUE(install_future.Wait());
    EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    other_app_id = install_future.Get<webapps::AppId>();
  }

  EXPECT_NE(other_app_id, app_id);
  EXPECT_FALSE(AppBrowserController::FindForWebApp(*profile(), other_app_id));
  ASSERT_TRUE(active_web_contents);
  EXPECT_EQ(WebAppTabHelper::FromWebContents(active_web_contents.get())
                ->window_app_id(),
            app_id);
}

// Tests that when an SVG icon is passed in the manifest with a size of |any|,
// the icon is downloaded correctly for the web app during installation.
// Parameterized to work with SVG icons that have an intrinsic size (width and
// height defined) as well as no intrinsic size.
class FetchManifestAndInstallCommandTestWithSVG
    : public FetchManifestAndInstallCommandTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  GURL GetSiteUrlBasedOnSVGParams() {
    if (GetParam()) {
      return https_server()->GetURL(
          "/banners/"
          "manifest_test_page.html?manifest=manifest_svg_icon_any.json");
    } else {
      return https_server()->GetURL(
          "/banners/"
          "manifest_test_page.html?manifest=manifest_svg_icon_no_intrinsic_"
          "size.json");
    }
  }

  // There is a minor loss in image quality during resizing that leads to colors
  // being not equal for precision for 2-3 decimal places. This is a helper
  // function to work around that behavior by verifying that the difference is
  // within some error margin.
  bool VerifyColorsMatchWithinErrorPrecision(SkColor read_color,
                                             SkColor expected_color) {
    bool colors_match = true;
    std::array<float, 4> read_colors = SkColor4f::FromColor(read_color).array();
    std::array<float, 4> expected_colors =
        SkColor4f::FromColor(expected_color).array();
    float precision = 0.02f;
    for (int i = 0; i < 4; i++) {
      colors_match =
          colors_match &&
          (std::abs(read_colors[i] - expected_colors[i]) < precision);
    }

    return colors_match;
  }
};

IN_PROC_BROWSER_TEST_P(FetchManifestAndInstallCommandTestWithSVG,
                       SuccessInstallSVGIcons) {
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(
      browser(), GetSiteUrlBasedOnSVGParams()));

  base::RunLoop loop;
  webapps::AppId installed_app_id;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      CreateDialogCallback(),
      base::BindLambdaForTesting([&](const webapps::AppId& app_id,
                                     webapps::InstallResultCode code) {
        EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
        EXPECT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
                  provider().registrar_unsafe().GetInstallState(app_id));
        installed_app_id = app_id;
        loop.Quit();
      }),
      FallbackBehavior::kCraftedManifestOnly);
  loop.Run();

  for (const int& icon_size : web_app::SizesToGenerate()) {
    SkColor small_icon_color = IconManagerReadAppIconPixel(
        provider().icon_manager(), installed_app_id, icon_size,
        /*x=*/icon_size / 2, /*y=*/icon_size / 2);
    EXPECT_TRUE(
        VerifyColorsMatchWithinErrorPrecision(small_icon_color, SK_ColorRED));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         FetchManifestAndInstallCommandTestWithSVG,
                         ::testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "SVGIconIntrinsicSize"
                                             : "SVGIconNoIntrinsicSize";
                         });

using FetchManifestAndInstallCommandUniversalInstallTest =
    FetchManifestAndInstallCommandTest;

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandUniversalInstallTest,
                       NoManifest) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "no_manifest_test_page.html");
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      CreateDialogCallback(), install_future.GetCallback(),
      FallbackBehavior::kUseFallbackInfoWhenNotInstallable);
  ASSERT_TRUE(install_future.Wait());
  EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kSuccessNewInstall);
  webapps::AppId app_id = install_future.Get<webapps::AppId>();
  EXPECT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
            provider().registrar_unsafe().GetInstallState(app_id));

  EXPECT_EQ("Web app banner test page",
            provider().registrar_unsafe().GetAppShortName(app_id));
  auto os_integration =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(os_integration);
  EXPECT_TRUE(os_integration->has_shortcut());
  // TODO(crbug.com/291778116): Add more checks once DIY apps are supported.
  EXPECT_TRUE(provider().registrar_unsafe().IsDiyApp(app_id));
}

// Test for crbug.com/381069204, where triggering an install command on
// chrome://password-manager does not throw any console errors.
using FetchManifestAndInstallTestNoConsoleErrors =
    FetchManifestAndInstallCommandTest;
IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallTestNoConsoleErrors,
                       PasswordManager) {
  std::unique_ptr<WebContentsErrorDelegate> delegate =
      std::make_unique<WebContentsErrorDelegate>();
  browser()->tab_strip_model()->GetActiveWebContents()->SetDelegate(
      delegate.get());

  GURL chrome_password_manager("chrome://password-manager/");
  EXPECT_TRUE(
      NavigateAndAwaitInstallabilityCheck(browser(), chrome_password_manager));

  base::RunLoop loop;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const webapps::AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
                      provider().registrar_unsafe().GetInstallState(app_id));
            loop.Quit();
          }),
      FallbackBehavior::kCraftedManifestOnly);
  loop.Run();

  EXPECT_TRUE(delegate->NoConsoleErrors());
}

// Valid icon measures the `kSuccess` histogram for chrome urls. This can't
// exist closer to `ManifestIconBrowserTest`, because the `shell()` does not
// load chrome urls.
IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallTestNoConsoleErrors,
                       ChromeUrlHistograms) {
  base::HistogramTester tester;
  GURL password_manager("chrome://password-manager/");

  // This involves loading and fetching the icons specified in the manifest, so
  // histograms are automatically measured.
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), password_manager));

  tester.ExpectBucketCount("WebApp.ManifestIconDownloader.Result",
                           content::ManifestIconDownloader::Result::kSuccess,
                           1);
  tester.ExpectBucketCount("WebApp.ManifestIconDownloader.ChromeUrl.Result",
                           content::ManifestIconDownloader::Result::kSuccess,
                           1);
}

}  // namespace web_app
