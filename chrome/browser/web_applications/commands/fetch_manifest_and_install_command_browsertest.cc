// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
  WebAppInstallDialogCallback CreateDialogCallback(bool accept = true) {
    return base::BindOnce(
        [](bool accept, content::WebContents* initiator_web_contents,
           std::unique_ptr<WebAppInstallInfo> web_app_info,
           WebAppInstallationAcceptanceCallback acceptance_callback) {
          std::move(acceptance_callback).Run(accept, std::move(web_app_info));
        },
        accept);
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
    ScopedRegistryUpdate update(&provider().sync_bridge_unsafe());
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
      /*bypass_service_worker_check=*/false, CreateDialogCallback(),
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            loop.Quit();
          }),
      /*use_fallback=*/true);
  loop.Run();
  EXPECT_TRUE(provider().registrar_unsafe().IsLocallyInstalled(app_id));

  // Install defaults to `kBrowser` because `CreateDialogCallback` doesn't set
  // `open_as_window` to true.
  EXPECT_EQ(provider().registrar_unsafe().GetAppUserDisplayMode(app_id).value(),
            mojom::UserDisplayMode::kBrowser);
}

}  // namespace web_app
