// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_FULL_SYSTEM_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_FULL_SYSTEM_LOCK_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"

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
// Locks can be acquired by using the `WebAppLockManager`. The lock is acquired
// when the callback given to the WebAppLockManager is called. Destruction of
// this class will release the lock or cancel the lock request if it is not
// acquired yet.
class FullSystemLockDescription : public LockDescription {
 public:
  FullSystemLockDescription();
  ~FullSystemLockDescription();
};

class FullSystemLock : public Lock, public WithAppResources {
 public:
  using LockDescription = FullSystemLockDescription;

  FullSystemLock(std::unique_ptr<content::PartitionedLockHolder> holder,
                 WebAppRegistrar& registrar,
                 WebAppSyncBridge& sync_bridge,
                 WebAppInstallFinalizer& install_finalizer,
                 OsIntegrationManager& os_integration_manager,
                 WebAppInstallManager& install_manager,
                 WebAppIconManager& icon_manager,
                 WebAppTranslationManager& translation_manager,
                 WebAppUiManager& ui_manager);
  ~FullSystemLock();

  base::WeakPtr<FullSystemLock> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FullSystemLock> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_FULL_SYSTEM_LOCK_H_
