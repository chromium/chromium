// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_ASYNC_CHECK_TRACKER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_ASYNC_CHECK_TRACKER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/safe_browsing/content/browser/url_checker_holder.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace safe_browsing {

class BaseUIManager;

// AsyncCheckTracker is responsible for:
// * Manage the lifetime of any `UrlCheckerHolder` that is not able to
// complete before BrowserUrlLoaderThrottle::WillProcessResponse is called.
// * Trigger a warning based on the result from `UrlCheckerHolder` if the
// check is completed between BrowserUrlLoaderThrottle::WillProcessResponse and
// WebContentsObserver::DidFinishNavigation. If the check is completed before
// WillProcessResponse, SafeBrowsingNavigationThrottle will trigger the warning.
// If the check is completed after DidFinishNavigation,
// BaseUIManager::DisplayBlockingPage will trigger the warning.
// * Track and provide the status of navigation that is associated with
// UnsafeResource. Other classes can add themselves as an observer and get
// notified when certain events happen.
// This class should only be called on the UI thread.
class AsyncCheckTracker
    : public content::WebContentsUserData<AsyncCheckTracker>,
      public content::WebContentsObserver {
 public:
  // Interface for observing events on AsyncCheckTracker.
  class Observer : public base::CheckedObserver {
   public:
    // Called when a SB check is completed by AsyncCheckTracker. This is not
    // called if the check was not handled by AsyncCheckTracker (i.e. the check
    // is completed before the response body is processed, which is the majority
    // of cases).
    virtual void OnAsyncSafeBrowsingCheckCompleted() {}
    // Notify the observers to unsubscribe before AsyncCheckTracker is
    // destructed.
    virtual void OnAsyncSafeBrowsingCheckTrackerDestructed() {}
  };

  static AsyncCheckTracker* GetOrCreateForWebContents(
      content::WebContents* web_contents,
      scoped_refptr<BaseUIManager> ui_manager,
      bool should_sync_checker_check_allowlist);

  // Returns true if the main frame load is pending (i.e. the navigation has not
  // yet committed). Note that a main frame hit may not be pending, eg. 1)
  // client side detection happens after the load is committed, or 2) async Safe
  // Browsing check is enabled.
  // Caveat: This class only tracks committed navigation ids for a
  // certain period, so this function may not return the correct result if the
  // navigation associated with the `resource` is too old.
  static bool IsMainPageLoadPending(
      const security_interstitials::UnsafeResource& resource);

  // Returns the timestamp when the navigation associated with `resource` is
  // committed. Returns nullopt if the navigation has not committed.
  // Caveat: This class only tracks committed navigation ids for a
  // certain period, so this function may not return the correct result if the
  // navigation associated with the `resource` is too old.
  static std::optional<base::TimeTicks> GetBlockedPageCommittedTimestamp(
      const security_interstitials::UnsafeResource& resource);

  // Returns whether the platform is eligible for its sync checker to check the
  // allowlist first. Only return true if
  //   * The allowlist check is significantly faster than the local blocklist
  //     check on this platform. AND
  //   * As a risk mitigation, the async checker should still fall back to local
  //     blocklist check if the URL matches the allowlist.
  static bool IsPlatformEligibleForSyncCheckerCheckAllowlist();

  AsyncCheckTracker(const AsyncCheckTracker&) = delete;
  AsyncCheckTracker& operator=(const AsyncCheckTracker&) = delete;

  ~AsyncCheckTracker() override;

  // Takes ownership of `checker`.
  void TransferUrlChecker(std::unique_ptr<UrlCheckerHolder> checker);

  // Called by `UrlCheckerHolder` or `BrowserURLLoaderThrottle`, when the check
  // completes.
  void PendingCheckerCompleted(int64_t navigation_id,
                               UrlCheckerHolder::OnCompleteCheckResult result);

  // Returns whether navigation is pending.
  bool IsNavigationPending(int64_t navigation_id);

  // Returns the time when the navigation is committed. Returns nullopt if the
  // navigation has not yet committed.
  std::optional<base::TimeTicks> GetNavigationCommittedTimestamp(
      int64_t navigation_id);

  // Returns whether the additional sync checker should check the allowlist
  // first.
  // Checking the allowlist first can reduce the loading latency caused by Safe
  // Browsing on certain platforms.
  bool should_sync_checker_check_allowlist() {
    return should_sync_checker_check_allowlist_;
  }

  // content::WebContentsObserver methods:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  size_t PendingCheckersSizeForTesting();

  void SetNavigationTimestampsSizeThresholdForTesting(size_t threshold);

  base::WeakPtr<AsyncCheckTracker> GetWeakPtr();

 private:
  friend class content::WebContentsUserData<AsyncCheckTracker>;
  friend class SBBrowserUrlLoaderThrottleTestBase;
  friend class AsyncCheckTrackerTest;
  friend class SafeBrowsingBlockingPageTestHelper;

  AsyncCheckTracker(content::WebContents* web_contents,
                    scoped_refptr<BaseUIManager> ui_manager,
                    bool should_sync_checker_check_allowlist);

  // Deletes the pending checker in `pending_checkers_` that is keyed by
  // `navigation_id`. Does nothing if `navigation_id` is not found.
  void MaybeDeleteChecker(int64_t navigation_id);

  // Deletes all pending checkers in `pending_checkers_` except the checker that
  // is keyed by `excluded_navigation_id`.
  void DeletePendingCheckers(std::optional<int64_t> excluded_navigation_id);

  // Deletes expired timestamps to avoid `committed_navigation_timestamps_`
  // getting too large.
  void DeleteExpiredNavigationTimestamps();

  // Displays an interstitial if there is unsafe resource associated with
  // `redirect_chain` and `navigation_id`.
  void MaybeDisplayBlockingPage(const std::vector<GURL>& redirect_chain,
                                int64_t navigation_id);

  // Displays an interstitial on `resource`.
  void DisplayBlockingPage(security_interstitials::UnsafeResource resource);

  // Sets callback to be used once all checkers are completed. Used only for
  // tests.
  void SetOnAllCheckersCompletedForTesting(base::OnceClosure callback);

  // May call |on_all_checkers_completed_callback_for_testing_| if there are no
  // |pending_checkers_| remaining.
  void MaybeCallOnAllCheckersCompletedCallback();

  // Used to display a warning.
  scoped_refptr<BaseUIManager> ui_manager_;

  // Pending Safe Browsing checkers on the current page, keyed by the
  // navigation_id.
  base::flat_map<int64_t, std::unique_ptr<UrlCheckerHolder>> pending_checkers_;

  // Set to true if interstitial should be shown after DidFinishNavigation is
  // called. Reset to false after interstitial is triggered.
  bool show_interstitial_after_finish_navigation_ = false;

  // Time when a navigation is committed, keyed by the navigation_id.
  // Whether a navigation id is in the map can be used to determine if a
  // navigation has committed. The time when the navigation has committed is
  // used for logging metrics.
  base::flat_map<int64_t, base::TimeTicks> committed_navigation_timestamps_;

  // The threshold that will trigger a cleanup on
  // `committed_navigation_timestamps_`. Overridden in tests.
  size_t navigation_timestamps_size_threshold_;

  // Whether sync checker should check the allowlist first.
  const bool should_sync_checker_check_allowlist_ = false;

  // A list of observers that are interested in events from this class.
  base::ObserverList<Observer> observers_;

  // Callback that is called once all checkers are completed. Used only for
  // tests.
  base::OnceClosure on_all_checkers_completed_callback_for_testing_;

  base::WeakPtrFactory<AsyncCheckTracker> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_ASYNC_CHECK_TRACKER_H_
