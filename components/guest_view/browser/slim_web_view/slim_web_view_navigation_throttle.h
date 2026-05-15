// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_NAVIGATION_THROTTLE_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_NAVIGATION_THROTTLE_H_

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

namespace guest_view {

// Enforces an optional allowlist for top-level navigations in SlimWebView.
// If the guest was created with an "allowedOrigins" allowlist, any top-level
// HTTP(S) navigation that does not match the allowlist will be canceled.
void MaybeCreateAndAddSlimWebViewNavigationThrottle(
    content::NavigationThrottleRegistry& registry);

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_NAVIGATION_THROTTLE_H_
