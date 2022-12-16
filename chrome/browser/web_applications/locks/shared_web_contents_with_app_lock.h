// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_WITH_APP_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_WITH_APP_LOCK_H_

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace content {
class WebContents;
struct PartitionedLockHolder;
}  // namespace content

namespace web_app {

class OsIntegrationManager;
class WebAppIconManager;
class WebAppInstallFinalizer;
class WebAppInstallManager;
class WebAppRegistrar;
class WebAppSyncBridge;
class WebAppTranslationManager;
class WebAppUiManager;

// This locks both the background shared web contents AND the given app ids. The
// background web contents is used by the WebAppProvider system to do operations
// in the background that require a web contents, like install web apps and
// fetch data.
//
// Locks can be acquired by using the `WebAppLockManager`. The lock is acquired
// when the callback given to the WebAppLockManager is called. Destruction of
// this class will release the lock or cancel the lock request if it is not
// acquired yet.
class SharedWebContentsWithAppLockDescription : public LockDescription {
 public:
  explicit SharedWebContentsWithAppLockDescription(
      base::flat_set<AppId> app_ids);
  ~SharedWebContentsWithAppLockDescription();
};

class SharedWebContentsWithAppLock : public Lock,
                                     public WithSharedWebContentsResources,
                                     public WithAppResources {
 public:
  using LockDescription = SharedWebContentsWithAppLockDescription;

  SharedWebContentsWithAppLock(
      std::unique_ptr<content::PartitionedLockHolder> holder,
      content::WebContents& shared_web_contents,
      WebAppRegistrar& registrar,
      WebAppSyncBridge& sync_bridge,
      WebAppInstallFinalizer& install_finalizer,
      OsIntegrationManager& os_integration_manager,
      WebAppInstallManager& install_manager,
      WebAppIconManager& icon_manager,
      WebAppTranslationManager& translation_manager,
      WebAppUiManager& ui_manager);
  ~SharedWebContentsWithAppLock();

  base::WeakPtr<SharedWebContentsWithAppLock> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<SharedWebContentsWithAppLock> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_WITH_APP_LOCK_H_
