// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/link_capturing_redirect_navigation_throttle.h"

#include "chrome/browser/web_applications/navigation_capturing_information_forwarder.h"
#include "chrome/browser/web_applications/navigation_capturing_navigation_handle_user_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/window_open_disposition.h"

namespace web_app {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
LinkCapturingRedirectNavigationThrottle::MaybeCreate(
    content::NavigationHandle* handle) {
  return base::WrapUnique(new LinkCapturingRedirectNavigationThrottle(handle));
}

LinkCapturingRedirectNavigationThrottle::
    ~LinkCapturingRedirectNavigationThrottle() = default;

const char* LinkCapturingRedirectNavigationThrottle::GetNameForLogging() {
  return "LinkCapturingRedirectNavigationThrottle";
}

ThrottleCheckResult
LinkCapturingRedirectNavigationThrottle::WillProcessResponse() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return HandleRequest();
}

ThrottleCheckResult LinkCapturingRedirectNavigationThrottle::HandleRequest() {
  // TODO(crbug.com/351775835): This is where the final response of a navigation
  // will be handled.
  return content::NavigationThrottle::PROCEED;
}

LinkCapturingRedirectNavigationThrottle::
    LinkCapturingRedirectNavigationThrottle(
        content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

}  // namespace web_app
