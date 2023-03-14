// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WEB_APP_LOCK_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WEB_APP_LOCK_MANAGER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/types/pass_key.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

namespace base {
class Value;
}

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;
class NoopLock;
class SharedWebContentsLock;
class SharedWebContentsWithAppLock;
class SharedWebContentsWithAppLockDescription;
class WebAppProvider;

// This class handles acquiring and upgrading locks in the WebAppProvider
// system.
class WebAppLockManager {
 public:
  using PassKey = base::PassKey<WebAppLockManager>;

  explicit WebAppLockManager(WebAppProvider& provider);
  ~WebAppLockManager();

  // Returns if the lock for the shared web contents is free.  This means no one
  // is using the shared web contents.
  bool IsSharedWebContentsLockFree();

  // Acquires the lock for the given `lock_description`, calling
  // `on_lock_acquired` when complete with the given lock. The lock
  // is considered released when the `lock` given to the callback is destroyed.
  template <class LockType>
  void AcquireLock(
      const LockDescription& lock_description,
      base::OnceCallback<void(std::unique_ptr<LockType> lock)> on_lock_acquired,
      const base::Location& location);

  // Upgrades the given lock to a new one, and will call `on_lock_acquired` on
  // when the new lock has been acquired.
  std::unique_ptr<SharedWebContentsWithAppLockDescription>
  UpgradeAndAcquireLock(
      std::unique_ptr<SharedWebContentsLock> lock,
      const base::flat_set<AppId>& app_ids,
      base::OnceCallback<void(std::unique_ptr<SharedWebContentsWithAppLock>)>
          on_lock_acquired,
      const base::Location& location = FROM_HERE);

  // Upgrades the given lock to a new one, and will call `on_lock_acquired` on
  // when the new lock has been acquired.
  std::unique_ptr<AppLockDescription> UpgradeAndAcquireLock(
      std::unique_ptr<NoopLock> lock,
      const base::flat_set<AppId>& app_ids,
      base::OnceCallback<void(std::unique_ptr<AppLock>)> on_lock_acquired,
      const base::Location& location = FROM_HERE);

  base::Value ToDebugValue() const;

 private:
  // Acquires the lock for the given `lock`, calling `on_lock_acquired` when
  // complete.
  void AcquireLock(base::WeakPtr<content::PartitionedLockHolder> holder,
                   const LockDescription& lock,
                   base::OnceClosure on_lock_acquired,
                   const base::Location& location);

  content::PartitionedLockManager lock_manager_;
  raw_ref<WebAppProvider> provider_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WEB_APP_LOCK_MANAGER_H_
