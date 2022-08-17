// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/commands/externally_managed_install_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace web_app {

using ExternallyManagedInstallCommandBrowserTest = WebAppControllerBrowserTest;

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       BasicInstallCommand) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::RunLoop run_loop;
  ExternalInstallOptions install_options(
      kWebAppUrl,
      /*user_display_mode=*/absl::nullopt,
      ExternalInstallSource::kInternalDefault);

  provider().command_manager().ScheduleCommand(
      std::make_unique<ExternallyManagedInstallCommand>(
          install_options,
          base::BindLambdaForTesting(
              [&](const AppId& app_id, webapps::InstallResultCode code) {
                EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
                EXPECT_TRUE(provider().registrar().IsLocallyInstalled(app_id));
                run_loop.Quit();
              }),
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          &provider().install_finalizer(),
          std::make_unique<WebAppDataRetriever>()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       ExternalInstallWindowMode) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::RunLoop run_loop;
  ExternalInstallOptions install_options(
      kWebAppUrl, UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);

  provider().command_manager().ScheduleCommand(
      std::make_unique<ExternallyManagedInstallCommand>(
          install_options,
          base::BindLambdaForTesting([&](const AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_TRUE(provider().registrar().IsLocallyInstalled(app_id));
            EXPECT_EQ(
                UserDisplayMode::kStandalone,
                provider().registrar().GetAppUserDisplayMode(app_id).value());
            run_loop.Quit();
          }),
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          &provider().install_finalizer(),
          std::make_unique<WebAppDataRetriever>()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       ExternalInstallBrowserMode) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::RunLoop run_loop;
  ExternalInstallOptions install_options(
      kWebAppUrl, UserDisplayMode::kBrowser,
      ExternalInstallSource::kInternalDefault);

  provider().command_manager().ScheduleCommand(
      std::make_unique<ExternallyManagedInstallCommand>(

          install_options,
          base::BindLambdaForTesting([&](const AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_TRUE(provider().registrar().IsLocallyInstalled(app_id));
            EXPECT_EQ(
                UserDisplayMode::kBrowser,
                provider().registrar().GetAppUserDisplayMode(app_id).value());
            run_loop.Quit();
          }),
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          &provider().install_finalizer(),

          std::make_unique<WebAppDataRetriever>()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       InstallAppFromPolicy) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::RunLoop run_loop;
  ExternalInstallOptions install_options(
      kWebAppUrl, UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);

  provider().command_manager().ScheduleCommand(
      std::make_unique<ExternallyManagedInstallCommand>(
          install_options,
          base::BindLambdaForTesting(
              [&](const AppId& app_id, webapps::InstallResultCode code) {
                EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
                EXPECT_TRUE(provider().registrar().IsLocallyInstalled(app_id));
                EXPECT_TRUE(provider()
                                .registrar()
                                .GetAppById(app_id)
                                ->IsPolicyInstalledApp());
                run_loop.Quit();
              }),
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          &provider().install_finalizer(),
          std::make_unique<WebAppDataRetriever>()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       InstallFailsWebContentsDestroyed) {
  const GURL kWebAppUrl("https://external_app.com");
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::RunLoop run_loop;
  ExternalInstallOptions install_options(
      kWebAppUrl, UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  provider().command_manager().ScheduleCommand(
      std::make_unique<ExternallyManagedInstallCommand>(
          install_options,
          base::BindLambdaForTesting([&](const AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kWebContentsDestroyed);
            EXPECT_FALSE(provider().registrar().IsLocallyInstalled(app_id));
            run_loop.Quit();
          }),
          web_contents->GetWeakPtr(), &provider().install_finalizer(),

          std::make_unique<WebAppDataRetriever>()));

  web_contents->Close();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       InstallFailsWithInvalidManifest) {
  const GURL kWebAppUrl("https://external_app.com");
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::RunLoop run_loop;
  ExternalInstallOptions install_options(
      kWebAppUrl, UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);
  // This should force the install_params to have a valid manifest, otherwise
  // install will not happen.
  install_options.require_manifest = true;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  provider().command_manager().ScheduleCommand(
      std::make_unique<ExternallyManagedInstallCommand>(
          install_options,
          base::BindLambdaForTesting([&](const AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code,
                      webapps::InstallResultCode::kNotValidManifestForWebApp);
            EXPECT_FALSE(provider().registrar().IsLocallyInstalled(app_id));
            run_loop.Quit();
          }),
          web_contents->GetWeakPtr(), &provider().install_finalizer(),

          std::make_unique<WebAppDataRetriever>()));

  run_loop.Run();
}

};  // namespace web_app
