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
// transferring a `Browser` instance everywhere.
// Note: Apps can be configured to open in a browser tab or open in a standalone
// window.
enum class NavigationHandlingInitialResult {
  // A browser tab was opened that wasn't the result of web app navigation
  // capturing, but due to redirection the final behavior could change.
  // Note: New context & capturable behavior for open-in-browser-tab apps apply
  // to the cases below, and are not part of this category.
  kBrowserTab = 0,
  // The web app system handles the navigation and launches a new app, but it
  // wasn't captured as it was triggered by a shift or middle click. Launch
  // parameters are enqueued.
  kNavigateCaptured = 1,
  // The capturing logic forced this to launch the app in a new context, with
  // the same behavior of `navigate-new`. This is used when it was a
  // user-modified navigation, triggered by a shift or middle click. Launch
  // parameters are enqueued.
  // TODO(crbug.com/370856876): Possibly merge this with kNavigateCaptured +
  // kNavigatedNew
  kForcedNewAppContext = 2,
  // The navigation open an auxiliary context, and thus the 'window container'
  // (app or browser) needs to stay the same.
  kAuxContext = 3,
  // This navigation should be excluded from redirection handling.
  kNotHandledByNavigationHandling = 4,
  kMaxValue = kNotHandledByNavigationHandling
};

// Stores the initial behavior of navigation capturing with respect to launch
// handlers when triggered via left clicks creating a new top level browsing
// context.
enum class InitialNavigationCapturedBehavior {
  kNotHandled = 0,
  kNavigatedExisting = 1,
  kNavigatedNew = 2,
  kMaxValue = kNavigatedNew
};

// Information that will be used to make decisions regarding redirection.
// Includes:
// 2. The initial result of navigation handling by the web app system.
// 3. The app_id of the app window created for the first url that is loaded pre
//    redirection if navigation handling launches the navigation in a new app
//    window.
// 4. The initial `WindowOpenDisposition` of the navigation.
// 5. The effective launch handling mode, provided the initial result of
//    navigation handling was `kNavigateCaptured`. This can never be
//    `kFocusExisting`, since navigation is aborted for that case, causing
//    redirections to be dropped.
class NavigationCapturingRedirectionInfo {
 public:
  NavigationCapturingRedirectionInfo(const NavigationCapturingRedirectionInfo&);
  NavigationCapturingRedirectionInfo& operator=(
      const NavigationCapturingRedirectionInfo&);

  static NavigationCapturingRedirectionInfo Disabled();

  static NavigationCapturingRedirectionInfo AuxiliaryContext(
      const std::optional<webapps::AppId>& source_browser_app_id,
      WindowOpenDisposition disposition);

  // Created for user-modified or capturable navigations that don't have an
  // initial controlling app of the first url.
  static NavigationCapturingRedirectionInfo
  NoInitialActionRedirectionHandlingEligible(
      const std::optional<webapps::AppId>& source_browser_app_id,
      WindowOpenDisposition disposition);

  // Created for non-user modified navigations that usually launch a new app
  // window.
  static NavigationCapturingRedirectionInfo ForcedNewContext(
      const std::optional<webapps::AppId>& source_browser_app_id,
      const webapps::AppId controlling_app_id,
      WindowOpenDisposition disposition);

  // Created for non user modified navigations that result in a capturable
  // navigation launching a new app container (window or tab).
  static NavigationCapturingRedirectionInfo CapturedNewContext(
      const std::optional<webapps::AppId>& source_browser_app_id,
      const webapps::AppId controlling_app_id,
      WindowOpenDisposition disposition);

  // Created for non user modified navigations that result in a capturable
  // navigation opening an existing app container (window or tab). This is
  // triggered if there is a guarantee that an existing app container was
  // already open for the controlling app.
  static NavigationCapturingRedirectionInfo CapturedNavigateExisting(
      const std::optional<webapps::AppId>& source_browser_app_id,
      const webapps::AppId controlling_app_id,
      WindowOpenDisposition disposition);

  ~NavigationCapturingRedirectionInfo();

  // The initial result of navigation capturing on this navigation, on the
  // initial url.
  NavigationHandlingInitialResult initial_nav_handling_result() const {
    return initial_nav_handling_result_;
  }
  // If the navigation occurred in a standalone PWA window, then this is the
  // app_id of that PWA. Otherwise this is `std::nullopt`.
  const std::optional<webapps::AppId>& app_id_source_browser() const {
    return app_id_source_browser_;
  }
  // The id of the capturing app_id of the first navigation, if this was
  // captured or a forced new window.
  // Note: This is currently not populated for aux context navigations, as it's
  // not needed.
  const std::optional<webapps::AppId>& first_navigation_app_id() const {
    return first_navigation_app_id_;
  }
  // The `WindowOpenDisposition` of the first navigation.
  WindowOpenDisposition disposition() const { return disposition_; }

  // If the first navigation was captured, this is the effective result of that
  // capture.
  // TODO(crbug.com/370856876): Possibly merge the
  // NavigationHandlingInitialResult and InitialNavigationCapturedBehavior, as
  // behavior matches for new context.
  InitialNavigationCapturedBehavior effective_launch_handling_mode() const {
    return effective_launch_handling_mode_;
  }

 private:
  NavigationCapturingRedirectionInfo(
      const std::optional<webapps::AppId>& source_browser_app_id,
      NavigationHandlingInitialResult initial_nav_handling_result,
      const std::optional<webapps::AppId>& first_navigation_app_id,
      WindowOpenDisposition disposition,
      InitialNavigationCapturedBehavior effective_launch_handling_mode);

  std::optional<webapps::AppId> app_id_source_browser_;
  NavigationHandlingInitialResult initial_nav_handling_result_ =
      NavigationHandlingInitialResult::kBrowserTab;
  std::optional<webapps::AppId> first_navigation_app_id_;
  WindowOpenDisposition disposition_ = WindowOpenDisposition::UNKNOWN;
  InitialNavigationCapturedBehavior effective_launch_handling_mode_ =
      InitialNavigationCapturedBehavior::kNotHandled;
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
