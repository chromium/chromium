// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/buildflag.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/mock_file_utils_wrapper.h"
#include "chrome/browser/web_applications/test/mock_os_integration_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/gmock_callback_support.h"
#endif

namespace web_app {
namespace {

class WebAppUninstallCommandTest : public WebAppTest {
 public:
  WebAppUninstallCommandTest() = default;

  void SetUp() override {
    WebAppTest::SetUp();

    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    file_utils_wrapper_ =
        base::MakeRefCounted<testing::StrictMock<MockFileUtilsWrapper>>();
    provider->SetIconManager(
        std::make_unique<WebAppIconManager>(profile(), file_utils_wrapper_));
    auto manager =
        std::make_unique<testing::StrictMock<MockOsIntegrationManager>>();
    os_integration_manager_ = manager.get();
    provider->SetOsIntegrationManager(std::move(manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    file_utils_wrapper_ = nullptr;
    WebAppTest::TearDown();
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  raw_ptr<testing::StrictMock<MockOsIntegrationManager>>
      os_integration_manager_;
  scoped_refptr<testing::StrictMock<MockFileUtilsWrapper>> file_utils_wrapper_;
  base::HistogramTester histogram_tester_;
};

TEST_F(WebAppUninstallCommandTest, SimpleUninstallInternal) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }

  OsHooksErrors result;
  EXPECT_CALL(*os_integration_manager_, Synchronize(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());
  EXPECT_CALL(*os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, absl::nullopt, webapps::WebappUninstallSource::kAppMenu,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kSuccess, code);
            loop.Quit();
          }),
          profile()));

  loop.Run();
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, SimpleUninstallExternal) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kDefault);
  AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }

  OsHooksErrors result;
  EXPECT_CALL(*os_integration_manager_, Synchronize(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());
  EXPECT_CALL(*os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, WebAppManagement::kDefault,
          webapps::WebappUninstallSource::kAppMenu,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kSuccess, code);
            loop.Quit();
          }),
          profile()));

  loop.Run();
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, FailedDataDeletion) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }

  OsHooksErrors result;
  EXPECT_CALL(*os_integration_manager_, Synchronize(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());
  EXPECT_CALL(*os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(false));

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, absl::nullopt, webapps::WebappUninstallSource::kAppMenu,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kError, code);
            loop.Quit();
          }),
          profile()));

  loop.Run();
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, FailedOsHooksSetting) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }

  OsHooksErrors result;
  result.set(true);
  EXPECT_CALL(*os_integration_manager_, Synchronize(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());
  EXPECT_CALL(*os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, absl::nullopt, webapps::WebappUninstallSource::kAppMenu,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kError, code);
            loop.Quit();
          }),
          profile()));

  loop.Run();
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, TryToUninstallNonExistentApp) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId app_id = web_app->app_id();

  EXPECT_CALL(*os_integration_manager_, Synchronize(app_id, testing::_))
      .Times(0);
  EXPECT_CALL(*os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .Times(0);

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .Times(0);

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, absl::nullopt, webapps::WebappUninstallSource::kAppMenu,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kNoAppToUninstall, code);
            loop.Quit();
          }),
          profile()));

  loop.Run();
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, CommandManagerShutdownThrowsError) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }

  EXPECT_CALL(*os_integration_manager_, Synchronize(app_id, testing::_))
      .Times(0);
  EXPECT_CALL(*os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .Times(0);

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .Times(0);

  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, absl::nullopt, webapps::WebappUninstallSource::kAppMenu,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kError, code);
          }),
          profile()));

  provider()->command_manager().Shutdown();
  // App is not uninstalled.
  EXPECT_NE(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, UserUninstalledPrefsFilled) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kDefault);
  AppId app_id = web_app->app_id();
  web_app->AddInstallURLToManagementExternalConfigMap(
      WebAppManagement::kDefault, GURL("https://www.example.com/install"));
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }
  EXPECT_FALSE(UserUninstalledPreinstalledWebAppPrefs(profile()->GetPrefs())
                   .DoesAppIdExist(app_id));

  OsHooksErrors result;
  EXPECT_CALL(*os_integration_manager_, Synchronize(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());
  EXPECT_CALL(*os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, absl::nullopt, webapps::WebappUninstallSource::kAppMenu,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kSuccess, code);
            loop.Quit();
          }),
          profile()));

  loop.Run();
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
  EXPECT_TRUE(UserUninstalledPreinstalledWebAppPrefs(profile()->GetPrefs())
                  .DoesAppIdExist(app_id));
}

TEST_F(WebAppUninstallCommandTest, ExternalConfigMapMissing) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kDefault);
  AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }
  EXPECT_TRUE(provider()->registrar_unsafe().IsLocallyInstalled(app_id));
  OsHooksErrors result;
  EXPECT_CALL(*os_integration_manager_, Synchronize(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());
  EXPECT_CALL(*os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, absl::nullopt, webapps::WebappUninstallSource::kAppMenu,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kSuccess, code);
            loop.Quit();
          }),
          profile()));

  loop.Run();
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "WebApp.Preinstalled.ExternalConfigMapAbsentDuringUninstall"),
              BucketsAre(base::Bucket(true, 1)));
}

TEST_F(WebAppUninstallCommandTest, RemoveSourceAndTriggerOSUninstallation) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kDefault);
  web_app->AddSource(WebAppManagement::kPolicy);
  web_app->AddInstallURLToManagementExternalConfigMap(
      WebAppManagement::kDefault, GURL("https://example.com/install"));
  EXPECT_FALSE(web_app->CanUserUninstallWebApp());
  AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }

  EXPECT_CALL(*os_integration_manager_, Synchronize(app_id, testing::_))
      .Times(0);
  EXPECT_CALL(*os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .Times(0);

// This is called once on Windows because OsUninstallRegistration is limited to
// WIN.
#if BUILDFLAG(IS_WIN)
  EXPECT_CALL(*os_integration_manager_,
              MacAppShimOnAppInstalledForProfile(app_id))
      .Times(1);
  EXPECT_CALL(*os_integration_manager_,
              RegisterWebAppOsUninstallation(app_id, testing::_))
      .Times(1);
  EXPECT_CALL(*os_integration_manager_, Synchronize(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());
#else
  EXPECT_CALL(*os_integration_manager_,
              RegisterWebAppOsUninstallation(app_id, testing::_))
      .Times(0);
#endif

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .Times(0);

  base::RunLoop run_loop;
  auto command = std::make_unique<WebAppUninstallCommand>(
      app_id, WebAppManagement::kPolicy,
      webapps::WebappUninstallSource::kExternalPolicy,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(webapps::UninstallResultCode::kSuccess, code);
        run_loop.Quit();
      }),
      profile());

  command->SetRemoveManagementTypeCallbackForTesting(
      base::BindLambdaForTesting([&](const AppId& app_id) {
        // The policy source will be removed and WebAppOsUninstallation is
        // registered.
        EXPECT_FALSE(provider()
                         ->registrar_unsafe()
                         .GetAppById(app_id)
                         ->IsPolicyInstalledApp());
      }));
  provider()->command_manager().ScheduleCommand(std::move(command));
  run_loop.Run();
}

struct UninstallSources {
  webapps::WebappUninstallSource source;
};

class WebAppUninstallCommandSourceTest
    : public WebAppUninstallCommandTest,
      public testing::WithParamInterface<UninstallSources> {
 public:
  WebAppUninstallCommandSourceTest() = default;
  ~WebAppUninstallCommandSourceTest() override = default;
};

// Verify an app can be uninstalled with non-external uninstall_sources and
// Sync.
TEST_P(WebAppUninstallCommandSourceTest, RunTestForUninstallSource) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }

  OsHooksErrors result;
  EXPECT_CALL(*os_integration_manager_, Synchronize(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());
  EXPECT_CALL(*os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, absl::nullopt, GetParam().source,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kSuccess, code);
            loop.Quit();
          }),
          profile()));

  loop.Run();
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppUninstallCommandSourceTest,
    ::testing::Values(
        UninstallSources({webapps::WebappUninstallSource::kAppMenu}),
        UninstallSources({webapps::WebappUninstallSource::kAppsPage}),
        UninstallSources({webapps::WebappUninstallSource::kOsSettings}),
        UninstallSources({webapps::WebappUninstallSource::kAppManagement}),
        UninstallSources({webapps::WebappUninstallSource::kMigration}),
        UninstallSources({webapps::WebappUninstallSource::kAppList}),
        UninstallSources({webapps::WebappUninstallSource::kShelf}),
        UninstallSources({webapps::WebappUninstallSource::kSync}),
        UninstallSources({webapps::WebappUninstallSource::kStartupCleanup})));

}  // namespace
}  // namespace web_app
