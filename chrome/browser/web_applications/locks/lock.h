// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_LOCK_H_

#include <iosfwd>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class VisitedManifestManager;
class WebAppLockManager;
class WebContentsManager;
struct PartitionedLockHolder;

// Represents a lock in the WebAppProvider system. Locks can be acquired by
// creating one of the subclasses of this class, and using the
// `WebAppLockManager` to acquire the lock.
class LockDescription {
 public:
  enum class Type {
    kNoOp,
    kBackgroundWebContents,
    kApp,
    kAppAndWebContents,
    kAllAppsLock,
  };

  LockDescription(LockDescription&&);

  LockDescription(const LockDescription&) = delete;
  LockDescription& operator=(const LockDescription&) = delete;

  ~LockDescription();

  Type type() const { return type_; }

  const base::flat_set<webapps::AppId>& app_ids() const { return app_ids_; }

  // Shortcut methods looking at the `type()`. Returns if this lock includes an
  // exclusive lock on the shared web contents.
  bool IncludesSharedWebContents() const;

  base::Value AsDebugValue() const;

 protected:
  explicit LockDescription(base::flat_set<webapps::AppId> app_ids, Type type);

 private:
  enum class LockLevel {
    kStatic = 0,
    kApp = 1,
    kMaxValue = kApp,
  };

  const base::flat_set<webapps::AppId> app_ids_;
  const Type type_;
};

std::ostream& operator<<(std::ostream& os,
                         const LockDescription& lock_description);

// See `WebAppLockManager` for how to use locks. Destruction of this class will
// release the lock or cancel the lock request if it is not acquired yet.
//
// Note: Accessing a lock before it is granted or after the WebAppProvider
// system has shutdown (or the profile has shut down) will CHECK-fail.
class Lock {
 public:
  ~Lock();

  // Returns if the lock is granted. If this returns `false`, then all accessors
  // on this method will CHECK-fail.
  bool IsGranted() const;

  // Resources that are available on all locks:
  // This will CHECK-fail if `IsGranted()` returns false.
  WebContentsManager& web_contents_manager();
  // This will CHECK-fail if `IsGranted()` returns false.
  VisitedManifestManager& visited_manifest_manager();

  PartitionedLockHolder& GetLockHolder(base::PassKey<WebAppLockManager>) {
    return *holder_;
  }

 protected:
  Lock();
  // Will CHECK-fail if called more than once.
  void GrantLockResources(WebAppLockManager& lock_manager);

 private:
  // TODO(crbug.com/370534630) Store this by-value after header file is split
  // off.
  std::unique_ptr<PartitionedLockHolder> holder_;
  base::WeakPtr<WebAppLockManager> lock_manager_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_LOCK_H_
