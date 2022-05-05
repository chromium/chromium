// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

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

  auto* provider = WebAppProvider::GetForTest(profile());
  base::RunLoop loop;
  provider->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          &provider->install_finalizer(), &provider->registrar(),
          webapps::WebappInstallSource::MENU_BROWSER_TAB,
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          /*bypass_service_worker_check=*/false, CreateDialogCallback(),
          base::BindLambdaForTesting(
              [&](const AppId& app_id, webapps::InstallResultCode code) {
                EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
                EXPECT_TRUE(provider->registrar().IsLocallyInstalled(app_id));
                loop.Quit();
              })));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest, MultipleInstalls) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  auto* provider = WebAppProvider::GetForTest(profile());

  // Schedule two installs and both succeed.
  base::RunLoop loop;
  provider->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          &provider->install_finalizer(), &provider->registrar(),
          webapps::WebappInstallSource::MENU_BROWSER_TAB,
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          /*bypass_service_worker_check=*/false, CreateDialogCallback(),
          base::BindLambdaForTesting(
              [&](const AppId& app_id, webapps::InstallResultCode code) {
                EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
                EXPECT_TRUE(provider->registrar().IsLocallyInstalled(app_id));
              })));

  provider->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          &provider->install_finalizer(), &provider->registrar(),
          webapps::WebappInstallSource::MENU_BROWSER_TAB,
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          /*bypass_service_worker_check=*/false, CreateDialogCallback(),
          base::BindLambdaForTesting(
              [&](const AppId& app_id, webapps::InstallResultCode code) {
                EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
                EXPECT_TRUE(provider->registrar().IsLocallyInstalled(app_id));
                loop.Quit();
              })));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest, InvalidManifest) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "no_manifest_test_page.html");
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));

  auto* provider = WebAppProvider::GetForTest(profile());

  base::RunLoop loop;
  provider->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          &provider->install_finalizer(), &provider->registrar(),
          webapps::WebappInstallSource::MENU_BROWSER_TAB,
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          /*bypass_service_worker_check=*/false, CreateDialogCallback(),
          base::BindLambdaForTesting([&](const AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code,
                      webapps::InstallResultCode::kNotValidManifestForWebApp);
            EXPECT_FALSE(provider->registrar().IsLocallyInstalled(app_id));
            loop.Quit();
          })));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest, UserDeclineInstall) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));
  auto* provider = WebAppProvider::GetForTest(profile());

  base::RunLoop loop;
  provider->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          &provider->install_finalizer(), &provider->registrar(),
          webapps::WebappInstallSource::MENU_BROWSER_TAB,
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          /*bypass_service_worker_check=*/false,
          CreateDialogCallback(/*accept=*/false),
          base::BindLambdaForTesting([&](const AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kUserInstallDeclined);
            EXPECT_FALSE(provider->registrar().IsLocallyInstalled(app_id));
            loop.Quit();
          })));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest,
                       HandleWebContentsDestroyed) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));
  auto* provider = WebAppProvider::GetForTest(profile());

  base::RunLoop loop;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  provider->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          &provider->install_finalizer(), &provider->registrar(),
          webapps::WebappInstallSource::MENU_BROWSER_TAB,
          web_contents->GetWeakPtr(),
          /*bypass_service_worker_check=*/false, CreateDialogCallback(),
          base::BindLambdaForTesting([&](const AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kWebContentsDestroyed);
            EXPECT_FALSE(provider->registrar().IsLocallyInstalled(app_id));
            loop.Quit();
          })));

  web_contents->Close();

  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchManifestAndInstallCommandTest,
                       InstallWithFallback) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "no_manifest_test_page.html");
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), test_url));
  auto* provider = WebAppProvider::GetForTest(profile());

  base::RunLoop loop;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  provider->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          &provider->install_finalizer(), &provider->registrar(),
          webapps::WebappInstallSource::MENU_BROWSER_TAB,
          web_contents->GetWeakPtr(),
          /*bypass_service_worker_check=*/false, CreateDialogCallback(),
          base::BindLambdaForTesting(
              [&](const AppId& app_id, webapps::InstallResultCode code) {
                EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
                EXPECT_TRUE(provider->registrar().IsLocallyInstalled(app_id));
                loop.Quit();
              }),
          /*use_fallback=*/true, WebAppInstallFlow::kInstallSite));
  loop.Run();
}

}  // namespace web_app
