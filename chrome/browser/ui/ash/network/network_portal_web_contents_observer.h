// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace ash {

// Observes captive portal sign-in WebContents (in a sign-in window, tab, or
// dialog) to log navigation lifecycle events (redirects, page load HTTP status,
// SSL/net errors, and closure) and trigger portal detection when navigation
// finishes or the WebContents is destroyed.
class NetworkPortalWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit NetworkPortalWebContentsObserver(content::WebContents* web_contents);
  NetworkPortalWebContentsObserver(const NetworkPortalWebContentsObserver&) =
      delete;
  NetworkPortalWebContentsObserver& operator=(
      const NetworkPortalWebContentsObserver&) = delete;
  ~NetworkPortalWebContentsObserver() override;

  int portal_detection_requested_for_testing() const {
    return portal_detection_requested_for_testing_;
  }

 private:
  // content::WebContentsObserver:
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  void RequestPortalDetection();

  int portal_detection_requested_for_testing_ = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_WEB_CONTENTS_OBSERVER_H_
