// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_LOCK_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace content {
struct LeveledLockHolder;
}

namespace web_app {

// Represents a lock in the WebAppProvider system. Locks can be acquired by
// creating one of the subclasses of this class, and using the
// `WebAppLockManager` to acquire the lock. The lock is acquired when the
// callback given to the WebAppLockManager is called. Destruction of this class
// will release the lock or cancel the lock request if it is not acquired yet.
class Lock {
 public:
  enum class Type {
    kNoOp,
    kBackgroundWebContents,
    kApp,
    kAppAndWebContents,
    kFullSystem,
  };

  Lock(Lock&&);
  Lock& operator=(Lock&&);

  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;

  ~Lock();

  Type type() const { return type_; }

  const base::flat_set<AppId>& app_ids() const { return app_ids_; }

  // Shortcut methods looking at the `type()`. Returns if this lock includes an
  // exclusive lock on the shared web contents.
  bool IncludesSharedWebContents() const;

  bool HasLockBeenRequested() const { return !!holder_.get(); }

 protected:
  explicit Lock(base::flat_set<AppId> app_ids, Type type);

 private:
  friend class WebAppLockManager;

  enum class LockLevel {
    kStatic = 0,
    kApp = 1,
    kMaxValue = kApp,
  };

  std::unique_ptr<content::LeveledLockHolder> holder_;
  const base::flat_set<AppId> app_ids_{};
  const Type type_;

  base::WeakPtrFactory<Lock> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_LOCK_H_
