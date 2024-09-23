// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_launch_queue.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace ash {

Browser* SystemWebAppDelegate::GetWindowForLaunch(Profile* profile,
                                                  const GURL& url) const {
  DCHECK(!ShouldShowNewWindowMenuOption())
      << "App can't show 'new window' menu option and reuse windows at "
         "the same time.";
  return FindSystemWebAppBrowser(profile, GetType(), Browser::TYPE_APP);
}

// TODO(crbug.com/40190893): Reduce code duplication between SWA launch code and
// web app launch code, so SWAs can easily maintain feature parity with regular
// web apps (e.g. launch_handler behaviours).
Browser* SystemWebAppDelegate::LaunchAndNavigateSystemWebApp(
    Profile* profile,
    web_app::WebAppProvider* provider,
    const GURL& url,
    const apps::AppLaunchParams& params) const {
  // System Web App windows can't be properly restored without storing the app
  // type. Until that is implemented, skip them for session restore.
  // TODO(crbug.com/40098476): Enable session restore for System Web Apps by
  // passing through the underlying value of params.omit_from_session_restore.
  constexpr bool kOmitFromSessionRestore = true;

  // Always reuse an existing browser for popups. Otherwise let the app decide.
  // TODO(crbug.com/40679012): Allow apps to control whether popups are single.
  Browser* browser =
      (params.disposition == WindowOpenDisposition::NEW_POPUP)
          ? FindSystemWebAppBrowser(profile, GetType(), Browser::TYPE_APP_POPUP)
          : GetWindowForLaunch(profile, url);

  bool started_new_navigation = false;
  if (!browser) {
    Browser::CreateParams create_params = web_app::CreateParamsForApp(
        params.app_id, params.disposition == WindowOpenDisposition::NEW_POPUP,
        /*trusted_source=*/true, /*window_bounds=*/gfx::Rect(), profile,
        /*user_gesture=*/true);
    create_params.restore_id = params.restore_id;
    create_params.omit_from_session_restore = kOmitFromSessionRestore;
    create_params.initial_show_state = ui::mojom::WindowShowState::kDefault;
    create_params.can_resize = ShouldAllowResize();
    create_params.can_maximize = ShouldAllowMaximize();
    create_params.can_fullscreen = ShouldAllowFullscreen();
    browser = web_app::CreateWebAppWindowMaybeWithHomeTab(params.app_id,
                                                          create_params);
    started_new_navigation = true;
  }

  // Navigate application window to application's |url| if necessary.
  // Help app always navigates because its url might not match the url inside
  // the iframe, and the iframe's url is the one that matters.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetWebContentsAt(0);
  if (!web_contents || web_contents->GetURL() != url ||
      GetType() == SystemWebAppType::HELP) {
    NavigateParams nav_params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    // TODO(crbug.com/1308961): Migrate to use PWA pinned home tab when ready.
    if (ShouldPinTab(url)) {
      nav_params.tabstrip_add_types |= AddTabTypes::ADD_PINNED;
    }
    web_contents =
        web_app::NavigateWebAppUsingParams(params.app_id, nav_params);
    started_new_navigation = true;
  }

  // Send launch files.
  if (provider->os_integration_manager().IsFileHandlingAPIAvailable(
          params.app_id)) {
    base::FilePath launch_dir = GetLaunchDirectory(params);

    if (!launch_dir.empty() || !params.launch_files.empty()) {
      web_app::WebAppLaunchParams launch_params;
      launch_params.started_new_navigation = started_new_navigation;
      launch_params.app_id = params.app_id;
      launch_params.target_url = web_contents->GetURL();
      launch_params.dir = std::move(launch_dir);
      launch_params.paths = params.launch_files;
      web_app::WebAppTabHelper::FromWebContents(web_contents)
          ->EnsureLaunchQueue()
          .Enqueue(std::move(launch_params));
    }
  }

  return browser;
}

}  // namespace ash
