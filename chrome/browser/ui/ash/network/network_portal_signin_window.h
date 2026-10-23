// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_WINDOW_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

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
    return portal_detection_requested_for_testing_;
  }

 protected:
  friend class base::NoDestructor<NetworkPortalSigninWindow>;

  NetworkPortalSigninWindow();

 private:
  class WindowObserver;

  BrowserDelegate* GetBrowser();

  SessionID window_session_id_{SessionID::InvalidValue()};
  std::unique_ptr<WindowObserver> window_observer_;
  int portal_detection_requested_for_testing_ = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_WINDOW_H_
