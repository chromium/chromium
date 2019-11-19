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
    WebviewController* controller)
    : NavigationThrottle(handle),
      url_(handle->GetURL()),
      is_in_main_frame_(handle->IsInMainFrame()),
      controller_(controller) {}

WebviewNavigationThrottle::~WebviewNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
WebviewNavigationThrottle::WillStartRequest() {
  controller_->SendNavigationEvent(this, url_, is_in_main_frame_);

  return content::NavigationThrottle::DEFER;
}

void WebviewNavigationThrottle::ProcessNavigationDecision(
    webview::NavigationDecision decision) {
  if (decision != webview::PREVENT) {
    this->Resume();
    return;
  }
  this->CancelDeferredNavigation(content::NavigationThrottle::CANCEL);
}

const char* WebviewNavigationThrottle::GetNameForLogging() {
  return "WebviewNavigationThrottle";
}

}  // namespace chromecast
