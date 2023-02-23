// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_LOCK_H_

#include <iosfwd>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

namespace web_app {

// Represents a lock in the WebAppProvider system. Locks can be acquired by
// creating one of the subclasses of this class, and using the
// `WebAppLockManager` to acquire the lock. The lock is acquired when the
// callback given to the WebAppLockManager is called. Destruction of this class
// will release the lock or cancel the lock request if it is not acquired yet.
class LockDescription {
 public:
  enum class Type {
    kNoOp,
    kBackgroundWebContents,
    kApp,
    kAppAndWebContents,
    kFullSystem,
  };

  LockDescription(LockDescription&&);
  LockDescription& operator=(LockDescription&&);

  LockDescription(const LockDescription&) = delete;
  LockDescription& operator=(const LockDescription&) = delete;

  ~LockDescription();

  Type type() const { return type_; }

  const base::flat_set<AppId>& app_ids() const { return app_ids_; }

  // Shortcut methods looking at the `type()`. Returns if this lock includes an
  // exclusive lock on the shared web contents.
  bool IncludesSharedWebContents() const;

  base::Value AsDebugValue() const;

 protected:
  explicit LockDescription(base::flat_set<AppId> app_ids, Type type);

 private:
  enum class LockLevel {
    kStatic = 0,
    kApp = 1,
    kMaxValue = kApp,
  };

  const base::flat_set<AppId> app_ids_{};
  const Type type_;

  base::WeakPtrFactory<LockDescription> weak_factory_{this};
};

std::ostream& operator<<(std::ostream& os,
                         const LockDescription& lock_description);

class Lock {
 public:
  explicit Lock(std::unique_ptr<content::PartitionedLockHolder> holder);

  ~Lock();

 private:
  friend class WebAppLockManager;
  std::unique_ptr<content::PartitionedLockHolder> holder_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_LOCK_H_
