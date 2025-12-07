// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_NOOP_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_NOOP_LOCK_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

class WebAppLockManager;

// This lock doesn't lock any resources in the system. It can be used as a
// placeholder when a command needs to acquire a lock but doesn't know which
// resources it will need yet. The lock can be upgraded later.
//
// Locks can be acquired by using the `WebAppLockManager`.
class NoopLockDescription : public LockDescription {
 public:
  NoopLockDescription();
  ~NoopLockDescription();
  NoopLockDescription(NoopLockDescription&&);
};

// Holding a NoopLock is required when a locked operation needs to be executed,
// but there is no explicit resource to be locked yet. If a resource shows up in
// the middle of the command, an upgrade from a NoopLock to an AppLock or
// SharedWebContentsWithAppLock may occur.
//
// See `WebAppLockManager` for how to use locks. Destruction of this class will
// release the lock or cancel the lock request if it is not acquired yet.
//
// Note: Accessing a lock before it is granted or after the WebAppProvider
// system has shutdown (or the profile has shut down) will CHECK-fail.
class NoopLock : public Lock {
 public:
  using LockDescription = NoopLockDescription;

  NoopLock();
  ~NoopLock() override;

  base::WeakPtr<NoopLock> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  friend WebAppLockManager;
  void GrantLock(WebAppLockManager& lock_manager);

  base::WeakPtrFactory<NoopLock> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_NOOP_LOCK_H_
