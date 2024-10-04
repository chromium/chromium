// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WITH_SHARED_WEB_CONTENTS_RESOURCES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WITH_SHARED_WEB_CONTENTS_RESOURCES_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class WebAppLockManager;

// This gives access to a `content::WebContents` instance that's managed by
// `WebAppCommandManager`. A lock class that needs access to
// `content::WebContents` can inherit from this class.
//
// Note: Accessing resources before the lock is granted or after the
// WebAppProvider system has shutdown (or the profile has shut down) will
// CHECK-fail.
class WithSharedWebContentsResources {
 public:
  ~WithSharedWebContentsResources();

  // Will CHECK-fail if accessed before the lock is granted.
  content::WebContents& shared_web_contents() const;

 protected:
  WithSharedWebContentsResources();

  void GrantWithSharedWebContentsResources(
      WebAppLockManager& lock_manager,
      content::WebContents& shared_web_contents);

 private:
  base::WeakPtr<WebAppLockManager> lock_manager_;
  base::WeakPtr<content::WebContents> shared_web_contents_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WITH_SHARED_WEB_CONTENTS_RESOURCES_H_
