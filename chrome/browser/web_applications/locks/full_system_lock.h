// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_FULL_SYSTEM_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_FULL_SYSTEM_LOCK_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"

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

// This locks the whole system. No other locks can be held when this lock is
// acquired.
//
// Locks can be acquired by using the `WebAppLockManager`.
class FullSystemLockDescription : public LockDescription {
 public:
  FullSystemLockDescription();
  ~FullSystemLockDescription();
};

// Holding this lock means that no other lock-compatible operations are
// operating on the system.
//
// See `WebAppLockManager` for how to use locks. Destruction of this class will
// release the lock or cancel the lock request if it is not acquired yet.
//
// Note: Accessing a lock will CHECK-fail if the WebAppProvider system has
// shutdown (or the profile has shut down).
class FullSystemLock : public Lock, public WithAppResources {
 public:
  using LockDescription = FullSystemLockDescription;

  ~FullSystemLock();

  base::WeakPtr<FullSystemLock> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class WebAppLockManager;
  FullSystemLock(base::WeakPtr<WebAppLockManager> lock_manager,
                 std::unique_ptr<content::PartitionedLockHolder> holder,
                 WebAppRegistrar& registrar,
                 WebAppSyncBridge& sync_bridge,
                 WebAppInstallFinalizer& install_finalizer,
                 OsIntegrationManager& os_integration_manager,
                 WebAppInstallManager& install_manager,
                 WebAppIconManager& icon_manager,
                 WebAppTranslationManager& translation_manager,
                 WebAppUiManager& ui_manager);

  base::WeakPtrFactory<FullSystemLock> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_FULL_SYSTEM_LOCK_H_
