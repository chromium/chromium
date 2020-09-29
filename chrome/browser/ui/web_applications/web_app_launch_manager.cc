// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_launch/web_launch_files_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "url/gurl.h"

namespace web_app {

namespace {

ui::WindowShowState DetermineWindowShowState() {
  if (chrome::IsRunningInForcedAppMode())
    return ui::SHOW_STATE_FULLSCREEN;

  return ui::SHOW_STATE_DEFAULT;
}

void SetTabHelperAppId(content::WebContents* web_contents,
                       const std::string& app_id) {
  // TODO(https://crbug.com/1032443):
  // Eventually move this to browser_navigator.cc: CreateTargetContents().
  WebAppTabHelperBase* tab_helper =
      WebAppTabHelperBase::FromWebContents(web_contents);
  DCHECK(tab_helper);
  tab_helper->SetAppId(app_id);
}

}  // namespace

Browser* CreateWebApplicationWindow(Profile* profile,
                                    const std::string& app_id,
                                    WindowOpenDisposition disposition,
                                    bool can_resize) {
  std::string app_name = GenerateApplicationNameFromAppId(app_id);
  gfx::Rect initial_bounds;
  Browser::CreateParams browser_params =
      disposition == WindowOpenDisposition::NEW_POPUP
          ? Browser::CreateParams::CreateForAppPopup(
                app_name, /*trusted_source=*/true, initial_bounds, profile,
                /*user_gesture=*/true)
          : Browser::CreateParams::CreateForApp(
                app_name, /*trusted_source=*/true, initial_bounds, profile,
                /*user_gesture=*/true);
  browser_params.initial_show_state = DetermineWindowShowState();
  browser_params.can_resize = can_resize;
  return new Browser(browser_params);
}

content::WebContents* NavigateWebApplicationWindow(
    Browser* browser,
    const std::string& app_id,
    const GURL& url,
    WindowOpenDisposition disposition) {
  NavigateParams nav_params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  nav_params.disposition = disposition;
  Navigate(&nav_params);

  content::WebContents* const web_contents =
      nav_params.navigated_or_inserted_contents;

  SetTabHelperAppId(web_contents, app_id);
  web_app::SetAppPrefsForWebContents(web_contents);

  return web_contents;
}

WebAppLaunchManager::WebAppLaunchManager(Profile* profile)
    : profile_(profile), provider_(WebAppProvider::Get(profile)) {}

WebAppLaunchManager::~WebAppLaunchManager() = default;

content::WebContents* WebAppLaunchManager::OpenApplication(
    const apps::AppLaunchParams& params) {
  if (!provider_->registrar().IsInstalled(params.app_id))
    return nullptr;

  if (params.container == apps::mojom::LaunchContainer::kLaunchContainerWindow)
    RecordAppWindowLaunch(profile_, params.app_id);

  web_app::OsIntegrationManager& os_integration_manager =
      provider_->os_integration_manager();

  const GURL url =
      params.override_url.is_empty()
          ? os_integration_manager
                .GetMatchingFileHandlerURL(params.app_id, params.launch_files)
                .value_or(provider_->registrar().GetAppLaunchUrl(params.app_id))
          : params.override_url;

  // Place new windows on the specified display.
  display::ScopedDisplayForNewWindows scoped_display(params.display_id);

  // System Web Apps go through their own launch path.
  base::Optional<SystemAppType> system_app_type =
      GetSystemWebAppTypeForAppId(profile_, params.app_id);
  if (system_app_type) {
    Browser* browser =
        LaunchSystemWebApp(profile_, *system_app_type, url, params);
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  Browser* browser = nullptr;
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  if (params.container == apps::mojom::LaunchContainer::kLaunchContainerTab) {
    browser = chrome::FindTabbedBrowser(
        profile_, /*match_original_profiles=*/false, params.display_id);
    if (browser) {
      // For existing browser, ensure its window is activated.
      browser->window()->Activate();
      disposition = params.disposition;
    } else {
      browser =
          new Browser(Browser::CreateParams(Browser::TYPE_NORMAL, profile_,
                                            /*user_gesture=*/true));
    }
  } else {
    if (params.disposition == WindowOpenDisposition::CURRENT_TAB &&
        provider_->registrar().IsInExperimentalTabbedWindowMode(
            params.app_id)) {
      for (Browser* open_browser : *BrowserList::GetInstance()) {
        if (AppBrowserController::IsForWebAppBrowser(open_browser,
                                                     params.app_id)) {
          browser = open_browser;
          break;
        }
      }
    }
    if (!browser) {
      browser = CreateWebApplicationWindow(profile_, params.app_id,
                                           params.disposition);
    }
  }

  content::WebContents* web_contents;
  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    TabStripModel* const model = browser->tab_strip_model();
    content::WebContents* existing_tab = model->GetActiveWebContents();
    const int tab_index = model->GetIndexOfWebContents(existing_tab);

    existing_tab->OpenURL(content::OpenURLParams(
        url,
        content::Referrer::SanitizeForRequest(
            url, content::Referrer(existing_tab->GetURL(),
                                   network::mojom::ReferrerPolicy::kDefault)),
        disposition, ui::PAGE_TRANSITION_AUTO_BOOKMARK,
        /*is_renderer_initiated=*/false));

    // Reset existing_tab as OpenURL() may have clobbered it.
    existing_tab = browser->tab_strip_model()->GetActiveWebContents();
    model->ActivateTabAt(tab_index, {TabStripModel::GestureType::kOther});
    web_contents = existing_tab;
    SetTabHelperAppId(web_contents, params.app_id);
  } else {
    web_contents = NavigateWebApplicationWindow(
        browser, params.app_id, url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }

  if (os_integration_manager.IsFileHandlingAPIAvailable(params.app_id)) {
    web_launch::WebLaunchFilesHelper::SetLaunchPaths(web_contents, url,
                                                     params.launch_files);
  }

  browser->window()->Show();

  // TODO(crbug.com/1014328): Populate WebApp metrics instead of Extensions.

  UMA_HISTOGRAM_ENUMERATION("Extensions.HostedAppLaunchContainer",
                            params.container);
  if (params.container == apps::mojom::LaunchContainer::kLaunchContainerTab) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.AppTabLaunchType",
                              extensions::LAUNCH_TYPE_REGULAR, 100);
  }
  UMA_HISTOGRAM_ENUMERATION("Extensions.BookmarkAppLaunchSource",
                            params.source);
  UMA_HISTOGRAM_ENUMERATION("Extensions.BookmarkAppLaunchContainer",
                            params.container);

  // Record the launch time in the site engagement service. A recent web
  // app launch will provide an engagement boost to the origin.
  SiteEngagementService::Get(profile_)->SetLastShortcutLaunchTime(web_contents,
                                                                  url);
  provider_->registry_controller().SetAppLastLaunchTime(params.app_id,
                                                        base::Time::Now());
  // Refresh the app banner added to homescreen event. The user may have
  // cleared their browsing data since installing the app, which removes the
  // event and will potentially permit a banner to be shown for the site.
  RecordAppBanner(web_contents, url);

  return web_contents;
}

void WebAppLaunchManager::LaunchApplication(
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)> callback) {
  if (!provider_)
    return;

  apps::AppLaunchParams params(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceCommandLine);
  params.command_line = command_line;
  params.current_directory = current_directory;
  params.launch_files = apps::GetLaunchFilesFromCommandLine(command_line);
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu)) {
    params.override_url = GURL(command_line.GetSwitchValueASCII(
        switches::kAppLaunchUrlForShortcutsMenuItem));
  }

  // Wait for the web applications database to load.
  // If the profile and WebAppLaunchManager are destroyed,
  // on_registry_ready will not fire.
  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppLaunchManager::LaunchWebApplication,
                                weak_ptr_factory_.GetWeakPtr(), params,
                                std::move(callback)));
}

void WebAppLaunchManager::LaunchWebApplication(
    apps::AppLaunchParams params,
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)> callback) {
  Browser* browser;
  if (provider_->registrar().IsInstalled(params.app_id)) {
    if (provider_->registrar().GetAppEffectiveDisplayMode(params.app_id) ==
        blink::mojom::DisplayMode::kBrowser) {
      params.container = apps::mojom::LaunchContainer::kLaunchContainerTab;
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    }

    const content::WebContents* web_contents = OpenApplication(params);
    browser = chrome::FindBrowserWithWebContents(web_contents);
    DCHECK(browser);
  } else {
    // Open an empty browser window as the app_id is invalid.
    browser = apps::CreateBrowserWithNewTabPage(profile_);
    params.container = apps::mojom::LaunchContainer::kLaunchContainerNone;
  }
  std::move(callback).Run(browser, params.container);
}

void RecordAppWindowLaunch(Profile* profile, const std::string& app_id) {
  WebAppProvider* provider = WebAppProvider::Get(profile);
  if (!provider)
    return;

  DisplayMode display =
      provider->registrar().GetEffectiveDisplayModeFromManifest(app_id);
  if (display == DisplayMode::kUndefined)
    return;

  DCHECK_LT(DisplayMode::kUndefined, display);
  DCHECK_LE(display, DisplayMode::kMaxValue);
  UMA_HISTOGRAM_ENUMERATION("Launch.WebAppDisplayMode", display);
}

}  // namespace web_app
