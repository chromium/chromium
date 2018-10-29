// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/intercept_navigation_throttle.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

namespace navigation_interception {

const base::Feature InterceptNavigationThrottle::kAsyncCheck{
    "AsyncNavigationIntercept", base::FEATURE_DISABLED_BY_DEFAULT};

InterceptNavigationThrottle::InterceptNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    CheckCallback should_ignore_callback)
    : content::NavigationThrottle(navigation_handle),
      should_ignore_callback_(should_ignore_callback),
      ui_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {}

InterceptNavigationThrottle::~InterceptNavigationThrottle() {
  UMA_HISTOGRAM_BOOLEAN("Navigation.Intercept.Ignored", should_ignore_);
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillStartRequest() {
  DCHECK(!should_ignore_);
  base::ElapsedTimer timer;

  auto result = CheckIfShouldIgnoreNavigation(false /* is_redirect */);
  UMA_HISTOGRAM_COUNTS_10M("Navigation.Intercept.WillStart",
                           timer.Elapsed().InMicroseconds());
  return result;
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillRedirectRequest() {
  if (should_ignore_)
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  return CheckIfShouldIgnoreNavigation(true /* is_redirect */);
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillFailRequest() {
  return WillFinish();
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillProcessResponse() {
  return WillFinish();
}

const char* InterceptNavigationThrottle::GetNameForLogging() {
  return "InterceptNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillFinish() {
  DCHECK(!deferring_);
  if (should_ignore_)
    return content::NavigationThrottle::CANCEL_AND_IGNORE;

  if (pending_checks_ > 0) {
    deferring_ = true;
    return content::NavigationThrottle::DEFER;
  }

  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::CheckIfShouldIgnoreNavigation(bool is_redirect) {
  if (ShouldCheckAsynchronously()) {
    pending_checks_++;
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&InterceptNavigationThrottle::RunCheckAsync,
                                  weak_factory_.GetWeakPtr(),
                                  GetNavigationParams(is_redirect)));
    return content::NavigationThrottle::PROCEED;
  }
  // No need to set |should_ignore_| since if it is true, we'll cancel the
  // navigation immediately.
  return should_ignore_callback_.Run(navigation_handle()->GetWebContents(),
                                     GetNavigationParams(is_redirect))
             ? content::NavigationThrottle::CANCEL_AND_IGNORE
             : content::NavigationThrottle::PROCEED;
  // Careful, |this| can be deleted at this point.
}

void InterceptNavigationThrottle::RunCheckAsync(
    const NavigationParams& params) {
  DCHECK(base::FeatureList::IsEnabled(kAsyncCheck));
  DCHECK_GT(pending_checks_, 0);
  pending_checks_--;
  bool final_deferred_check = deferring_ && pending_checks_ == 0;
  auto weak_this = weak_factory_.GetWeakPtr();
  bool should_ignore = should_ignore_callback_.Run(
      navigation_handle()->GetWebContents(), params);
  if (!weak_this)
    return;

  should_ignore_ |= should_ignore;
  if (!final_deferred_check)
    return;

  if (should_ignore) {
    CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
  } else {
    Resume();
  }
}

bool InterceptNavigationThrottle::ShouldCheckAsynchronously() const {
  // Do not apply the async optimization for:
  // - POST navigations, to ensure we aren't violating idempotency.
  // - Subframe navigations, which aren't observed on Android, and should be
  //   fast on other platforms.
  // - non-http/s URLs, which are more likely to be intercepted.
  return navigation_handle()->IsInMainFrame() &&
         !navigation_handle()->IsPost() &&
         navigation_handle()->GetURL().SchemeIsHTTPOrHTTPS() &&
         base::FeatureList::IsEnabled(kAsyncCheck);
}

NavigationParams InterceptNavigationThrottle::GetNavigationParams(
    bool is_redirect) const {
  return NavigationParams(
      navigation_handle()->GetURL(), navigation_handle()->GetReferrer(),
      navigation_handle()->HasUserGesture(), navigation_handle()->IsPost(),
      navigation_handle()->GetPageTransition(), is_redirect,
      navigation_handle()->IsExternalProtocol(), true,
      navigation_handle()->IsRendererInitiated(),
      navigation_handle()->GetBaseURLForDataURL());
}

}  // namespace navigation_interception
