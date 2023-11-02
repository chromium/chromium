// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_FULL_SYSTEM_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_FULL_SYSTEM_LOCK_H_

#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

// This locks the whole system. No other locks can be held when this lock is
// acquired.
//
// Locks can be acquired by using the `WebAppLockManager`. The lock is acquired
// when the callback given to the WebAppLockManager is called. Destruction of
// this class will release the lock or cancel the lock request if it is not
// acquired yet.
class FullSystemLock : public Lock {
 public:
  FullSystemLock();
  ~FullSystemLock();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_FULL_SYSTEM_LOCK_H_
