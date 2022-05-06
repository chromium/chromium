// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_uninstall_job.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/mock_file_utils_wrapper.h"
#include "chrome/browser/web_applications/test/mock_os_integration_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace web_app {
namespace {

class WebAppUninstallJobTest : public WebAppTest {
 public:
  WebAppUninstallJobTest() = default;

  void SetUp() override {
    WebAppTest::SetUp();

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();

    controller().SetUp(profile());

    install_manager_ = std::make_unique<WebAppInstallManager>(profile());

    file_utils_wrapper_ =
        base::MakeRefCounted<testing::StrictMock<MockFileUtilsWrapper>>();
    icon_manager_ =
        std::make_unique<WebAppIconManager>(profile(), file_utils_wrapper_);
    icon_manager_->SetSubsystems(&controller().registrar(), &install_manager());
  }

  void TearDown() override {
    file_utils_wrapper_ = nullptr;
    WebAppTest::TearDown();
  }

  FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }

  WebAppInstallManager& install_manager() const { return *install_manager_; }

  WebAppInstallFinalizer& install_finalizer() const {
    return *install_finalizer_;
  }

  testing::StrictMock<MockOsIntegrationManager> os_integration_manager_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<WebAppInstallFinalizer> install_finalizer_;
  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  scoped_refptr<testing::StrictMock<MockFileUtilsWrapper>> file_utils_wrapper_;
};

TEST_F(WebAppUninstallJobTest, SimpleUninstall) {
  Registry registry;
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId id = web_app->app_id();
  registry.emplace(id, std::move(web_app));
  controller().database_factory().WriteRegistry(registry);
  controller().Init();

  WebAppUninstallJob task(&os_integration_manager_, &controller().sync_bridge(),
                          icon_manager_.get(), &controller().registrar(),
                          &install_manager(), &install_finalizer(),
                          &controller().translation_manager(),
                          profile()->GetPrefs());

  OsHooksErrors result;
  EXPECT_CALL(os_integration_manager_, UninstallAllOsHooks(id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::RunLoop loop;
  task.Start(id, url::Origin(), webapps::WebappUninstallSource::kAppMenu,
             base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
               EXPECT_EQ(webapps::UninstallResultCode::kSuccess, code);
               loop.Quit();
             }));
  loop.Run();

  EXPECT_EQ(controller().registrar().GetAppById(id), nullptr);
}

TEST_F(WebAppUninstallJobTest, FailedDataDelete) {
  Registry registry;
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId id = web_app->app_id();
  registry.emplace(id, std::move(web_app));
  controller().database_factory().WriteRegistry(registry);
  controller().Init();

  WebAppUninstallJob task(&os_integration_manager_, &controller().sync_bridge(),
                          icon_manager_.get(), &controller().registrar(),
                          &install_manager(), &install_finalizer(),
                          &controller().translation_manager(),
                          profile()->GetPrefs());

  OsHooksErrors result;
  EXPECT_CALL(os_integration_manager_, UninstallAllOsHooks(id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(false));

  base::RunLoop loop;
  task.Start(id, url::Origin(), webapps::WebappUninstallSource::kAppMenu,
             base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
               EXPECT_EQ(webapps::UninstallResultCode::kError, code);
               loop.Quit();
             }));
  loop.Run();

  EXPECT_EQ(controller().registrar().GetAppById(id), nullptr);
}

TEST_F(WebAppUninstallJobTest, FailedOsHooks) {
  Registry registry;
  auto web_app = test::CreateWebApp(GURL("https://www.example.com"),
                                    WebAppManagement::kSync);
  AppId id = web_app->app_id();
  registry.emplace(id, std::move(web_app));
  controller().database_factory().WriteRegistry(registry);
  controller().Init();

  WebAppUninstallJob task(&os_integration_manager_, &controller().sync_bridge(),
                          icon_manager_.get(), &controller().registrar(),
                          &install_manager(), &install_finalizer(),
                          &controller().translation_manager(),
                          profile()->GetPrefs());

  OsHooksErrors result;
  result.set(true);
  EXPECT_CALL(os_integration_manager_, UninstallAllOsHooks(id, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(result));

  base::FilePath deletion_path = GetManifestResourcesDirectoryForApp(
      GetWebAppsRootDirectory(profile()), id);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively(deletion_path))
      .WillOnce(testing::Return(true));

  base::RunLoop loop;
  task.Start(id, url::Origin(), webapps::WebappUninstallSource::kAppMenu,
             base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
               EXPECT_EQ(webapps::UninstallResultCode::kError, code);
               loop.Quit();
             }));
  loop.Run();

  EXPECT_EQ(controller().registrar().GetAppById(id), nullptr);
}

}  // namespace
}  // namespace web_app
