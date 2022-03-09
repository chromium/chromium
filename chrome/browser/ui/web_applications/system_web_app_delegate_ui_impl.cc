// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"

#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_launch_queue.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace web_app {

Browser* SystemWebAppDelegate::LaunchAndNavigateSystemWebApp(
    Profile* profile,
    WebAppProvider* provider,
    const GURL& url,
    const apps::AppLaunchParams& params) const {
  Browser::Type browser_type =
      (params.disposition == WindowOpenDisposition::NEW_POPUP)
          ? Browser::TYPE_APP_POPUP
          : Browser::TYPE_APP;

  // Always find an existing window, so that we can offset the screen
  // coordinates from a previously opened one.
  Browser* browser = FindSystemWebAppBrowser(profile, GetType(), browser_type);

  // System Web App windows can't be properly restored without storing the app
  // type. Until that is implemented, skip them for session restore.
  // TODO(crbug.com/1003170): Enable session restore for System Web Apps by
  // passing through the underlying value of params.omit_from_session_restore.
  constexpr bool kOmitFromSessionRestore = true;

  // Always reuse an existing browser for popups, otherwise check app type
  // whether we should use a single window.
  // TODO(crbug.com/1060423): Allow apps to control whether popups are single.
  const bool reuse_existing_window =
      browser_type == Browser::TYPE_APP_POPUP || ShouldReuseExistingWindow();

  bool started_new_navigation = false;
  if (!browser) {
    browser = CreateWebApplicationWindow(
        profile, params.app_id, params.disposition, params.restore_id,
        kOmitFromSessionRestore, ShouldAllowResize(), ShouldAllowMaximize());
    started_new_navigation = true;
  } else if (!reuse_existing_window) {
    gfx::Rect initial_bounds = browser->window()->GetRestoredBounds();
    initial_bounds.Offset(20, 20);
    browser = CreateWebApplicationWindow(
        profile, params.app_id, params.disposition, params.restore_id,
        kOmitFromSessionRestore, ShouldAllowResize(), ShouldAllowMaximize(),
        initial_bounds);
    started_new_navigation = true;
  }

  // Navigate application window to application's |url| if necessary.
  // Help app always navigates because its url might not match the url inside
  // the iframe, and the iframe's url is the one that matters.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetWebContentsAt(0);
  if (!web_contents || web_contents->GetURL() != url ||
      GetType() == SystemAppType::HELP) {
    web_contents = NavigateWebApplicationWindow(
        browser, params.app_id, url, WindowOpenDisposition::CURRENT_TAB);
    started_new_navigation = true;
  }

  // Send launch files.
  if (provider->os_integration_manager().IsFileHandlingAPIAvailable(
          params.app_id)) {
    base::FilePath launch_dir = GetLaunchDirectory(params);

    if (!launch_dir.empty() || !params.launch_files.empty()) {
      WebAppLaunchParams launch_params;
      launch_params.started_new_navigation = started_new_navigation;
      launch_params.app_id = params.app_id;
      launch_params.target_url = web_contents->GetURL();
      launch_params.dir = std::move(launch_dir);
      launch_params.paths = params.launch_files;
      WebAppTabHelper::FromWebContents(web_contents)
          ->EnsureLaunchQueue()
          .Enqueue(std::move(launch_params));
    }
  }

  return browser;
}

}  // namespace web_app
