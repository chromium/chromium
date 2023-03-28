// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_APP_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_APP_LOCK_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
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

// This locks the given app ID(s) in the WebAppProvider system.
//
// Locks can be acquired by using the `WebAppLockManager`.
class AppLockDescription : public LockDescription {
 public:
  explicit AppLockDescription(const AppId& app_id);
  explicit AppLockDescription(base::flat_set<AppId> app_ids);
  ~AppLockDescription();
};

// This gives access to web app components that allow read/write access to web
// apps. A lock class that needs read/read access to web apps can inherit from
// this class.
// Note: a future improvement could be to only give read/write access to a list
// of specific web apps.
//
// See `WebAppLockManager` for how to use locks. Destruction of this class will
// release the lock or cancel the lock request if it is not acquired yet.
//
// Note: Accessing a lock will CHECK-fail if the WebAppProvider system has
// shutdown (or the profile has shut down).
class WithAppResources {
 public:
  ~WithAppResources();

  WebAppRegistrar& registrar();
  WebAppSyncBridge& sync_bridge();
  WebAppInstallFinalizer& install_finalizer();
  OsIntegrationManager& os_integration_manager();
  WebAppInstallManager& install_manager();
  WebAppIconManager& icon_manager();
  WebAppTranslationManager& translation_manager();
  WebAppUiManager& ui_manager();

 protected:
  WithAppResources(base::WeakPtr<WebAppLockManager> lock_manager,
                   WebAppRegistrar& registrar,
                   WebAppSyncBridge& sync_bridge,
                   WebAppInstallFinalizer& install_finalizer,
                   OsIntegrationManager& os_integration_manager,
                   WebAppInstallManager& install_manager,
                   WebAppIconManager& icon_manager,
                   WebAppTranslationManager& translation_manager,
                   WebAppUiManager& ui_manager);

 private:
  base::WeakPtr<WebAppLockManager> lock_manager_;
  raw_ref<WebAppRegistrar, DanglingUntriaged> registrar_;
  raw_ref<WebAppSyncBridge, DanglingUntriaged> sync_bridge_;
  raw_ref<WebAppInstallFinalizer, DanglingUntriaged> install_finalizer_;
  raw_ref<OsIntegrationManager, DanglingUntriaged> os_integration_manager_;
  raw_ref<WebAppInstallManager, DanglingUntriaged> install_manager_;
  raw_ref<WebAppIconManager, DanglingUntriaged> icon_manager_;
  raw_ref<WebAppTranslationManager, DanglingUntriaged> translation_manager_;
  raw_ref<WebAppUiManager, DanglingUntriaged> ui_manager_;
};

// Holding this lock means that no other lock-compatible operations are touching
// the same app id/s. This does not ensure that the app/s are installed when the
// lock is granted. Checks for that will need to be handled by the user of
// the lock.
//
// See `WebAppLockManager` for how to use locks. Destruction of this class will
// release the lock or cancel the lock request if it is not acquired yet.
//
// Note: Accessing a lock will CHECK-fail if the WebAppProvider system has
// shutdown (or the profile has shut down).
class AppLock : public Lock, public WithAppResources {
 public:
  using LockDescription = AppLockDescription;

  ~AppLock();

  base::WeakPtr<AppLock> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  friend class WebAppLockManager;
  AppLock(base::WeakPtr<WebAppLockManager> lock_manager,
          std::unique_ptr<content::PartitionedLockHolder> holder,
          WebAppRegistrar& registrar,
          WebAppSyncBridge& sync_bridge,
          WebAppInstallFinalizer& install_finalizer,
          OsIntegrationManager& os_integration_manager,
          WebAppInstallManager& install_manager,
          WebAppIconManager& icon_manager,
          WebAppTranslationManager& translation_manager,
          WebAppUiManager& ui_manager);

  base::WeakPtrFactory<AppLock> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_APP_LOCK_H_
