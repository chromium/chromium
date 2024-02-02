// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_ASYNC_CHECK_TRACKER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_ASYNC_CHECK_TRACKER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/content/browser/url_checker_on_sb.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
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
// * Track and provide the status of navigation that is associated with
// UnsafeResource. This class should only be called on the UI thread.
class AsyncCheckTracker
    : public content::WebContentsUserData<AsyncCheckTracker>,
      public content::WebContentsObserver {
 public:
  static AsyncCheckTracker* GetOrCreateForWebContents(
      content::WebContents* web_contents,
      scoped_refptr<BaseUIManager> ui_manager);

  // Returns true if the main frame load is pending (i.e. the navigation has not
  // yet committed). Note that a main frame hit may not be pending, eg. 1)
  // client side detection happens after the load is committed, or 2) async Safe
  // Browsing check is enabled.
  static bool IsMainPageLoadPending(
      const security_interstitials::UnsafeResource& resource);

  AsyncCheckTracker(const AsyncCheckTracker&) = delete;
  AsyncCheckTracker& operator=(const AsyncCheckTracker&) = delete;

  ~AsyncCheckTracker() override;

  // Takes ownership of `checker`.
  void TransferUrlChecker(std::unique_ptr<UrlCheckerOnSB> checker);

  // Called by `UrlCheckerOnSB` or `BrowserURLLoaderThrottle`, when the check
  // completes.
  void PendingCheckerCompleted(int64_t navigation_id,
                               UrlCheckerOnSB::OnCompleteCheckResult result);

  // Returns whether navigation is pending.
  bool IsNavigationPending(int64_t navigation_id);

  // content::WebContentsObserver methods:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  size_t PendingCheckersSizeForTesting();

  base::WeakPtr<AsyncCheckTracker> GetWeakPtr();

 private:
  friend class content::WebContentsUserData<AsyncCheckTracker>;
  friend class SBBrowserUrlLoaderThrottleTestBase;
  friend class AsyncCheckTrackerTest;

  AsyncCheckTracker(content::WebContents* web_contents,
                    scoped_refptr<BaseUIManager> ui_manager);

  // Deletes the pending checker in `pending_checkers_` that is keyed by
  // `navigation_id`. Does nothing if `navigation_id` is not found.
  void MaybeDeleteChecker(int64_t navigation_id);

  // Deletes all pending checkers in `pending_checkers_` except the checker that
  // is keyed by `excluded_navigation_id`.
  void DeletePendingCheckers(absl::optional<int64_t> excluded_navigation_id);

  // Displays an interstitial if there is unsafe resource associated with
  // `redirect_chain` and `navigation_id`.
  void MaybeDisplayBlockingPage(const std::vector<GURL>& redirect_chain,
                                int64_t navigation_id);

  // Displays an interstitial on `resource`.
  void DisplayBlockingPage(security_interstitials::UnsafeResource resource);

  // Used to display a warning.
  scoped_refptr<BaseUIManager> ui_manager_;

  // Pending Safe Browsing checkers on the current page, keyed by the
  // navigation_id.
  base::flat_map<int64_t, std::unique_ptr<UrlCheckerOnSB>> pending_checkers_;

  // Set to true if interstitial should be shown after DidFinishNavigation is
  // called. Reset to false after interstitial is triggered.
  bool show_interstitial_after_finish_navigation_ = false;

  // A set of navigation ids that have committed.
  base::flat_set<int64_t> committed_navigation_ids_;

  base::WeakPtrFactory<AsyncCheckTracker> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_ASYNC_CHECK_TRACKER_H_
