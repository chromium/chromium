// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/app_lock.h"

#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

AppLockDescription::AppLockDescription(base::flat_set<AppId> app_ids)
    : LockDescription(std::move(app_ids), LockDescription::Type::kApp) {}
AppLockDescription::~AppLockDescription() = default;

WithAppResources::WithAppResources(
    WebAppRegistrar& registrar,
    WebAppSyncBridge& sync_bridge,
    WebAppInstallFinalizer& install_finalizer,
    OsIntegrationManager& os_integration_manager,
    WebAppInstallManager& install_manager,
    WebAppIconManager& icon_manager,
    WebAppTranslationManager& translation_manager,
    WebAppUiManager& ui_manager)
    : registrar_(registrar),
      sync_bridge_(sync_bridge),
      install_finalizer_(install_finalizer),
      os_integration_manager_(os_integration_manager),
      install_manager_(install_manager),
      icon_manager_(icon_manager),
      translation_manager_(translation_manager),
      ui_manager_(ui_manager) {}
WithAppResources::~WithAppResources() = default;

AppLock::AppLock(std::unique_ptr<content::PartitionedLockHolder> holder,
                 WebAppRegistrar& registrar,
                 WebAppSyncBridge& sync_bridge,
                 WebAppInstallFinalizer& install_finalizer,
                 OsIntegrationManager& os_integration_manager,
                 WebAppInstallManager& install_manager,
                 WebAppIconManager& icon_manager,
                 WebAppTranslationManager& translation_manager,
                 WebAppUiManager& ui_manager)
    : Lock(std::move(holder)),
      WithAppResources(registrar,
                       sync_bridge,
                       install_finalizer,
                       os_integration_manager,
                       install_manager,
                       icon_manager,
                       translation_manager,
                       ui_manager) {}
AppLock::~AppLock() = default;

}  // namespace web_app
