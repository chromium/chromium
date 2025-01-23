// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/intercept_navigation_throttle.h"

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

namespace navigation_interception {

// Note: this feature is a no-op on non-Android platforms.
BASE_FEATURE(kAsyncCheck,
             "AsyncNavigationIntercept",
             base::FEATURE_ENABLED_BY_DEFAULT);

InterceptNavigationThrottle::InterceptNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    CheckCallback should_ignore_callback,
    SynchronyMode async_mode,
    std::optional<base::RepeatingClosure> finish_async_work_callback)
    : content::NavigationThrottle(navigation_handle),
      should_ignore_callback_(should_ignore_callback),
      finish_async_work_callback_(finish_async_work_callback),
      ui_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      mode_(async_mode) {
  CHECK(mode_ == SynchronyMode::kSync ||
        finish_async_work_callback_.has_value());
}

InterceptNavigationThrottle::~InterceptNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillStartRequest() {
  DCHECK(!should_ignore_);
  DCHECK(!navigation_handle()->WasServerRedirect());
  return CheckIfShouldIgnoreNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillRedirectRequest() {
  FinishPendingCheck();
  if (should_ignore_) {
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }
  DCHECK(navigation_handle()->WasServerRedirect());
  return CheckIfShouldIgnoreNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillProcessResponse() {
  FinishPendingCheck();
  if (should_ignore_)
    return content::NavigationThrottle::CANCEL_AND_IGNORE;

  return content::NavigationThrottle::PROCEED;
}

const char* InterceptNavigationThrottle::GetNameForLogging() {
  return "InterceptNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::CheckIfShouldIgnoreNavigation() {
  bool async = ShouldCheckAsynchronously();
  pending_check_ = true;
  auto weak_this = weak_factory_.GetWeakPtr();
  should_ignore_callback_.Run(
      navigation_handle(), async,
      base::BindOnce(&InterceptNavigationThrottle::OnCheckComplete, weak_this));
  // Clients should not synchronously cause the navigation to be deleted.
  CHECK(weak_this);
  if (pending_check_) {
    CHECK(async);
    return content::NavigationThrottle::PROCEED;
  }
  if (should_ignore_) {
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }
  return content::NavigationThrottle::PROCEED;
}

void InterceptNavigationThrottle::FinishPendingCheck() {
  if (pending_check_) {
    finish_async_work_callback_->Run();
    CHECK(!pending_check_);
  }
}

void InterceptNavigationThrottle::OnCheckComplete(bool should_ignore) {
  should_ignore_ = should_ignore;
  pending_check_ = false;
}

bool InterceptNavigationThrottle::ShouldCheckAsynchronously() const {
  // Do not apply the async optimization for:
  // - Throttles in non-async mode.
  // - POST navigations, to ensure we aren't violating idempotency.
  // - Subframe navigations, which aren't observed on Android, and should be
  //   fast on other platforms.
  // - non-http/s URLs, which are more likely to be intercepted.
  return mode_ == SynchronyMode::kAsync &&
         navigation_handle()->IsInMainFrame() &&
         !navigation_handle()->IsPost() &&
         navigation_handle()->GetURL().SchemeIsHTTPOrHTTPS() &&
         base::FeatureList::IsEnabled(kAsyncCheck);
}

}  // namespace navigation_interception
