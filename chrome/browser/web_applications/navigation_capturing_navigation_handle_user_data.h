// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_NAVIGATION_HANDLE_USER_DATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_NAVIGATION_HANDLE_USER_DATA_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class NavigationHandle;
}  // namespace content

class Browser;

namespace web_app {

// TODO(crbug.com/336371044): Support web apps that open in a new tab.
// The initial result of navigation handling, stored as an enum to prevent
// transferring a `Browser` instance everywhere. The possible use-cases are:
// 1. The web app system does not handle the navigation, so a new browser tab
// opens.
// 2. The web app system handles the navigation and captures it as part of a
// left click with a new top level browsing context. Launch parameters are
// enqueued.
// 3. The web app system handles the navigation and launches a new app, but it
// wasn't captured as it was triggered by a shift or middle click. Launch
// parameters are enqueued.
// 4. The web app system handles the navigation and opens a new app window as
// part of a navigation that created an auxiliary browsing context. This is not
// an app launch, and as such, launch parameters are not enqueued.
// 5. A new web app was launched, but that behavior is not useful for
// redirection purposes, since it was triggered out of a redirection flow.
enum class NavigationHandlingInitialResult {
  kBrowserTab = 0,
  kAppWindowNavigationCaptured = 1,
  kAppWindowForcedNewContext = 2,
  kAppWindowAuxContext = 3,
  kNotHandledByNavigationHandling = 4,
  kMaxValue = kNotHandledByNavigationHandling
};

// Information that will be used to make decisions regarding redirection.
// Includes:
// 1. The app_id of the source app browser if the navigation was triggered from
// an app browser window or from a web app that is set to open in a new tab,
// std::nullopt otherwise.
// 2. The initial result of navigation handling by the web app system.
// 3. The initial `WindowOpenDisposition` of the navigation.
// TODO(crbug.com/370856876): Create a class and add correctness checks.
struct NavigationCapturingRedirectionInfo {
  NavigationCapturingRedirectionInfo();
  ~NavigationCapturingRedirectionInfo();
  NavigationCapturingRedirectionInfo(
      const NavigationCapturingRedirectionInfo& navigation_info);

  std::optional<webapps::AppId> app_id_initial_browser;
  NavigationHandlingInitialResult initial_nav_handling_result =
      NavigationHandlingInitialResult::kBrowserTab;
  std::optional<webapps::AppId> first_navigation_app_id;
  WindowOpenDisposition disposition = WindowOpenDisposition::UNKNOWN;
};

// Data that is tied to the NavigationHandle. Used in the
// `NavigationCapturingRedirectionThrottle` to make final decisions on what the
// outcome of navigation capturing on a redirected navigation should be.
class NavigationCapturingNavigationHandleUserData
    : public content::NavigationHandleUserData<
          NavigationCapturingNavigationHandleUserData> {
 public:
  ~NavigationCapturingNavigationHandleUserData() override;

  // Information necessary to perform different actions based on multiple
  // redirects.
  NavigationCapturingRedirectionInfo redirection_info() {
    return redirection_info_;
  }

 private:
  NavigationCapturingNavigationHandleUserData(
      content::NavigationHandle& navigation_handle,
      NavigationCapturingRedirectionInfo redirection_info);

  friend NavigationHandleUserData;

  NavigationCapturingRedirectionInfo redirection_info_;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_NAVIGATION_HANDLE_USER_DATA_H_
