// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class FetchManifestAndInstallCommandTest : public WebAppControllerBrowserTest {
 public:
  WebAppInstallDialogCallback CreateDialogCallback(
      bool accept = true,
      mojom::UserDisplayMode user_display_mode =
          mojom::UserDisplayMode::kStandalone) {
    return base::BindLambdaForTesting(
        [accept, user_display_mode](
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
      /*bypass_service_worker_check=*/false, CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_TRUE(
                provider().registrar_unsafe().IsLocallyInstalled(app_id));
            loop.Quit();
          }),
      /*use_fallback=*/false);
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest, MultipleInstalls) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  // Schedule two installs and both succeed.
  base::RunLoop loop;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      /*bypass_service_worker_check=*/false, CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_TRUE(
                provider().registrar_unsafe().IsLocallyInstalled(app_id));
          }),
      /*use_fallback=*/false);

  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      /*bypass_service_worker_check=*/false, CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_TRUE(
                provider().registrar_unsafe().IsLocallyInstalled(app_id));
            loop.Quit();
          }),
      /*use_fallback=*/false);
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
      /*bypass_service_worker_check=*/false, CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code,
                      webapps::InstallResultCode::kNotValidManifestForWebApp);
            EXPECT_FALSE(
                provider().registrar_unsafe().IsLocallyInstalled(app_id));
            loop.Quit();
          }),
      /*use_fallback=*/false);
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
      /*bypass_service_worker_check=*/false,
      CreateDialogCallback(/*accept=*/false),
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kUserInstallDeclined);
            EXPECT_FALSE(
                provider().registrar_unsafe().IsLocallyInstalled(app_id));
            loop.Quit();
          }),
      /*use_fallback=*/false);
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
      web_contents->GetWeakPtr(),
      /*bypass_service_worker_check=*/false, CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kWebContentsDestroyed);
            EXPECT_FALSE(
                provider().registrar_unsafe().IsLocallyInstalled(app_id));
            loop.Quit();
          }),
      /*use_fallback=*/false);

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
      web_contents->GetWeakPtr(),
      /*bypass_service_worker_check=*/false, CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_TRUE(
                provider().registrar_unsafe().IsLocallyInstalled(app_id));
            loop.Quit();
          }),
      /*use_fallback=*/true);
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest,
                       InstallWithFallbackOverwriteInstalled) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "no_manifest_test_page.html");
  auto web_app = test::CreateWebApp(test_url);
  const AppId app_id = web_app->app_id();

  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
    web_app->SetIsLocallyInstalled(false);
    update->CreateApp(std::move(web_app));
  }

  EXPECT_FALSE(provider().registrar_unsafe().IsLocallyInstalled(app_id));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppUserDisplayMode(app_id).value(),
            mojom::UserDisplayMode::kStandalone);

  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::RunLoop loop;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      web_contents->GetWeakPtr(),
      /*bypass_service_worker_check=*/false,
      CreateDialogCallback(
          /*accept=*/true,
          /*user_display_mode=*/mojom::UserDisplayMode::kStandalone),
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            loop.Quit();
          }),
      /*use_fallback=*/true);
  loop.Run();
  EXPECT_TRUE(provider().registrar_unsafe().IsLocallyInstalled(app_id));

  EXPECT_EQ(provider().registrar_unsafe().GetAppUserDisplayMode(app_id).value(),
            mojom::UserDisplayMode::kStandalone);
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest,
                       InstallFromOutsideScopeToolbarHasBackButton) {
  GURL test_url = https_server()->GetURL("/banners/app_with_nested/index.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  base::test::TestFuture<const AppId&, webapps::InstallResultCode>
      install_future;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      /*bypass_service_worker_check=*/false, CreateDialogCallback(),
      install_future.GetCallback(),
      /*use_fallback=*/false);
  ASSERT_TRUE(install_future.Wait());
  EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kSuccessNewInstall);
  AppId app_id = install_future.Get<AppId>();
  EXPECT_TRUE(provider().registrar_unsafe().IsLocallyInstalled(app_id));
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

}  // namespace web_app
