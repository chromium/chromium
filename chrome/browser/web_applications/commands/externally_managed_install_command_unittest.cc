// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/externally_managed_install_command.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/commands/callback_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"

namespace web_app {
namespace {

class ExternallyManagedInstallCommandTest : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");
  const GURL kWebAppScope = GURL("https://example.com/path/");
  const AppId kWebAppId =
      GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);
  const GURL kWebAppManifestUrl =
      GURL("https://example.com/path/manifest.json");

  struct InstallResult {
    AppId installed_app_id;
    webapps::InstallResultCode install_code;
  };

  InstallResult InstallAndWait(
      const ExternalInstallOptions& install_options,
      std::unique_ptr<WebAppDataRetriever> data_retriever) {
    base::RunLoop run_loop;
    InstallResult result;
    provider()->command_manager().ScheduleCommand(
        std::make_unique<ExternallyManagedInstallCommand>(
            install_options,
            base::BindLambdaForTesting(
                [&](const AppId& app_id, webapps::InstallResultCode code) {
                  result.install_code = code;
                  result.installed_app_id = app_id;
                  run_loop.Quit();
                }),
            web_contents()->GetWeakPtr(), &provider()->install_finalizer(),

            std::move(data_retriever)));
    run_loop.Run();
    return result;
  }

  void SetUp() override {
    WebAppTest::SetUp();

    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->SetDefaultFakeSubsystems();
    provider->SetRunSubsystemStartupTasks(true);

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }
};

TEST_F(ExternallyManagedInstallCommandTest, Success) {
  ExternalInstallOptions install_options(
      kWebAppUrl, UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);

  auto data_retriever = std::make_unique<FakeDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);

  auto result = InstallAndWait(install_options, std::move(data_retriever));
  EXPECT_EQ(result.install_code,
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(
      provider()->registrar().IsLocallyInstalled(result.installed_app_id));
}

TEST_F(ExternallyManagedInstallCommandTest, GetWebAppInstallInfoFailed) {
  ExternalInstallOptions install_options(
      kWebAppUrl, UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);

  auto data_retriever = std::make_unique<FakeDataRetriever>();

  auto result = InstallAndWait(install_options, std::move(data_retriever));
  EXPECT_EQ(result.install_code,
            webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
  EXPECT_FALSE(
      provider()->registrar().IsLocallyInstalled(result.installed_app_id));
}

TEST_F(ExternallyManagedInstallCommandTest,
       InstallWebAppWithParams_DisplayModeFromWebAppInstallInfo) {
  {
    GURL url("https://example1.com/");
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = UserDisplayMode::kBrowser;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));

    ExternalInstallOptions install_options(
        url, /*user_display_mode=*/absl::nullopt,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    EXPECT_EQ(UserDisplayMode::kBrowser,
              provider()
                  ->registrar()
                  .GetAppById(result.installed_app_id)
                  ->user_display_mode());
  }
  {
    GURL url("https://example2.com/");
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = UserDisplayMode::kStandalone;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));

    ExternalInstallOptions install_options(
        url, /*user_display_mode=*/absl::nullopt,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    EXPECT_EQ(UserDisplayMode::kStandalone,
              provider()
                  ->registrar()
                  .GetAppById(result.installed_app_id)
                  ->user_display_mode());
  }
}

TEST_F(ExternallyManagedInstallCommandTest,
       InstallWebAppWithParams_DisplayModeOverrideByExternalInstallOptions) {
  {
    GURL url("https://example3.com/");
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = UserDisplayMode::kStandalone;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));

    ExternalInstallOptions install_options(
        url, UserDisplayMode::kBrowser,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    EXPECT_EQ(UserDisplayMode::kBrowser,
              provider()
                  ->registrar()
                  .GetAppById(result.installed_app_id)
                  ->user_display_mode());
  }
  {
    GURL url("https://example4.com/");
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = UserDisplayMode::kBrowser;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));

    ExternalInstallOptions install_options(
        url, UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    EXPECT_EQ(UserDisplayMode::kStandalone,
              provider()
                  ->registrar()
                  .GetAppById(result.installed_app_id)
                  ->user_display_mode());
  }
}

TEST_F(ExternallyManagedInstallCommandTest, UpgradeLock) {
  ExternalInstallOptions install_options(
      kWebAppUrl, UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);

  auto data_retriever = std::make_unique<FakeDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);

  base::flat_set<AppId> app_ids{
      GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl)};

  bool callback_command_run = false;
  auto callback_command = std::make_unique<CallbackCommand>(
      std::make_unique<AppLock>(app_ids),
      base::BindLambdaForTesting([&]() { callback_command_run = true; }));

  bool callback_command_2_run = false;
  base::RunLoop callback_runloop;
  auto callback_command_2 = std::make_unique<CallbackCommand>(
      std::make_unique<AppLock>(app_ids), base::BindLambdaForTesting([&]() {
        callback_command_2_run = true;
        callback_runloop.Quit();
      }));

  base::RunLoop run_loop;
  InstallResult result;
  auto command = std::make_unique<ExternallyManagedInstallCommand>(
      install_options,
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            result.install_code = code;
            result.installed_app_id = app_id;
            run_loop.Quit();
          }),
      web_contents()->GetWeakPtr(), &provider()->install_finalizer(),

      std::move(data_retriever));

  // Schedules another callback command that acquires the same app lock after
  // current command upgrades to app lock.
  command->SetOnLockUpgradedCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        provider()->command_manager().ScheduleCommand(
            std::move(callback_command_2));
      }));

  provider()->command_manager().ScheduleCommand(std::move(command));
  // Immediately schedule a callback command, this will request the app lock
  // before the ExternallyManagedInstallCommand.
  provider()->command_manager().ScheduleCommand(std::move(callback_command));

  run_loop.Run();

  EXPECT_EQ(result.install_code,
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(
      provider()->registrar().IsLocallyInstalled(result.installed_app_id));

  EXPECT_TRUE(callback_command_run);

  EXPECT_FALSE(callback_command_2_run);

  callback_runloop.Run();
  EXPECT_TRUE(callback_command_2_run);
}

}  // namespace
}  // namespace web_app
