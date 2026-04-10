// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/slim_web_view/slim_web_view_navigation_throttle.h"

#include <memory>

#include "base/logging.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "url/gurl.h"

namespace guest_view {

namespace {

class SlimWebViewNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit SlimWebViewNavigationThrottle(
      content::NavigationThrottleRegistry& registry)
      : content::NavigationThrottle(registry) {}

  SlimWebViewNavigationThrottle(const SlimWebViewNavigationThrottle&) = delete;
  SlimWebViewNavigationThrottle& operator=(
      const SlimWebViewNavigationThrottle&) = delete;

  ~SlimWebViewNavigationThrottle() override = default;

  ThrottleCheckResult WillStartRequest() override {
    return CheckUrl(navigation_handle()->GetURL());
  }

  ThrottleCheckResult WillRedirectRequest() override {
    return CheckUrl(navigation_handle()->GetURL());
  }

  const char* GetNameForLogging() override {
    return "SlimWebViewNavigationThrottle";
  }

 private:
  ThrottleCheckResult CheckUrl(const GURL& url) {
    // about:blank is allowed for initialization and internal state resets.
    if (url.IsAboutBlank()) {
      return PROCEED;
    }

    // Only enforce on network navigations. Non-network schemes (e.g.
    // chrome-error://) may be used for internal error pages.
    if (!url.SchemeIsHTTPOrHTTPS()) {
      return PROCEED;
    }

    // Re-lookup the guest each time to avoid holding a raw pointer that could
    // outlive the guest.
    auto* guest =
        SlimWebViewGuest::FromNavigationHandle(navigation_handle());
    if (!guest || guest->IsUrlAllowed(url)) {
      return PROCEED;
    }

    DVLOG(2) << "Blocked SlimWebView navigation outside allowlist: " << url;
    return content::NavigationThrottle::BLOCK_REQUEST;
  }
};

}  // namespace

void MaybeCreateAndAddSlimWebViewNavigationThrottle(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInMainFrame()) {
    return;
  }

  auto* guest = SlimWebViewGuest::FromNavigationHandle(&handle);
  if (!guest || !guest->HasAllowedOrigins()) {
    return;
  }

  registry.AddThrottle(
      std::make_unique<SlimWebViewNavigationThrottle>(registry));
}

}  // namespace guest_view
