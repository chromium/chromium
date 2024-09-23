// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/link_capturing_redirect_navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

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
LinkCapturingRedirectNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LOG(ERROR) << "WillStartRequest";
  return HandleRequest();
}

ThrottleCheckResult
LinkCapturingRedirectNavigationThrottle::WillRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LOG(ERROR) << "WillRedirectRequest";
  return HandleRequest();
}

ThrottleCheckResult
LinkCapturingRedirectNavigationThrottle::WillProcessResponse() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LOG(ERROR) << "WillProcessResponse";
  return HandleRequest();
}

ThrottleCheckResult LinkCapturingRedirectNavigationThrottle::HandleRequest() {
  content::NavigationHandle* handle = navigation_handle();

  LOG(ERROR) << "HandleRequest (proceed): " << handle->GetURL().spec().c_str();
  return content::NavigationThrottle::PROCEED;
}

LinkCapturingRedirectNavigationThrottle::
    LinkCapturingRedirectNavigationThrottle(
        content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

}  // namespace web_app
