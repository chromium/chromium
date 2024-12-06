// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_NAVIGATION_CAPTURING_NAVIGATION_HANDLE_USER_DATA_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_NAVIGATION_CAPTURING_NAVIGATION_HANDLE_USER_DATA_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "ui/base/window_open_disposition.h"

namespace base {
class Value;
}

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
  // The navigation was captured by an app and it resulted in the creation of a
  // new app window for the navigation. This can only occur when the app opens
  // in a standalone window.
  kNavigateCapturedNewAppWindow = 1,
  // The navigation was captured and it resulted in the creation of a new
  // browser tab for the navigation. This can only occur when the app opens in a
  // browser tab.
  kNavigateCapturedNewBrowserTab = 2,
  // The navigation was captured and it resulted in a existing web contents
  // (either in an app window or browser tab) to be navigated.
  kNavigateCapturingNavigateExisting = 3,
  // The capturing logic forced this to launch the app in a new app window
  // context, with the same behavior of `navigate-new`. This is used when it was
  // a user-modified navigation, triggered by a shift or middle click. Launch
  // parameters are enqueued.
  kForcedNewAppContextAppWindow = 4,
  // Same as above but for an app that opens in a browser tab.
  kForcedNewAppContextBrowserTab = 5,
  // The navigation open an auxiliary context, and thus the 'window container'
  // (app or browser) needs to stay the same.
  kAuxContext = 6,
  // This navigation should be excluded from redirection handling.
  kNotHandledByNavigationHandling = 7,
  kMaxValue = kNotHandledByNavigationHandling
};

// Information that will be used to make decisions regarding redirection.
class NavigationCapturingRedirectionInfo {
 public:
  NavigationCapturingRedirectionInfo(const NavigationCapturingRedirectionInfo&);
  NavigationCapturingRedirectionInfo& operator=(
      const NavigationCapturingRedirectionInfo&);

  static NavigationCapturingRedirectionInfo Disabled();

  static NavigationCapturingRedirectionInfo AuxiliaryContext(
      const std::optional<webapps::AppId>& source_browser_app_id,
      const std::optional<webapps::AppId>& source_tab_app_id,
      WindowOpenDisposition disposition);

  // Created for user-modified or capturable navigations that don't have an
  // initial controlling app of the first url.
  static NavigationCapturingRedirectionInfo
  NoInitialActionRedirectionHandlingEligible(
      const std::optional<webapps::AppId>& source_browser_app_id,
      const std::optional<webapps::AppId>& source_tab_app_id,
      WindowOpenDisposition disposition);

  // Created for non-user modified navigations that usually launch a new app
  // window.
  static NavigationCapturingRedirectionInfo ForcedNewContext(
      const std::optional<webapps::AppId>& source_browser_app_id,
      const std::optional<webapps::AppId>& source_tab_app_id,
      const webapps::AppId& capturing_app_id,
      blink::mojom::DisplayMode capturing_display_mode,
      WindowOpenDisposition disposition);

  // Created for non user modified navigations that result in a capturable
  // navigation launching a new app container (window or tab).
  static NavigationCapturingRedirectionInfo CapturedNewContext(
      const std::optional<webapps::AppId>& source_browser_app_id,
      const std::optional<webapps::AppId>& source_tab_app_id,
      const webapps::AppId& capturing_app_id,
      blink::mojom::DisplayMode capturing_display_mode,
      WindowOpenDisposition disposition);

  // Created for non user modified navigations that result in a capturable
  // navigation opening an existing app container (window or tab). This is
  // triggered if there is a guarantee that an existing app container was
  // already open for the controlling app.
  static NavigationCapturingRedirectionInfo CapturedNavigateExisting(
      const std::optional<webapps::AppId>& source_browser_app_id,
      const std::optional<webapps::AppId>& source_tab_app_id,
      const webapps::AppId& capturing_app_id,
      WindowOpenDisposition disposition);

  ~NavigationCapturingRedirectionInfo();

  // The initial result of navigation capturing on this navigation, on the
  // initial url.
  NavigationHandlingInitialResult initial_nav_handling_result() const {
    return initial_nav_handling_result_;
  }

  // If the navigation occurred in a standalone PWA window, then this is the
  // app_id of that PWA. Otherwise this is `std::nullopt`. Note that this will
  // be `std::nullopt` if the navigation came from a browser tab of an
  // open-in-browser-tab app.
  const std::optional<webapps::AppId>& source_browser_app_id() const {
    return source_browser_app_id_;
  }

  const std::optional<webapps::AppId>& source_tab_app_id() const {
    return source_tab_app_id_;
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

  base::Value ToDebugData() const;

 private:
  NavigationCapturingRedirectionInfo(
      const std::optional<webapps::AppId>& source_browser_app_id,
      const std::optional<webapps::AppId>& source_tab_app_id,
      NavigationHandlingInitialResult initial_nav_handling_result,
      const std::optional<webapps::AppId>& first_navigation_app_id,
      WindowOpenDisposition disposition);

  std::optional<webapps::AppId> source_browser_app_id_;
  std::optional<webapps::AppId> source_tab_app_id_;
  NavigationHandlingInitialResult initial_nav_handling_result_ =
      NavigationHandlingInitialResult::kBrowserTab;
  std::optional<webapps::AppId> first_navigation_app_id_;
  WindowOpenDisposition disposition_ = WindowOpenDisposition::UNKNOWN;
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
  const std::optional<NavigationCapturingRedirectionInfo>& redirection_info() {
    return redirection_info_;
  }

  std::optional<webapps::AppId> launched_app() const { return launched_app_; }

  // Sets the `launched_app` as the id of the app that was/will be launched as a
  // result of this navigation.  Setting `force_iph_off` to `true` will prevent
  // in-product-help from being displayed when it otherwise would. This is used
  // by `MaybePerformAppHandlingTasksInWebContents` to decide if launch params,
  // launch metrics and possible navigation capturing IPH need to be triggered.
  void SetLaunchedAppState(std::optional<webapps::AppId> launched_app,
                           bool force_iph_off);

  // If this navigation triggered a web app launch, this method will queue
  // launch params, record launch metrics and maybe show a navigation capturing
  // IPH. Should be called when it is known that this navigation will commit.
  void MaybePerformAppHandlingTasksInWebContents();

 private:
  NavigationCapturingNavigationHandleUserData(
      content::NavigationHandle& navigation_handle,
      std::optional<NavigationCapturingRedirectionInfo> redirection_info,
      std::optional<webapps::AppId> launched_app,
      bool force_iph_off);

  friend NavigationHandleUserData;

  base::Value ToDebugData() const;

  raw_ref<content::NavigationHandle> navigation_handle_;
  std::optional<NavigationCapturingRedirectionInfo> redirection_info_;
  std::optional<webapps::AppId> launched_app_;
  bool force_iph_off_;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_NAVIGATION_CAPTURING_NAVIGATION_HANDLE_USER_DATA_H_
