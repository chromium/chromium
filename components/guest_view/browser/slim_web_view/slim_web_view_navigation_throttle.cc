// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/slim_web_view/slim_web_view_navigation_throttle.h"

#include <memory>

#include "base/logging.h"
#include "base/notreached.h"
#include "components/guest_view/browser/slim_web_view/request_utils.h"
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

  ThrottleCheckResult WillCommitWithoutUrlLoader() override {
    // Navigations that commit without a URL loader (about:blank, about:srcdoc,
    // other empty-document schemes) never reach WillStartRequest(). None of the
    // empty-document URLs are on an HTTP(S) allowlist, and about:blank would
    // inherit the initiator subframe's origin. Cancel them to prevent moving
    // an off-allowlist origin into the main frame. Same-document navigations
    // keep their already-committed allowlisted URL and proceed.
    //
    // Note: Only PROCEED, DEFER, and CANCEL_AND_IGNORE are supported for
    // WillCommitWithoutUrlLoader; BLOCK_REQUEST cannot be used here.
    const GURL& url = navigation_handle()->GetURL();
    if (url.IsAboutBlank() || url.IsAboutSrcdoc() ||
        !url.SchemeIsHTTPOrHTTPS()) {
      return content::NavigationThrottle::CANCEL_AND_IGNORE;
    }
    if (CheckUrl(url).action() != content::NavigationThrottle::PROCEED) {
      return content::NavigationThrottle::CANCEL_AND_IGNORE;
    }
    return PROCEED;
  }

  const char* GetNameForLogging() override {
    return "SlimWebViewNavigationThrottle";
  }

 private:
  ThrottleCheckResult CheckUrl(const GURL& url) {
    // Re-lookup the guest each time to avoid holding a raw pointer that could
    // outlive the guest.
    auto* guest = SlimWebViewGuest::FromNavigationHandle(navigation_handle());
    CHECK(guest) << "The throttle should only be created if there is a guest "
                    "associated with the navigation handle.";
    RequestResourceType resource_type = navigation_handle()->IsInMainFrame()
                                            ? RequestResourceType::kMainFrame
                                            : RequestResourceType::kSubFrame;
    auto result = guest->IsUrlAllowed(resource_type, url);
    if (result.has_value()) {
      return PROCEED;
    }

    DVLOG(2) << "Blocked SlimWebView navigation: " << result.error();
    return content::NavigationThrottle::BLOCK_REQUEST;
  }
};

}  // namespace

void MaybeCreateAndAddSlimWebViewNavigationThrottle(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame() && !handle.IsInPrerenderedMainFrame()) {
    return;
  }

  auto* guest = SlimWebViewGuest::FromNavigationHandle(&handle);
  if (!guest || !guest->HasAllowedOrigins(RequestResourceType::kMainFrame)) {
    return;
  }

  registry.AddThrottle(
      std::make_unique<SlimWebViewNavigationThrottle>(registry));
}

}  // namespace guest_view
