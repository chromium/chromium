// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_ALL_APPS_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_ALL_APPS_LOCK_H_

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class WebAppLockManager;

// This locks all app ids in the WebAppProvider system.
//
// Locks can be acquired by using the `WebAppLockManager`.
class AllAppsLockDescription : public LockDescription {
 public:
  AllAppsLockDescription();
  AllAppsLockDescription(AllAppsLockDescription&&);
  ~AllAppsLockDescription();
};

// Holding this lock means that no other lock-compatible operations are touching
// the same app id/s. This does not ensure that the app/s are installed when the
// lock is granted. Checks for that will need to be handled by the user of
// the lock.
//
// See `WebAppLockManager` for how to use locks. Destruction of this class will
// release the lock or cancel the lock request if it is not acquired yet.
//
// Note: Accessing a lock before it is granted or after the WebAppProvider
// system has shutdown (or the profile has shut down) will CHECK-fail.
class AllAppsLock : public Lock, public WithAppResources {
 public:
  using LockDescription = AllAppsLockDescription;

  AllAppsLock();
  ~AllAppsLock();

  base::WeakPtr<AllAppsLock> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  friend class WebAppLockManager;
  void GrantLock(WebAppLockManager& lock_manager);

  base::WeakPtrFactory<AllAppsLock> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_ALL_APPS_LOCK_H_
