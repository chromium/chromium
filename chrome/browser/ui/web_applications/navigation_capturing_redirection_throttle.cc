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
void NavigationCapturingRedirectionThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  registry.AddThrottle(
      base::WrapUnique(new NavigationCapturingRedirectionThrottle(registry)));
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

  return result;
}

NavigationCapturingRedirectionThrottle::NavigationCapturingRedirectionThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

}  // namespace web_app
