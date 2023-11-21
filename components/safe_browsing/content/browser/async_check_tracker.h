// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_ASYNC_CHECK_TRACKER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_ASYNC_CHECK_TRACKER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace safe_browsing {

class BaseUIManager;

// AsyncCheckTracker is responsible for:
// * Manage the lifetime of any `UrlCheckerOnSB` that is not able to
// complete before BrowserUrlLoaderThrottle::WillProcessResponse is called.
// * Trigger a warning based on the result from `UrlCheckerOnSB` if the
// check is completed between BrowserUrlLoaderThrottle::WillProcessResponse and
// WebContentsObserver::DidFinishNavigation. If the check is completed before
// WillProcessResponse, SafeBrowsingNavigationThrottle will trigger the warning.
// If the check is completed after DidFinishNavigation,
// BaseUIManager::DisplayBlockingPage will trigger the warning.
// * Track and provide the status of async `UrlCheckerOnSB`.
// This class should only be called on the UI thread.
// TODO(crbug.com/1501194): Implement this class.
class AsyncCheckTracker
    : public content::WebContentsUserData<AsyncCheckTracker> {
 public:
  static AsyncCheckTracker* GetOrCreateForWebContents(
      content::WebContents* web_contents,
      scoped_refptr<BaseUIManager> ui_manager);

  AsyncCheckTracker(const AsyncCheckTracker&) = delete;
  AsyncCheckTracker& operator=(const AsyncCheckTracker&) = delete;

  ~AsyncCheckTracker() override;

  base::WeakPtr<AsyncCheckTracker> GetWeakPtr();

 private:
  friend class content::WebContentsUserData<AsyncCheckTracker>;

  AsyncCheckTracker(content::WebContents* web_contents,
                    scoped_refptr<BaseUIManager> ui_manager);

  // Used to display a warning.
  scoped_refptr<BaseUIManager> ui_manager_;

  base::WeakPtrFactory<AsyncCheckTracker> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_ASYNC_CHECK_TRACKER_H_
