// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_WITH_APP_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_WITH_APP_LOCK_H_

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/locks/with_shared_web_contents_resources.h"
#include "components/webapps/common/web_app_id.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class WebAppLockManager;

// This locks both the background shared web contents AND the given app ids. The
// background web contents is used by the WebAppProvider system to do operations
// in the background that require a web contents, like install web apps and
// fetch data.
//
// Locks can be acquired by using the `WebAppLockManager`.
class SharedWebContentsWithAppLockDescription : public LockDescription {
 public:
  explicit SharedWebContentsWithAppLockDescription(
      base::flat_set<webapps::AppId> app_ids);
  SharedWebContentsWithAppLockDescription(
      SharedWebContentsWithAppLockDescription&&);
  ~SharedWebContentsWithAppLockDescription();
};

// Holding this lock means that the user has exclusive access to the app id/s
// and the background web contents in use by the WebAppProvider system. This
// does not ensure that the app/s are installed when the lock is granted. Checks
// for that will need to be handled by the user of the lock.
// The web contents will be prepared for use via
// WebAppUrlLoader::PrepareForLoad() prior to being granted access.
//
// See `WebAppLockManager` for how to use locks. Destruction of this class will
// release the lock or cancel the lock request if it is not acquired yet.
//
// Note: Accessing a lock before it is granted or after the WebAppProvider
// system has shutdown (or the profile has shut down) will CHECK-fail.
class SharedWebContentsWithAppLock : public Lock,
                                     public WithSharedWebContentsResources,
                                     public WithAppResources {
 public:
  using LockDescription = SharedWebContentsWithAppLockDescription;

  SharedWebContentsWithAppLock();
  ~SharedWebContentsWithAppLock();

  base::WeakPtr<SharedWebContentsWithAppLock> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class WebAppLockManager;
  void GrantLock(WebAppLockManager& lock_manager,
                 content::WebContents& web_contents);

  base::WeakPtrFactory<SharedWebContentsWithAppLock> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_WITH_APP_LOCK_H_
