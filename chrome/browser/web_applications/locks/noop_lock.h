// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_NOOP_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_NOOP_LOCK_H_

#include "chrome/browser/web_applications/locks/lock.h"

namespace content {
struct PartitionedLockHolder;
}

namespace web_app {

// This lock essentially doesn't lock anything in the system. However, if a
// `FullSystemLock` is used, then that will block the acquisition of this lock.
//
// Locks can be acquired by using the `WebAppLockManager`. The lock is acquired
// when the callback given to the WebAppLockManager is called. Destruction of
// this class will release the lock or cancel the lock request if it is not
// acquired yet.
class NoopLockDescription : public LockDescription {
 public:
  NoopLockDescription();
  ~NoopLockDescription();
};

class NoopLock : public Lock {
 public:
  using LockDescription = NoopLockDescription;

  explicit NoopLock(std::unique_ptr<content::PartitionedLockHolder> holder)
      : Lock(std::move(holder)) {}
  ~NoopLock() = default;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_NOOP_LOCK_H_
