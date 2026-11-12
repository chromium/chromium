// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_WINDOW_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ui/ash/network/network_portal_web_contents_observer.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace ash {

class BrowserDelegate;

// Class controlling the captive portal signin window.
// See also the NetworkPortalSigninController class.
class NetworkPortalSigninWindow {
 public:
  static NetworkPortalSigninWindow* Get();

  NetworkPortalSigninWindow(const NetworkPortalSigninWindow&) = delete;
  NetworkPortalSigninWindow& operator=(const NetworkPortalSigninWindow&) =
      delete;
  ~NetworkPortalSigninWindow();

  // Shows the signin window.
  void Show(const GURL& url);

  BrowserDelegate* GetBrowserForTesting();
  content::WebContents* GetWebContentsForTesting();

  int portal_detection_requested_for_testing() const {
    return window_observer_
               ? window_observer_->portal_detection_requested_for_testing()
               : 0;
  }

 protected:
  friend class base::NoDestructor<NetworkPortalSigninWindow>;

  NetworkPortalSigninWindow();

 private:
  BrowserDelegate* GetBrowser();

  SessionID window_session_id_{SessionID::InvalidValue()};
  std::unique_ptr<NetworkPortalWebContentsObserver> window_observer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_WINDOW_H_
