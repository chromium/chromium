// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_MECHANISMS_TAB_LOADING_FRAME_NAVIGATION_SCHEDULER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_MECHANISMS_TAB_LOADING_FRAME_NAVIGATION_SCHEDULER_H_

#include "base/callback.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
}  // namespace content

namespace performance_manager {
namespace mechanisms {

// The mechanism half of the TabLoadingFrameNavigation system. The mechanism is
// responsible for applying NavigationThrottles to frames in accordance with
// policy decisions, and for releasing these throttles when the policy decides
// the navigation should continue unimpeded. This object lives on the UI thread,
// as WebContentsUserData. They are created for a WebContents if that contents
// needs to be throttled, and destroyed when the throttling is complete, or when
// the WebContents is destroyed, whichever comes first.
//
// Policy decisions about whether to apply scheduling to a WebContents or to
// apply a NavigationThrottle to a particular frame need to be made
// synchronously on the UI thread. As such, the mechanism pulls those
// decisions from the policy object.
//
// This entire class lives on the UI thread and can only be accessed from there.
class TabLoadingFrameNavigationScheduler
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TabLoadingFrameNavigationScheduler> {
 public:
  class PolicyDelegate;

  using ResumeCallback =
      base::RepeatingCallback<void(content::NavigationThrottle*)>;

  ~TabLoadingFrameNavigationScheduler() override;

  // Invoked by the embedder hooks. Depending on policy decisions this may end
  // up creating an instance of a scheduler for the associated WebContents, and
  // may additionally create a NavigationThrottle for the provided |handle|.
  static std::unique_ptr<content::NavigationThrottle>
  MaybeCreateThrottleForNavigation(content::NavigationHandle* handle);

  // Notifies the mechanism when it is enabled/disabled. The mechanisms starts
  // in a disabled state, and only starts throttling when explicitly enabled.
  // When subsequently disabled all outstanding throttles are released. Can be
  // toggled multiple times, but this should only really happen in tests.
  // The mechanism adds itself to the list of PM UI-thread mechanisms when it is
  // enabled, and removes itself when disabled.
  static void SetThrottlingEnabled(bool enabled);

  // Stops throttling the given |contents|, only if it is currently treating the
  // specified |last_navigation_id|. Invoked by the policy engine.
  static void StopThrottling(content::WebContents* contents,
                             int64_t last_navigation_id);

  // Testing seams.
  static TabLoadingFrameNavigationScheduler* GetRootForTesting();
  static void SetPolicyDelegateForTesting(PolicyDelegate* policy_delegate);
  static bool IsThrottlingEnabledForTesting();
  static bool IsMechanismRegisteredForTesting();
  void StopThrottlingForTesting() { StopThrottlingImpl(); }
  size_t GetThrottleCountForTesting() const { return throttles_.size(); }
  int64_t GetNavigationIdForTesting() const { return navigation_id_; }
  void SetResumeCallbackForTesting(ResumeCallback resume_callback) {
    resume_callback_ = resume_callback;
  }

 private:
  friend class content::WebContentsUserData<TabLoadingFrameNavigationScheduler>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  class Throttle;
  using ThrottleMap = base::flat_map<content::NavigationHandle*, Throttle*>;

  explicit TabLoadingFrameNavigationScheduler(content::WebContents* contents);

  // WebContentsObserver implementation:
  // This is used to know that a navigation is about to be destroyed.
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  // Invoked by the policy object to indicate that throttling should stop for
  // the given contents. This causes the object to delete itself, so care must
  // be taken in using this.
  void StopThrottlingImpl();

  // Called by instances of Throttle as they tear down. Allows this class to
  // maintain the list of throttles it has issued, and observe them as they are
  // destroyed.
  void NotifyThrottleDestroyed(Throttle* throttle);

  // Called by both "NotifyThrottleDestroyed" and "DidFinishNavigation".
  void RemoveThrottleForHandle(content::NavigationHandle* handle);

  // The navigation ID that this scheduler applies to. Set immediately after
  // object creation.
  int64_t navigation_id_ = 0;

  // The set of Throttles that have been created by this object, and the
  // navigation handles to which they are associated.
  ThrottleMap throttles_;

  // Used as a testing seam.
  ResumeCallback resume_callback_;

  // Linked list mechanism for the collection of all mechanism instances. This
  // is used to implement StopThrottlingEverything.
  TabLoadingFrameNavigationScheduler* prev_ = nullptr;
  TabLoadingFrameNavigationScheduler* next_ = nullptr;
};

// The policy delegate that the scheduler uses to make policy decisions. By
// default the scheduler will use TabLoadingFrameNavigationPolicy as a delegate,
// but this can be redirected as a testing seam.
class TabLoadingFrameNavigationScheduler::PolicyDelegate {
 public:
  PolicyDelegate() = default;
  PolicyDelegate(const PolicyDelegate&) = delete;
  PolicyDelegate& operator=(const PolicyDelegate&) = delete;
  virtual ~PolicyDelegate() = default;

  // See TabLoadingFramNavigationPolicy for full descriptions.
  virtual bool ShouldThrottleWebContents(content::WebContents* contents) = 0;
  virtual bool ShouldThrottleNavigation(content::NavigationHandle* handle) = 0;
};

}  // namespace mechanisms
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_MECHANISMS_TAB_LOADING_FRAME_NAVIGATION_SCHEDULER_H_