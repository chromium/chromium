// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/intercept_navigation_throttle.h"

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

namespace navigation_interception {

// Note: this feature is a no-op on non-Android platforms.
BASE_FEATURE(kAsyncCheck,
             "AsyncNavigationIntercept",
             base::FEATURE_ENABLED_BY_DEFAULT);

InterceptNavigationThrottle::InterceptNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    CheckCallback should_ignore_callback,
    SynchronyMode async_mode,
    std::optional<base::RepeatingClosure> request_finish_async_work_callback)
    : content::NavigationThrottle(registry),
      should_ignore_callback_(should_ignore_callback),
      request_finish_async_work_callback_(request_finish_async_work_callback),
      ui_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      mode_(async_mode) {
  CHECK(mode_ == SynchronyMode::kSync ||
        request_finish_async_work_callback_.has_value());
}

InterceptNavigationThrottle::~InterceptNavigationThrottle() {
  // Clients should not synchronously cause the navigation to be deleted.
  // Putting the check here allows us to get a stack for what's causing the
  // deletion.
  CHECK(!in_sync_check_);
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillStartRequest() {
  DCHECK(!should_ignore_);
  DCHECK(!navigation_handle()->WasServerRedirect());
  return CheckIfShouldIgnoreNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillRedirectRequest() {
  RequestFinishPendingCheck();
  if (pending_check_) {
    deferring_redirect_ = true;
    return Defer();
  }
  if (should_ignore_) {
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }
  DCHECK(navigation_handle()->WasServerRedirect());
  return CheckIfShouldIgnoreNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillProcessResponse() {
  RequestFinishPendingCheck();
  if (pending_check_) {
    return Defer();
  }
  if (should_ignore_) {
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  return content::NavigationThrottle::PROCEED;
}

const char* InterceptNavigationThrottle::GetNameForLogging() {
  return "InterceptNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::Defer() {
  deferring_ = true;
  defer_start_ = base::TimeTicks::Now();
  return content::NavigationThrottle::DEFER;
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::CheckIfShouldIgnoreNavigation() {
  // If called while deferring, could lead to crashes where the OnCheckComplete
  // callback cancels the deferred navigation and deletes |this| synchronously.
  CHECK(!deferring_);

  bool async = ShouldCheckAsynchronously();
  pending_check_ = true;
  in_sync_check_ = true;
  auto weak_this = weak_factory_.GetWeakPtr();
  should_ignore_callback_.Run(
      navigation_handle(), async,
      base::BindOnce(&InterceptNavigationThrottle::OnCheckComplete, weak_this));
  // Clients should not synchronously cause the navigation to be deleted.
  CHECK(weak_this);
  in_sync_check_ = false;
  if (pending_check_) {
    if (async) {
      return content::NavigationThrottle::PROCEED;
    } else {
      return Defer();
    }
  }
  if (should_ignore_) {
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }
  return content::NavigationThrottle::PROCEED;
}

void InterceptNavigationThrottle::RequestFinishPendingCheck() {
  if (pending_check_) {
    request_finish_async_work_callback_->Run();
  }
}

void InterceptNavigationThrottle::OnCheckComplete(bool should_ignore) {
  should_ignore_ = should_ignore;
  pending_check_ = false;

  if (deferring_redirect_ && !should_ignore_) {
    CHECK(deferring_);
    deferring_redirect_ = false;
    // Not calling CheckIfShouldIgnoreNavigation() for complicated reasons.
    // CancelDeferredNavigation below can delete |this|, and when called from
    // this callsite that's actually fine, so we don't want to crash from the
    // checks to ensure we're not deleted in CheckIfShouldIgnoreNavigation. Also
    // we don't want to call Defer() a second time since we're already
    // deferring.
    should_ignore_callback_.Run(
        navigation_handle(), ShouldCheckAsynchronously(),
        base::BindOnce(&InterceptNavigationThrottle::OnCheckComplete,
                       weak_factory_.GetWeakPtr()));
    // Careful, we may be re-entrant here for synchronous checks.
    return;
  }
  deferring_redirect_ = false;
  if (deferring_) {
    UMA_HISTOGRAM_TIMES("Android.Intent.InterceptNavigationDeferDuration",
                        base::TimeTicks::Now() - defer_start_);
    deferring_ = false;
    if (should_ignore_) {
      CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
    } else {
      Resume();
    }
    // Careful, |this| may be deleted be either the Resume or Cancel calls.
  }
}

bool InterceptNavigationThrottle::ShouldCheckAsynchronously() const {
  // Do not apply the async optimization for:
  // - Throttles in non-async mode.
  // - POST navigations, to ensure we aren't violating idempotency.
  // - Subframe navigations, which aren't observed on Android, and should be
  //   fast on other platforms.
  // - non-http/s URLs, which are more likely to be intercepted.
  // - If we're already deferring, the navigation is blocked, so checking async
  //   is counter-productive.
  return mode_ == SynchronyMode::kAsync &&
         navigation_handle()->IsInMainFrame() &&
         !navigation_handle()->IsPost() &&
         navigation_handle()->GetURL().SchemeIsHTTPOrHTTPS() &&
         base::FeatureList::IsEnabled(kAsyncCheck) && !deferring_;
}

base::WeakPtr<InterceptNavigationThrottle>
InterceptNavigationThrottle::GetWeakPtrForTesting() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace navigation_interception
