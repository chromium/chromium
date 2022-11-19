// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/full_system_lock.h"

#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

FullSystemLockDescription::FullSystemLockDescription()
    : LockDescription({}, LockDescription::Type::kFullSystem) {}
FullSystemLockDescription::~FullSystemLockDescription() = default;

FullSystemLock::FullSystemLock(
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
      WithAppResources(registrar,
                       sync_bridge,
                       install_finalizer,
                       os_integration_manager,
                       install_manager,
                       icon_manager,
                       translation_manager,
                       ui_manager) {}
FullSystemLock::~FullSystemLock() = default;
}  // namespace web_app
