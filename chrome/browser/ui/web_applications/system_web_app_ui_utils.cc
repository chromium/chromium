// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/chromeos/printing/print_management/print_management_uma.h"
#include "chrome/browser/installable/installable_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_launch/web_launch_files_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"

namespace web_app {
namespace {

void LogPrintManagementEntryPoints(apps::mojom::AppLaunchSource source) {
  if (source == apps::mojom::AppLaunchSource::kSourceAppLauncher) {
    base::UmaHistogramEnumeration("Printing.CUPS.PrintManagementAppEntryPoint",
                                  PrintManagementAppEntryPoint::kLauncher);
  } else if (source == apps::mojom::AppLaunchSource::kSourceIntentUrl) {
    base::UmaHistogramEnumeration("Printing.CUPS.PrintManagementAppEntryPoint",
                                  PrintManagementAppEntryPoint::kBrowser);
  }
}

}  // namespace

base::Optional<SystemAppType> GetSystemWebAppTypeForAppId(Profile* profile,
                                                          AppId app_id) {
  auto* provider = WebAppProvider::Get(profile);
  return provider ? provider->system_web_app_manager().GetSystemAppTypeForAppId(
                        app_id)
                  : base::Optional<SystemAppType>();
}

base::Optional<AppId> GetAppIdForSystemWebApp(Profile* profile,
                                              SystemAppType app_type) {
  auto* provider = WebAppProvider::Get(profile);
  return provider
             ? provider->system_web_app_manager().GetAppIdForSystemApp(app_type)
             : base::Optional<AppId>();
}

base::Optional<apps::AppLaunchParams> CreateSystemWebAppLaunchParams(
    Profile* profile,
    SystemAppType app_type,
    int64_t display_id) {
  base::Optional<AppId> app_id = GetAppIdForSystemWebApp(profile, app_type);
  // TODO(calamity): Decide whether to report app launch failure or CHECK fail.
  if (!app_id)
    return base::nullopt;

  auto* provider = WebAppProvider::Get(profile);
  DCHECK(provider);

  DisplayMode display_mode =
      provider->registrar().GetAppEffectiveDisplayMode(app_id.value());

  // TODO(crbug/1113502): Plumb through better launch sources from callsites.
  apps::AppLaunchParams params = apps::CreateAppIdLaunchParamsWithEventFlags(
      app_id.value(), /*event_flags=*/0,
      apps::mojom::AppLaunchSource::kSourceChromeInternal, display_id,
      /*fallback_container=*/
      ConvertDisplayModeToAppLaunchContainer(display_mode));
  params.launch_source = apps::mojom::LaunchSource::kFromChromeInternal;

  return params;
}

namespace {
base::FilePath GetLaunchDirectory(
    const std::vector<base::FilePath>& launch_files) {
  // |launch_dir| is the directory that contains all |launch_files|. If
  // there are no launch files, launch_dir is empty.
  base::FilePath launch_dir =
      launch_files.size() ? launch_files[0].DirName() : base::FilePath();

#if DCHECK_IS_ON()
  // Check |launch_files| all come from the same directory.
  if (!launch_dir.empty()) {
    for (auto path : launch_files) {
      DCHECK_EQ(launch_dir, path.DirName());
    }
  }
#endif

  return launch_dir;
}
}  // namespace

Browser* LaunchSystemWebApp(Profile* profile,
                            SystemAppType app_type,
                            const GURL& url,
                            base::Optional<apps::AppLaunchParams> params,
                            bool* did_create) {
  auto* provider = WebAppProvider::Get(profile);

  if (!provider)
    return nullptr;

  if (!params) {
    params = CreateSystemWebAppLaunchParams(profile, app_type,
                                            display::kInvalidDisplayId);
  }
  if (!params)
    return nullptr;
  params->override_url = url;

  DCHECK_EQ(params->app_id, *GetAppIdForSystemWebApp(profile, app_type));

  // TODO(crbug/1117655): The file manager records metrics directly when opening
  // a file registered to an app, but can't tell if an SWA will ultimately be
  // used to open it. Remove this when the file manager code is moved into
  // the app service.
  if (params->launch_source != apps::mojom::LaunchSource::kFromFileManager) {
    apps::RecordAppLaunch(params->app_id, params->launch_source);
  }
  // Log enumerated entry point for Print Management App. Only log here if the
  // app was launched from the browser (omnibox) or from the system launcher.
  if (app_type == SystemAppType::PRINT_MANAGEMENT) {
    LogPrintManagementEntryPoints(params->source);
  }

  // Make sure we have a browser for app.  Always reuse an existing browser for
  // popups, otherwise check app type whether we should use a single window.
  // TODO(crbug.com/1060423): Allow apps to control whether popups are single.
  Browser* browser = nullptr;
  Browser::Type browser_type = Browser::TYPE_APP;
  if (params->disposition == WindowOpenDisposition::NEW_POPUP)
    browser_type = Browser::TYPE_APP_POPUP;
  if (browser_type == Browser::TYPE_APP_POPUP ||
      provider->system_web_app_manager().IsSingleWindow(app_type)) {
    browser = FindSystemWebAppBrowser(profile, app_type, browser_type);
  }

  // We create the app window if no existing browser found.
  if (did_create)
    *did_create = !browser;

  content::WebContents* web_contents = nullptr;

  // TODO(crbug.com/1129340): Remove these lines and make CCA resizeable after
  // CCA supports responsive UI.
  bool can_resize = app_type != SystemAppType::CAMERA;
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)) {
    if (!browser)
      browser = CreateWebApplicationWindow(profile, params->app_id,
                                           params->disposition, can_resize);

    // Navigate application window to application's |url| if necessary.
    // Help app always navigates because its url might not match the url inside
    // the iframe, and the iframe's url is the one that matters.
    web_contents = browser->tab_strip_model()->GetWebContentsAt(0);
    if (!web_contents || web_contents->GetURL() != url ||
        app_type == SystemAppType::HELP) {
      web_contents = NavigateWebApplicationWindow(
          browser, params->app_id, url, WindowOpenDisposition::CURRENT_TAB);
    }
  } else {
    if (!browser)
      browser = CreateApplicationWindow(profile, *params, url, can_resize);

    // Navigate application window to application's |url| if necessary.
    // Help app always navigates because its url might not match the url inside
    // the iframe, and the iframe's url is the one that matters.
    web_contents = browser->tab_strip_model()->GetWebContentsAt(0);
    if (!web_contents || web_contents->GetURL() != url ||
        app_type == SystemAppType::HELP) {
      web_contents = NavigateApplicationWindow(
          browser, *params, url, WindowOpenDisposition::CURRENT_TAB);
    }
  }

  // Send launch files.
  if (provider->os_integration_manager().IsFileHandlingAPIAvailable(
          params->app_id)) {
    if (provider->system_web_app_manager().AppShouldReceiveLaunchDirectory(
            app_type)) {
      web_launch::WebLaunchFilesHelper::SetLaunchDirectoryAndLaunchPaths(
          web_contents, web_contents->GetURL(),
          GetLaunchDirectory(params->launch_files), params->launch_files);
    } else {
      web_launch::WebLaunchFilesHelper::SetLaunchPaths(
          web_contents, web_contents->GetURL(), params->launch_files);
    }
  }

  // TODO(crbug.com/1114939): Need to make sure the browser is shown on the
  // correct desktop, when used in multi-profile mode.
  browser->window()->Show();
  return browser;
}

Browser* FindSystemWebAppBrowser(Profile* profile,
                                 SystemAppType app_type,
                                 Browser::Type browser_type) {
  // TODO(calamity): Determine whether, during startup, we need to wait for
  // app install and then provide a valid answer here.
  base::Optional<AppId> app_id = GetAppIdForSystemWebApp(profile, app_type);
  if (!app_id)
    return nullptr;

  auto* provider = WebAppProvider::Get(profile);
  DCHECK(provider);

  if (!provider->registrar().IsInstalled(app_id.value()))
    return nullptr;

  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile || browser->type() != browser_type)
      continue;

    if (GetAppIdFromApplicationName(browser->app_name()) == app_id.value())
      return browser;
  }

  return nullptr;
}

bool IsSystemWebApp(Browser* browser) {
  DCHECK(browser);
  return browser->app_controller() &&
         browser->app_controller()->is_for_system_web_app();
}

bool IsBrowserForSystemWebApp(Browser* browser, SystemAppType type) {
  DCHECK(browser);
  return browser->app_controller() &&
         browser->app_controller()->system_app_type() == type;
}

base::Optional<SystemAppType> GetCapturingSystemAppForURL(Profile* profile,
                                                          const GURL& url) {
  auto* provider = WebAppProvider::Get(profile);

  if (!provider)
    return base::nullopt;

  return provider->system_web_app_manager().GetCapturingSystemAppForURL(url);
}

gfx::Size GetSystemWebAppMinimumWindowSize(Browser* browser) {
  DCHECK(browser);
  if (!browser->app_controller())
    return gfx::Size();  // Not an app.

  auto* app_controller = browser->app_controller();
  if (!app_controller->HasAppId())
    return gfx::Size();

  auto* provider = WebAppProvider::Get(browser->profile());
  if (!provider)
    return gfx::Size();

  return provider->system_web_app_manager().GetMinimumWindowSize(
      app_controller->GetAppId());
}

}  // namespace web_app
