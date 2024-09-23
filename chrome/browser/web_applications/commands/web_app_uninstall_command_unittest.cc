// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/buildflag.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_url_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/mock_file_utils_wrapper.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace web_app {
namespace {

class WebAppUninstallCommandTest : public WebAppTest {
 public:
  WebAppUninstallCommandTest() = default;

  void SetUp() override {
    WebAppTest::SetUp();

    file_utils_wrapper_ =
        base::MakeRefCounted<testing::StrictMock<MockFileUtilsWrapper>>();
    fake_provider().SetFileUtils(file_utils_wrapper_);
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    file_utils_wrapper_ = nullptr;
    WebAppTest::TearDown();
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  scoped_refptr<testing::StrictMock<MockFileUtilsWrapper>> file_utils_wrapper_;
};

TEST_F(WebAppUninstallCommandTest, SimpleUninstallInternal) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  webapps::AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update =
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::test::TestFuture<webapps::UninstallResultCode> result_future;
  provider()->scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  EXPECT_EQ(webapps::UninstallResultCode::kAppRemoved, result_future.Get());
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, SimpleUninstallExternal) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kDefault);
  webapps::AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update =
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::test::TestFuture<webapps::UninstallResultCode> result_future;
  provider()->scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  EXPECT_EQ(webapps::UninstallResultCode::kAppRemoved, result_future.Get());
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, FailedDataDeletionOrOsHookRemoval) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  webapps::AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update =
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(false));

  base::test::TestFuture<webapps::UninstallResultCode> result_future;
  provider()->scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  EXPECT_EQ(webapps::UninstallResultCode::kError, result_future.Get());
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, TryToUninstallNonExistentApp) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  webapps::AppId app_id = web_app->app_id();

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .Times(0);

  base::test::TestFuture<webapps::UninstallResultCode> result_future;
  provider()->scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  EXPECT_EQ(webapps::UninstallResultCode::kNoAppToUninstall,
            result_future.Get());
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, CommandManagerShutdownThrowsError) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  webapps::AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update =
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .Times(0);

  provider()->scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(webapps::UninstallResultCode::kShutdown, code);
      }));

  provider()->command_manager().Shutdown();
  // App is not uninstalled.
  EXPECT_NE(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, UserUninstalledPrefsFilled) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kDefault);
  webapps::AppId app_id = web_app->app_id();
  web_app->AddInstallURLToManagementExternalConfigMap(
      WebAppManagement::kDefault, GURL("https://www.example.com/install"));
  {
    ScopedRegistryUpdate update =
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }
  EXPECT_FALSE(UserUninstalledPreinstalledWebAppPrefs(profile()->GetPrefs())
                   .DoesAppIdExist(app_id));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::test::TestFuture<webapps::UninstallResultCode> future;
  provider()->scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(webapps::UninstallResultCode::kAppRemoved, future.Get());
  EXPECT_EQ(provider()->registrar_unsafe().GetAppById(app_id), nullptr);
  EXPECT_TRUE(UserUninstalledPreinstalledWebAppPrefs(profile()->GetPrefs())
                  .DoesAppIdExist(app_id));
}

TEST_F(WebAppUninstallCommandTest, RemoveSourceAndTriggerOSUninstallation) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kDefault);
  web_app->AddInstallURLToManagementExternalConfigMap(
      WebAppManagement::kDefault, GURL("https://example.com/install"));
  web_app->AddSource(WebAppManagement::kPolicy);
  web_app->AddInstallURLToManagementExternalConfigMap(
      WebAppManagement::kPolicy, GURL("https://example.com/install"));
  EXPECT_FALSE(web_app->CanUserUninstallWebApp());
  webapps::AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update =
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .Times(0);

  base::RunLoop run_loop;
  auto command = WebAppUninstallCommand::CreateForRemoveInstallManagements(
      webapps::WebappUninstallSource::kExternalPolicy, *profile(), app_id,
      {WebAppManagement::kPolicy},
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(webapps::UninstallResultCode::kInstallSourceRemoved, code);
        run_loop.Quit();
      }));

  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppSourceRemovedDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& app_id) {
        // The policy source will be removed and WebAppOsUninstallation is
        // registered.
        EXPECT_FALSE(provider()
                         ->registrar_unsafe()
                         .GetAppById(app_id)
                         ->IsPolicyInstalledApp());
      }));

  provider()->command_manager().ScheduleCommand(std::move(command));
  run_loop.Run();

  // It should still be around from the default install.
  EXPECT_THAT(provider()->registrar_unsafe().LookUpAppIdByInstallUrl(
                  GURL("https://example.com/install")),
              testing::Eq(app_id));
  // But it shouldn't be there for policy.
  EXPECT_EQ(provider()->registrar_unsafe().LookUpAppByInstallSourceInstallUrl(
                WebAppManagement::kPolicy, GURL("https://example.com/install")),
            nullptr);
}

TEST_F(WebAppUninstallCommandTest, Shutdown) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  webapps::AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update =
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  base::test::TestFuture<webapps::UninstallResultCode> future;
  provider()->scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu, future.GetCallback());
  provider()->Shutdown();
  ASSERT_TRUE(future.Wait());
  // Shutdown occurs before the install can finish, so the app should not be
  // removed.
  EXPECT_EQ(future.Get(), webapps::UninstallResultCode::kShutdown);
  EXPECT_NE(provider()->registrar_unsafe().GetAppById(app_id), nullptr);

  // Test post-shutdown behavior.
  base::test::TestFuture<webapps::UninstallResultCode> future2;
  provider()->scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu, future2.GetCallback());
  provider()->Shutdown();
  ASSERT_TRUE(future2.Wait());
  EXPECT_EQ(future2.Get(), webapps::UninstallResultCode::kShutdown);
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
  webapps::AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update =
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::test::TestFuture<webapps::UninstallResultCode> result_future;
  provider()->scheduler().RemoveUserUninstallableManagements(
      app_id, GetParam().source, result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  EXPECT_EQ(webapps::UninstallResultCode::kAppRemoved, result_future.Get());
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
