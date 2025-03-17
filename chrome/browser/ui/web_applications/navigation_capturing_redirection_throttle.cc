// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/navigation_capturing_redirection_throttle.h"

#include <memory>
#include <optional>

#include "chrome/browser/ui/web_applications/navigation_capturing_process.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

namespace web_app {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
NavigationCapturingRedirectionThrottle::MaybeCreate(
    content::NavigationHandle* handle) {
  return base::WrapUnique(new NavigationCapturingRedirectionThrottle(handle));
}

NavigationCapturingRedirectionThrottle::
    ~NavigationCapturingRedirectionThrottle() = default;

const char* NavigationCapturingRedirectionThrottle::GetNameForLogging() {
  return "NavigationCapturingWebAppRedirectThrottle";
}

ThrottleCheckResult
NavigationCapturingRedirectionThrottle::WillProcessResponse() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ThrottleCheckResult result = content::NavigationThrottle::PROCEED;
  NavigationCapturingProcess* process =
      NavigationCapturingProcess::GetForNavigationHandle(*navigation_handle());
  if (process) {
    result = process->HandleRedirect();
  }

  // If the navigation is not cancelled, this is the time to enqueue launch
  // params, record launch metrics and maybe show a navigation capturing IPH.
  // Note that there is still a small chance that some other navigation throttle
  // will cancel this navigation, so ideally we would wait until the navigation
  // actually commits, but this is an easier place to hook into.
  if (result.action() != content::NavigationThrottle::CANCEL) {
    WebAppLaunchNavigationHandleUserData* handle_user_data =
        WebAppLaunchNavigationHandleUserData::GetForNavigationHandle(
            *navigation_handle());
    if (handle_user_data) {
      handle_user_data->MaybePerformAppHandlingTasksInWebContents();
    }
  }

  return result;
}

NavigationCapturingRedirectionThrottle::NavigationCapturingRedirectionThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

}  // namespace web_app
