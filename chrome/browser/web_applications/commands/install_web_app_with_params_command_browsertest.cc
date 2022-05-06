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
#include "chrome/browser/web_applications/commands/install_web_app_with_params_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace web_app {

using InstallWebAppWithParamsCommandBrowserTest = WebAppControllerBrowserTest;

IN_PROC_BROWSER_TEST_F(InstallWebAppWithParamsCommandBrowserTest,
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
  auto install_params = ConvertExternalInstallOptionsToParams(install_options);
  auto install_source = ConvertExternalInstallSourceToInstallSource(
      install_options.install_source);

  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallWebAppWithParamsCommand>(
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          install_params, install_source, &provider().install_finalizer(),
          &provider().registrar(),
          base::BindLambdaForTesting(
              [&](const AppId& app_id, webapps::InstallResultCode code) {
                EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
                EXPECT_TRUE(provider().registrar().IsLocallyInstalled(app_id));
                run_loop.Quit();
              }),
          std::make_unique<WebAppDataRetriever>()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(InstallWebAppWithParamsCommandBrowserTest,
                       ExternalInstallWindowMode) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::RunLoop run_loop;
  ExternalInstallOptions install_options(
      kWebAppUrl, web_app::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);
  auto install_params = ConvertExternalInstallOptionsToParams(install_options);
  auto install_source = ConvertExternalInstallSourceToInstallSource(
      install_options.install_source);

  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallWebAppWithParamsCommand>(
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          install_params, install_source, &provider().install_finalizer(),
          &provider().registrar(),
          base::BindLambdaForTesting([&](const AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_TRUE(provider().registrar().IsLocallyInstalled(app_id));
            EXPECT_EQ(
                web_app::UserDisplayMode::kStandalone,
                provider().registrar().GetAppUserDisplayMode(app_id).value());
            run_loop.Quit();
          }),
          std::make_unique<WebAppDataRetriever>()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(InstallWebAppWithParamsCommandBrowserTest,
                       ExternalInstallBrowserMode) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::RunLoop run_loop;
  ExternalInstallOptions install_options(
      kWebAppUrl, web_app::UserDisplayMode::kBrowser,
      ExternalInstallSource::kInternalDefault);
  auto install_params = ConvertExternalInstallOptionsToParams(install_options);
  auto install_source = ConvertExternalInstallSourceToInstallSource(
      install_options.install_source);

  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallWebAppWithParamsCommand>(
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          install_params, install_source, &provider().install_finalizer(),
          &provider().registrar(),
          base::BindLambdaForTesting([&](const AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_TRUE(provider().registrar().IsLocallyInstalled(app_id));
            EXPECT_EQ(
                web_app::UserDisplayMode::kBrowser,
                provider().registrar().GetAppUserDisplayMode(app_id).value());
            run_loop.Quit();
          }),
          std::make_unique<WebAppDataRetriever>()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(InstallWebAppWithParamsCommandBrowserTest,
                       InstallAppFromPolicy) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::RunLoop run_loop;
  ExternalInstallOptions install_options(
      kWebAppUrl, web_app::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);
  auto install_params = ConvertExternalInstallOptionsToParams(install_options);
  auto install_source = ConvertExternalInstallSourceToInstallSource(
      install_options.install_source);

  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallWebAppWithParamsCommand>(
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          install_params, install_source, &provider().install_finalizer(),
          &provider().registrar(),
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
          std::make_unique<WebAppDataRetriever>()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(InstallWebAppWithParamsCommandBrowserTest,
                       InstallFailsWebContentsDestroyed) {
  const GURL kWebAppUrl("https://external_app.com");
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::RunLoop run_loop;
  ExternalInstallOptions install_options(
      kWebAppUrl, web_app::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);
  auto install_params = ConvertExternalInstallOptionsToParams(install_options);
  auto install_source = ConvertExternalInstallSourceToInstallSource(
      install_options.install_source);
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallWebAppWithParamsCommand>(
          web_contents->GetWeakPtr(), install_params, install_source,
          &provider().install_finalizer(), &provider().registrar(),
          base::BindLambdaForTesting([&](const AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kWebContentsDestroyed);
            EXPECT_FALSE(provider().registrar().IsLocallyInstalled(app_id));
            run_loop.Quit();
          }),
          std::make_unique<WebAppDataRetriever>()));

  web_contents->Close();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(InstallWebAppWithParamsCommandBrowserTest,
                       InstallFailsWithInvalidManifest) {
  const GURL kWebAppUrl("https://external_app.com");
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::RunLoop run_loop;
  ExternalInstallOptions install_options(
      kWebAppUrl, web_app::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);
  // This should force the install_params to have a valid manifest, otherwise
  // install will not happen.
  install_options.require_manifest = true;
  auto install_params = ConvertExternalInstallOptionsToParams(install_options);
  auto install_source = ConvertExternalInstallSourceToInstallSource(
      install_options.install_source);
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  provider().command_manager().ScheduleCommand(
      std::make_unique<InstallWebAppWithParamsCommand>(
          web_contents->GetWeakPtr(), install_params, install_source,
          &provider().install_finalizer(), &provider().registrar(),
          base::BindLambdaForTesting([&](const AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code,
                      webapps::InstallResultCode::kNotValidManifestForWebApp);
            EXPECT_FALSE(provider().registrar().IsLocallyInstalled(app_id));
            run_loop.Quit();
          }),
          std::make_unique<WebAppDataRetriever>()));

  run_loop.Run();
}

};  // namespace web_app
