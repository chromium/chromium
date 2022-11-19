// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_LOCK_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/locks/lock.h"

namespace content {
class WebContents;
struct PartitionedLockHolder;
}  // namespace content

namespace web_app {

// This locks the background shared web contents that is used by the
// WebAppProvider system to do operations in the background that require a web
// contents, like install web apps and fetch data.
//
// Locks can be acquired by using the `WebAppLockManager`. The lock is acquired
// when the callback given to the WebAppLockManager is called. Destruction of
// this class will release the lock or cancel the lock request if it is not
// acquired yet.
class SharedWebContentsLockDescription : public LockDescription {
 public:
  SharedWebContentsLockDescription();
  ~SharedWebContentsLockDescription();
};

// This gives access to a `content::WebContents` instance that's managed by
// `WebAppCommandManager`. A lock class that needs access to
// `content::WebContents` can inherit from this class.
class WithSharedWebContentsResources {
 public:
  explicit WithSharedWebContentsResources(
      content::WebContents& shared_web_contents);
  ~WithSharedWebContentsResources();

  content::WebContents& shared_web_contents() const {
    return *shared_web_contents_;
  }

 private:
  raw_ref<content::WebContents> shared_web_contents_;
};

class SharedWebContentsLock : public Lock,
                              public WithSharedWebContentsResources {
 public:
  using LockDescription = SharedWebContentsLockDescription;

  explicit SharedWebContentsLock(
      std::unique_ptr<content::PartitionedLockHolder> holder,
      content::WebContents& shared_web_contents);
  ~SharedWebContentsLock();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_SHARED_WEB_CONTENTS_LOCK_H_
