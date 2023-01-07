// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WEB_APP_LOCK_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WEB_APP_LOCK_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager_impl.h"

namespace web_app {

class AppLock;
class Lock;
class NoopLock;
class SharedWebContentsLock;
class SharedWebContentsWithAppLock;

// This class handles acquiring and upgrading locks in the WebAppProvider
// system.
class WebAppLockManager {
 public:
  WebAppLockManager();
  ~WebAppLockManager();

  // Returns if the lock for the shared web contents is free.  This means no one
  // is using the shared web contents.
  bool IsSharedWebContentsLockFree();

  // Acquires the lock for the given `lock`, calling `on_lock_acquired` when
  // complete. This call will CHECK-fail if the lock has already been used in an
  // `AcquireLock` call. The lock is considered released when the `lock` is
  // destroyed.
  void AcquireLock(Lock& lock, base::OnceClosure on_lock_acquired);

  // Upgrades the given lock to a new one, and will call `on_lock_acquired` on
  // when the new lock has been acquired. This call will CHECK-fail if `lock`
  // was not already used in a call to `AcquireLock`.
  std::unique_ptr<SharedWebContentsWithAppLock> UpgradeAndAcquireLock(
      std::unique_ptr<SharedWebContentsLock> lock,
      const base::flat_set<AppId>& app_ids,
      base::OnceClosure on_lock_acquired);

  std::unique_ptr<AppLock> UpgradeAndAcquireLock(
      std::unique_ptr<NoopLock> lock,
      const base::flat_set<AppId>& app_ids,
      base::OnceClosure on_lock_acquired);

 private:
  content::PartitionedLockManagerImpl lock_manager_{2};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WEB_APP_LOCK_MANAGER_H_
