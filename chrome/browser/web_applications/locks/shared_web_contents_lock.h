// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_LOCK_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/with_shared_web_contents_resources.h"

namespace content {
class WebContents;
struct PartitionedLockHolder;
}  // namespace content

namespace web_app {

class WebAppLockManager;

// This locks the background shared web contents that is used by the
// WebAppProvider system to do operations in the background that require a web
// contents, like install web apps and fetch data.
//
// Locks can be acquired by using the `WebAppLockManager`.
class SharedWebContentsLockDescription : public LockDescription {
 public:
  SharedWebContentsLockDescription();
  ~SharedWebContentsLockDescription();
};

// Holding this locks means you have exclusive access to a background web
// contents that is shared by the WebAppProvider system.
//
// See `WebAppLockManager` for how to use locks. Destruction of this class will
// release the lock or cancel the lock request if it is not acquired yet.
//
// Note: Accessing a lock will CHECK-fail if the WebAppProvider system has
// shutdown (or the profile has shut down).
class SharedWebContentsLock : public Lock,
                              public WithSharedWebContentsResources {
 public:
  using LockDescription = SharedWebContentsLockDescription;

  ~SharedWebContentsLock();

  base::WeakPtr<SharedWebContentsLock> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class WebAppLockManager;
  SharedWebContentsLock(base::WeakPtr<WebAppLockManager> lock_manager,
                        std::unique_ptr<content::PartitionedLockHolder> holder,
                        content::WebContents& shared_web_contents);

  base::WeakPtrFactory<SharedWebContentsLock> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_LOCK_H_
