// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/app_lock.h"

#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

AppLockDescription::AppLockDescription(const AppId& app_id)
    : LockDescription({app_id}, LockDescription::Type::kApp) {}
AppLockDescription::AppLockDescription(base::flat_set<AppId> app_ids)
    : LockDescription(std::move(app_ids), LockDescription::Type::kApp) {}
AppLockDescription::~AppLockDescription() = default;

WithAppResources::~WithAppResources() = default;

WebAppRegistrar& WithAppResources::registrar() {
  CHECK(lock_manager_);
  return *registrar_;
}
WebAppSyncBridge& WithAppResources::sync_bridge() {
  CHECK(lock_manager_);
  return *sync_bridge_;
}
WebAppInstallFinalizer& WithAppResources::install_finalizer() {
  CHECK(lock_manager_);
  return *install_finalizer_;
}
OsIntegrationManager& WithAppResources::os_integration_manager() {
  CHECK(lock_manager_);
  return *os_integration_manager_;
}
WebAppInstallManager& WithAppResources::install_manager() {
  CHECK(lock_manager_);
  return *install_manager_;
}
WebAppIconManager& WithAppResources::icon_manager() {
  CHECK(lock_manager_);
  return *icon_manager_;
}
WebAppTranslationManager& WithAppResources::translation_manager() {
  CHECK(lock_manager_);
  return *translation_manager_;
}
WebAppUiManager& WithAppResources::ui_manager() {
  CHECK(lock_manager_);
  return *ui_manager_;
}

WithAppResources::WithAppResources(
    base::WeakPtr<WebAppLockManager> lock_manager,
    WebAppRegistrar& registrar,
    WebAppSyncBridge& sync_bridge,
    WebAppInstallFinalizer& install_finalizer,
    OsIntegrationManager& os_integration_manager,
    WebAppInstallManager& install_manager,
    WebAppIconManager& icon_manager,
    WebAppTranslationManager& translation_manager,
    WebAppUiManager& ui_manager)
    : lock_manager_(std::move(lock_manager)),
      registrar_(registrar),
      sync_bridge_(sync_bridge),
      install_finalizer_(install_finalizer),
      os_integration_manager_(os_integration_manager),
      install_manager_(install_manager),
      icon_manager_(icon_manager),
      translation_manager_(translation_manager),
      ui_manager_(ui_manager) {}

AppLock::AppLock(base::WeakPtr<WebAppLockManager> lock_manager,
                 std::unique_ptr<content::PartitionedLockHolder> holder,
                 WebAppRegistrar& registrar,
                 WebAppSyncBridge& sync_bridge,
                 WebAppInstallFinalizer& install_finalizer,
                 OsIntegrationManager& os_integration_manager,
                 WebAppInstallManager& install_manager,
                 WebAppIconManager& icon_manager,
                 WebAppTranslationManager& translation_manager,
                 WebAppUiManager& ui_manager)
    : Lock(std::move(holder)),
      WithAppResources(std::move(lock_manager),
                       registrar,
                       sync_bridge,
                       install_finalizer,
                       os_integration_manager,
                       install_manager,
                       icon_manager,
                       translation_manager,
                       ui_manager) {}
AppLock::~AppLock() = default;

}  // namespace web_app
