// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_REGISTRY_CONTROLLER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_REGISTRY_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/test/test_os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_install_delegate.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/test/model/mock_model_type_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

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

  void ApplySyncChanges_AddApps(std::vector<GURL> apps_to_add);
  void ApplySyncChanges_UpdateApps(
      const std::vector<std::unique_ptr<WebApp>>& apps_server_state);
  void ApplySyncChanges_DeleteApps(std::vector<AppId> app_ids_to_delete);

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
  TestOsIntegrationManager& os_integration_manager() {
    return *os_integration_manager_;
  }

 private:
  InstallWebAppsAfterSyncDelegate install_web_apps_after_sync_delegate_;
  UninstallWebAppsAfterSyncDelegate uninstall_web_apps_after_sync_delegate_;

  std::unique_ptr<TestWebAppDatabaseFactory> database_factory_;
  std::unique_ptr<WebAppRegistrarMutable> mutable_registrar_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<WebAppSyncBridge> sync_bridge_;
  std::unique_ptr<TestOsIntegrationManager> os_integration_manager_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_REGISTRY_CONTROLLER_H_
