// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_APP_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_APP_LOCK_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

class OsIntegrationManager;
class WebAppInstallFinalizer;
class WebAppRegistrar;
class WebAppSyncBridge;

// This locks the given app ids in the WebAppProvider system.
//
// Locks can be acquired by using the `WebAppLockManager`. The lock is acquired
// when the callback given to the WebAppLockManager is called. Destruction of
// this class will release the lock or cancel the lock request if it is not
// acquired yet.
class AppLockDescription : public LockDescription {
 public:
  explicit AppLockDescription(base::flat_set<AppId> app_ids);
  ~AppLockDescription();
};

class AppLock {
 public:
  AppLock(WebAppRegistrar& registrar,
          WebAppSyncBridge& sync_bridge,
          WebAppInstallFinalizer& install_finalizer,
          OsIntegrationManager& os_integration_manager);
  ~AppLock();

  WebAppRegistrar& registrar() { return *registrar_; }
  WebAppSyncBridge& sync_bridge() { return *sync_bridge_; }
  WebAppInstallFinalizer& install_finalizer() { return *install_finalizer_; }
  OsIntegrationManager& os_integration_manager() {
    return *os_integration_manager_;
  }

 private:
  raw_ref<WebAppRegistrar> registrar_;
  raw_ref<WebAppSyncBridge> sync_bridge_;
  raw_ref<WebAppInstallFinalizer> install_finalizer_;
  raw_ref<OsIntegrationManager> os_integration_manager_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_APP_LOCK_H_
