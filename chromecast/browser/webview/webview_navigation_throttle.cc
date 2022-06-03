// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_navigation_throttle.h"

#include "base/bind.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "chromecast/browser/webview/webview_controller.h"
#include "content/public/browser/navigation_handle.h"

namespace chromecast {

WebviewNavigationThrottle::WebviewNavigationThrottle(
    content::NavigationHandle* handle,
    base::WeakPtr<WebviewController> controller)
    : NavigationThrottle(handle), controller_(std::move(controller)) {}

WebviewNavigationThrottle::~WebviewNavigationThrottle() {
  if (controller_)
    controller_->OnNavigationThrottleDestroyed(this);
}

content::NavigationThrottle::ThrottleCheckResult
WebviewNavigationThrottle::WillStartRequest() {
  controller_->SendNavigationEvent(this, navigation_handle());

  return content::NavigationThrottle::DEFER;
}

void WebviewNavigationThrottle::ProcessNavigationDecision(
    webview::NavigationDecision decision) {
  if (decision != webview::PREVENT) {
    Resume();
    return;
  }
  CancelDeferredNavigation(content::NavigationThrottle::CANCEL);
}

const char* WebviewNavigationThrottle::GetNameForLogging() {
  return "WebviewNavigationThrottle";
}

}  // namespace chromecast
