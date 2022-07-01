// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/mock_file_utils_wrapper.h"
#include "chrome/browser/web_applications/test/mock_os_integration_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/prefs/testing_pref_service.h"
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

    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->SetDefaultFakeSubsystems();
    file_utils_wrapper_ =
        base::MakeRefCounted<testing::StrictMock<MockFileUtilsWrapper>>();
    provider->SetIconManager(
        std::make_unique<WebAppIconManager>(profile(), file_utils_wrapper_));
    provider->SetRunSubsystemStartupTasks(true);
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    file_utils_wrapper_ = nullptr;
    WebAppTest::TearDown();
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  testing::StrictMock<MockOsIntegrationManager> os_integration_manager_;
  scoped_refptr<testing::StrictMock<MockFileUtilsWrapper>> file_utils_wrapper_;
};

TEST_F(WebAppUninstallCommandTest, SimpleUninstall) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }

  OsHooksErrors result;
  EXPECT_CALL(os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, url::Origin(), profile(), &os_integration_manager_,
          &provider()->sync_bridge(), &provider()->icon_manager(),
          &provider()->registrar(), &provider()->install_manager(),
          &provider()->install_finalizer(), &provider()->translation_manager(),
          webapps::WebappUninstallSource::kAppMenu,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kSuccess, code);
            loop.Quit();
          })));

  loop.Run();
  EXPECT_EQ(provider()->registrar().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, FailedDataDelete) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }

  OsHooksErrors result;
  EXPECT_CALL(os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(false));

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, url::Origin(), profile(), &os_integration_manager_,
          &provider()->sync_bridge(), &provider()->icon_manager(),
          &provider()->registrar(), &provider()->install_manager(),
          &provider()->install_finalizer(), &provider()->translation_manager(),
          webapps::WebappUninstallSource::kAppMenu,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kError, code);
            loop.Quit();
          })));

  loop.Run();
  EXPECT_EQ(provider()->registrar().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, FailedOsHooks) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }

  OsHooksErrors result;
  result.set(true);
  EXPECT_CALL(os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, url::Origin(), profile(), &os_integration_manager_,
          &provider()->sync_bridge(), &provider()->icon_manager(),
          &provider()->registrar(), &provider()->install_manager(),
          &provider()->install_finalizer(), &provider()->translation_manager(),
          webapps::WebappUninstallSource::kAppMenu,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kError, code);
            loop.Quit();
          })));

  loop.Run();
  EXPECT_EQ(provider()->registrar().GetAppById(app_id), nullptr);
}

TEST_F(WebAppUninstallCommandTest, UninstallNonExistentApp) {
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId app_id = web_app->app_id();

  EXPECT_CALL(os_integration_manager_, UninstallAllOsHooks(app_id, testing::_))
      .Times(0);

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), app_id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .Times(0);

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, url::Origin(), profile(), &os_integration_manager_,
          &provider()->sync_bridge(), &provider()->icon_manager(),
          &provider()->registrar(), &provider()->install_manager(),
          &provider()->install_finalizer(), &provider()->translation_manager(),
          webapps::WebappUninstallSource::kAppMenu,
          base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
            EXPECT_EQ(webapps::UninstallResultCode::kNoAppToUninstall, code);
            loop.Quit();
          })));

  loop.Run();
  EXPECT_EQ(provider()->registrar().GetAppById(app_id), nullptr);
}

}  // namespace
}  // namespace web_app
