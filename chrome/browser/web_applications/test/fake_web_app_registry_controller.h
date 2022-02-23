// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_REGISTRY_CONTROLLER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_REGISTRY_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/web_applications/test/fake_externally_managed_app_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_install_delegate.h"
#include "components/sync/test/model/mock_model_type_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

class FakeWebAppDatabaseFactory;
class WebAppSyncBridge;
class WebAppTranslationManager;
class WebApp;
class WebAppPolicyManager;

class FakeWebAppRegistryController : public SyncInstallDelegate {
 public:
  FakeWebAppRegistryController();
  ~FakeWebAppRegistryController() override;

  void SetUp(base::raw_ptr<Profile> profile);

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

  using UninstallWithoutRegistryUpdateFromSyncDelegate =
      base::RepeatingCallback<void(const std::vector<AppId>& web_apps,
                                   RepeatingUninstallCallback callback)>;
  void SetUninstallWithoutRegistryUpdateFromSyncDelegate(
      UninstallWithoutRegistryUpdateFromSyncDelegate delegate);

  using RetryIncompleteUninstallsDelegate = base::RepeatingCallback<void(
      const std::vector<AppId>& apps_to_uninstall)>;
  void SetRetryIncompleteUninstallsDelegate(
      RetryIncompleteUninstallsDelegate delegate);

  // SyncInstallDelegate:
  void InstallWebAppsAfterSync(std::vector<WebApp*> web_apps,
                               RepeatingInstallCallback callback) override;
  void UninstallWithoutRegistryUpdateFromSync(
      const std::vector<AppId>& web_apps,
      RepeatingUninstallCallback callback) override;
  void RetryIncompleteUninstalls(
      const std::vector<AppId>& apps_to_uninstall) override;

  void DestroySubsystems();

  FakeWebAppDatabaseFactory& database_factory() { return *database_factory_; }
  WebAppRegistrar& registrar() { return *mutable_registrar_; }
  WebAppRegistrarMutable& mutable_registrar() { return *mutable_registrar_; }
  syncer::MockModelTypeChangeProcessor& processor() { return mock_processor_; }
  WebAppSyncBridge& sync_bridge() { return *sync_bridge_; }
  WebAppTranslationManager& translation_manager() {
    return *translation_manager_;
  }
  FakeOsIntegrationManager& os_integration_manager() {
    return *os_integration_manager_;
  }
  WebAppPolicyManager& policy_manager() { return *policy_manager_; }

 private:
  InstallWebAppsAfterSyncDelegate install_web_apps_after_sync_delegate_;
  UninstallWithoutRegistryUpdateFromSyncDelegate
      uninstall_from_sync_before_registry_update_delegate_;
  RetryIncompleteUninstallsDelegate retry_incomplete_uninstalls_delegate_;

  std::unique_ptr<FakeWebAppDatabaseFactory> database_factory_;
  std::unique_ptr<WebAppRegistrarMutable> mutable_registrar_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<WebAppSyncBridge> sync_bridge_;
  std::unique_ptr<FakeOsIntegrationManager> os_integration_manager_;
  std::unique_ptr<WebAppTranslationManager> translation_manager_;
  std::unique_ptr<WebAppPolicyManager> policy_manager_;
  std::unique_ptr<FakeExternallyManagedAppManager>
      fake_externally_managed_app_manager_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_REGISTRY_CONTROLLER_H_
