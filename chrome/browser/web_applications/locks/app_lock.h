// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_APP_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_APP_LOCK_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace content {
struct PartitionedLockHolder;
}

namespace web_app {

class OsIntegrationManager;
class WebAppIconManager;
class WebAppInstallFinalizer;
class WebAppInstallManager;
class WebAppRegistrar;
class WebAppSyncBridge;
class WebAppTranslationManager;
class WebAppUiManager;

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

// This gives access to web app components that allow read/write access to web
// apps. A lock class that needs read/read access to web apps can inherit from
// this class.
// Note: a future improvement could be to only give read/write access to a list
// of specific web apps.
class WithAppResources {
 public:
  WithAppResources(WebAppRegistrar& registrar,
                   WebAppSyncBridge& sync_bridge,
                   WebAppInstallFinalizer& install_finalizer,
                   OsIntegrationManager& os_integration_manager,
                   WebAppInstallManager& install_manager,
                   WebAppIconManager& icon_manager,
                   WebAppTranslationManager& translation_manager,
                   WebAppUiManager& ui_manager);
  ~WithAppResources();

  WebAppRegistrar& registrar() { return *registrar_; }
  WebAppSyncBridge& sync_bridge() { return *sync_bridge_; }
  WebAppInstallFinalizer& install_finalizer() { return *install_finalizer_; }
  OsIntegrationManager& os_integration_manager() {
    return *os_integration_manager_;
  }
  WebAppInstallManager& install_manager() { return *install_manager_; }
  WebAppIconManager& icon_manager() { return *icon_manager_; }
  WebAppTranslationManager& translation_manager() {
    return *translation_manager_;
  }
  WebAppUiManager& ui_manager() { return *ui_manager_; }

 private:
  raw_ref<WebAppRegistrar> registrar_;
  raw_ref<WebAppSyncBridge> sync_bridge_;
  raw_ref<WebAppInstallFinalizer> install_finalizer_;
  raw_ref<OsIntegrationManager> os_integration_manager_;
  raw_ref<WebAppInstallManager> install_manager_;
  raw_ref<WebAppIconManager> icon_manager_;
  raw_ref<WebAppTranslationManager> translation_manager_;
  raw_ref<WebAppUiManager> ui_manager_;
};

class AppLock : public Lock, public WithAppResources {
 public:
  using LockDescription = AppLockDescription;

  AppLock(std::unique_ptr<content::PartitionedLockHolder> holder,
          WebAppRegistrar& registrar,
          WebAppSyncBridge& sync_bridge,
          WebAppInstallFinalizer& install_finalizer,
          OsIntegrationManager& os_integration_manager,
          WebAppInstallManager& install_manager,
          WebAppIconManager& icon_manager,
          WebAppTranslationManager& translation_manager,
          WebAppUiManager& ui_manager);
  ~AppLock();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_APP_LOCK_H_
