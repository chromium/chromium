// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_THROTTLE_H_
#define COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_THROTTLE_H_

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
class NavigationThrottleRegistry;
}

namespace navigation_interception {

BASE_DECLARE_FEATURE(kAsyncCheck);

enum class SynchronyMode {
  // Support async interception in some cases (See ShouldCheckAsynchronously).
  kAsync,
  // Only support synchronous interception.
  kSync
};

// This class allows the provider of the Callback to selectively ignore top
// level navigations. This is a UI thread class.
class InterceptNavigationThrottle : public content::NavigationThrottle {
 public:
  typedef base::OnceCallback<void(bool)> ResultCallback;
  typedef base::RepeatingCallback<void(
      content::NavigationHandle* /* navigation_handle */,
      bool should_run_async,
      ResultCallback)>
      CheckCallback;

  InterceptNavigationThrottle(
      content::NavigationThrottleRegistry& registry,
      CheckCallback should_ignore_callback,
      SynchronyMode async_mode,
      std::optional<base::RepeatingClosure> request_finish_async_work_callback);

  InterceptNavigationThrottle(const InterceptNavigationThrottle&) = delete;
  InterceptNavigationThrottle& operator=(const InterceptNavigationThrottle&) =
      delete;

  ~InterceptNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  friend class InterceptNavigationThrottleTest;
  ThrottleCheckResult CheckIfShouldIgnoreNavigation();
  void OnCheckComplete(bool should_ignore);
  void RequestFinishPendingCheck();

  bool ShouldCheckAsynchronously() const;

  content::NavigationThrottle::ThrottleCheckResult Defer();

  base::WeakPtr<InterceptNavigationThrottle> GetWeakPtrForTesting();

  // This callback should be called at the start of navigation and every
  // redirect, until |should_ignore_| is true.
  // Note: the callback can delete |this|.
  CheckCallback should_ignore_callback_;

  // This callback will be called if a redirect comes in before the previous
  // should_ignore_callback_ completes, and requires that the outstanding
  // should_ignore_callback_ completes before returning.
  std::optional<base::RepeatingClosure> request_finish_async_work_callback_;

  // Note that the CheckCallback currently has thread affinity on the Java side.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  const SynchronyMode mode_ = SynchronyMode::kSync;

  // Whether the navigation should be ignored. Updated at every redirect.
  bool should_ignore_ = false;

  // Whether a should ignore check is in progress.
  bool pending_check_ = false;

  // Whether a navigation is being deferred because of an outstanding should
  // ignore check.
  bool deferring_ = false;

  // True if a redirect is doing the deferring.
  bool deferring_redirect_ = false;

  base::TimeTicks defer_start_;

  // Tracks whether we're in a synchronous intercept navigation check so we can
  // crash if we're deleted during the check and get a stack trace.
  bool in_sync_check_ = false;

  base::WeakPtrFactory<InterceptNavigationThrottle> weak_factory_{this};
};

}  // namespace navigation_interception

#endif  // COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_THROTTLE_H_
