// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/app_lock.h"

#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

AppLockDescription::AppLockDescription(base::flat_set<AppId> app_ids)
    : LockDescription(std::move(app_ids), LockDescription::Type::kApp) {}
AppLockDescription::~AppLockDescription() = default;

AppLock::AppLock(WebAppRegistrar& registrar,
                 WebAppSyncBridge& sync_bridge,
                 WebAppInstallFinalizer& install_finalizer,
                 OsIntegrationManager& os_integration_manager)
    : registrar_(registrar),
      sync_bridge_(sync_bridge),
      install_finalizer_(install_finalizer),
      os_integration_manager_(os_integration_manager) {}
AppLock::~AppLock() = default;

}  // namespace web_app
