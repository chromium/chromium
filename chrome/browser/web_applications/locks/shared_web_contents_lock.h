// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_LOCK_H_

#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

// This locks the background shared web contents that is used by the
// WebAppProvider system to do operations in the background that require a web
// contents, like install web apps and fetch data.
//
// Locks can be acquired by using the `WebAppLockManager`. The lock is acquired
// when the callback given to the WebAppLockManager is called. Destruction of
// this class will release the lock or cancel the lock request if it is not
// acquired yet.
class SharedWebContentsLock : public Lock {
 public:
  SharedWebContentsLock();
  ~SharedWebContentsLock();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_LOCK_H_
