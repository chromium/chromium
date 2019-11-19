// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_REGISTRY_CONTROLLER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_REGISTRY_CONTROLLER_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_install_delegate.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;

namespace web_app {

class TestWebAppDatabaseFactory;
class WebAppSyncBridge;
class WebApp;

class TestWebAppRegistryController : public SyncInstallDelegate {
 public:
  TestWebAppRegistryController();
  ~TestWebAppRegistryController();

  void SetUp(Profile* profile);

  // Synchronously init the sync bridge: open database, read all data and
  // metadata.
  void Init();

  void RegisterApp(std::unique_ptr<WebApp> web_app);
  void UnregisterApp(const AppId& app_id);
  void UnregisterAll();

  using InstallWebAppsAfterSyncDelegate =
      base::RepeatingCallback<void(std::vector<WebApp*> web_apps,
                                   RepeatingInstallCallback callback)>;
  void SetInstallWebAppsAfterSyncDelegate(
      InstallWebAppsAfterSyncDelegate delegate);

  using UninstallWebAppsAfterSyncDelegate = base::RepeatingCallback<void(
      std::vector<std::unique_ptr<WebApp>> web_apps,
      RepeatingUninstallCallback callback)>;
  void SetUninstallWebAppsAfterSyncDelegate(
      UninstallWebAppsAfterSyncDelegate delegate);

  // SyncInstallDelegate:
  void InstallWebAppsAfterSync(std::vector<WebApp*> web_apps,
                               RepeatingInstallCallback callback) override;
  void UninstallWebAppsAfterSync(std::vector<std::unique_ptr<WebApp>> web_apps,
                                 RepeatingUninstallCallback callback) override;

  void DestroySubsystems();

  TestWebAppDatabaseFactory& database_factory() { return *database_factory_; }
  WebAppRegistrar& registrar() { return *mutable_registrar_; }
  WebAppRegistrarMutable& mutable_registrar() { return *mutable_registrar_; }
  syncer::MockModelTypeChangeProcessor& processor() { return mock_processor_; }
  WebAppSyncBridge& sync_bridge() { return *sync_bridge_; }

 private:
  InstallWebAppsAfterSyncDelegate install_web_apps_after_sync_delegate_;
  UninstallWebAppsAfterSyncDelegate uninstall_web_apps_after_sync_delegate_;

  std::unique_ptr<TestWebAppDatabaseFactory> database_factory_;
  std::unique_ptr<WebAppRegistrarMutable> mutable_registrar_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<WebAppSyncBridge> sync_bridge_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_REGISTRY_CONTROLLER_H_
