// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_NAVIGATION_HANDLE_USER_DATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_NAVIGATION_HANDLE_USER_DATA_H_

#include "content/public/browser/navigation_handle_user_data.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace web_app {

// Data that is tied to the NavigationHandle. Used in the
// LinkCapturingRedirectNavigationThrottle to make final decisions on what the
// outcome of navigation capturing on a redirected navigation should be.
class NavigationCapturingNavigationHandleUserData
    : public content::NavigationHandleUserData<
          NavigationCapturingNavigationHandleUserData> {
 public:
  ~NavigationCapturingNavigationHandleUserData() override;

  // The initial disposition of the navigation (before any normalization) that
  // is currently being controlled by the NavigationHandle. This is set in
  // `Navigate()` and is used in the LinkCapturingRedirectNavigationThrottle to
  // determine how to handle redirections if any.
  WindowOpenDisposition disposition() { return disposition_; }

 private:
  NavigationCapturingNavigationHandleUserData(
      content::NavigationHandle& navigation_handle,
      WindowOpenDisposition disposition);

  friend NavigationHandleUserData;

  WindowOpenDisposition disposition_ = WindowOpenDisposition::UNKNOWN;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_NAVIGATION_HANDLE_USER_DATA_H_
