// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_launch/web_launch_files_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#endif

namespace {
// Returns the profile where we should launch System Web Apps into. It returns
// the most appropriate profile for launching, if the provided |profile| is
// unsuitable. It returns nullptr if the we can't find a suitable profile.
Profile* GetProfileForSystemWebAppLaunch(Profile* profile) {
  DCHECK(profile);

  // We can't launch into certain profiles, and we can't find a suitable
  // alternative.
  if (profile->IsSystemProfile())
    return nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return nullptr;
#endif

  // For a guest sessions, launch into the primary off-the-record profile, which
  // is used for browsing in guest sessions. We do this because the "original"
  // profile of the guest session can't create windows.
  if (profile->IsGuestSession())
    return profile->GetPrimaryOTRProfile();

  // We don't support launching SWA in incognito profiles, use the original
  // profile if an incognito profile is provided (with the exception of guest
  // session, which is implemented with an incognito profile, thus it is handled
  // above).
  if (profile->IsIncognitoProfile())
    return profile->GetOriginalProfile();

  // Use the profile provided in other scenarios.
  return profile;
}
}  // namespace

namespace web_app {

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

void LaunchSystemWebAppAsync(Profile* profile,
                             const SystemAppType type,
                             const SystemAppLaunchParams& params,
                             apps::mojom::WindowInfoPtr window_info) {
  // Terminal should be launched with crostini::LaunchTerminal*.
  DCHECK(type != SystemAppType::TERMINAL);

  // TODO(https://crbug.com/1135863): Implement a SWA-wide approach to handle
  // launching (or link capturing) from incognito.
  if (type == SystemAppType::SETTINGS) {
    // In non-guest incognito profile, OS Settings will silently launch into
    // the original profile.
    if (!profile->IsGuestSession() && profile->IsIncognitoProfile()) {
      profile = profile->GetOriginalProfile();
    }
  }

  Profile* profile_for_launch = GetProfileForSystemWebAppLaunch(profile);
  if (profile_for_launch == nullptr || profile_for_launch != profile) {
    // The provided profile can't launch system web apps. Complain about this so
    // we can catch the call site, and ask them to pick the right profile.
    base::debug::DumpWithoutCrashing();

    DVLOG(1)
        << "LaunchSystemWebAppAsync is called on a profile that can't launch "
           "system web apps. Please check the profile you are using is correct."
        << (profile_for_launch
                ? "Instead, launch the app into a suitable profile "
                  "based on your intention."
                : "Can't find a suitable profile based on the provided "
                  "argument. Thus ignore the launch request.");

    NOTREACHED();

    if (profile_for_launch == nullptr)
      return;
  }

  const base::Optional<AppId> app_id =
      GetAppIdForSystemWebApp(profile_for_launch, type);
  if (!app_id)
    return;

  auto* app_service =
      apps::AppServiceProxyFactory::GetForProfile(profile_for_launch);
  DCHECK(app_service);

  auto event_flags = apps::GetEventFlags(
      apps::mojom::LaunchContainer::kLaunchContainerNone,
      WindowOpenDisposition::NEW_WINDOW, /* prefer_container */ false);

  if (params.url.is_empty()) {
    app_service->Launch(app_id.value(), event_flags, params.launch_source,
                        std::move(window_info));
  } else {
    DCHECK(params.url.is_valid());
    app_service->LaunchAppWithUrl(app_id.value(), event_flags, params.url,
                                  params.launch_source, std::move(window_info));
  }
}

Browser* LaunchSystemWebAppImpl(Profile* profile,
                                SystemAppType app_type,
                                const GURL& url,
                                apps::AppLaunchParams& params) {
  // Exit early if we can't create browser windows (e.g. when browser is
  // shutting down, or a wrong profile is given).
  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    return nullptr;
  }

  auto* provider = WebAppProvider::Get(profile);
  if (!provider)
    return nullptr;

  DCHECK(url.GetOrigin() ==
         provider->registrar().GetAppLaunchUrl(params.app_id).GetOrigin());

  // Make sure we have a browser for app.  Always reuse an existing browser for
  // popups, otherwise check app type whether we should use a single window.
  // TODO(crbug.com/1060423): Allow apps to control whether popups are single.
  Browser* browser = nullptr;
  Browser::Type browser_type = Browser::TYPE_APP;
  if (params.disposition == WindowOpenDisposition::NEW_POPUP)
    browser_type = Browser::TYPE_APP_POPUP;
  if (browser_type == Browser::TYPE_APP_POPUP ||
      provider->system_web_app_manager().IsSingleWindow(app_type)) {
    browser = FindSystemWebAppBrowser(profile, app_type, browser_type);
  }

  bool can_resize =
      provider->system_web_app_manager().IsResizeableWindow(app_type);

  bool can_maximize =
      provider->system_web_app_manager().IsMaximizableWindow(app_type);

  if (!browser) {
    browser =
        CreateWebApplicationWindow(profile, params.app_id, params.disposition,
                                   params.restore_id, can_resize, can_maximize);
  }

  // Navigate application window to application's |url| if necessary.
  // Help app always navigates because its url might not match the url inside
  // the iframe, and the iframe's url is the one that matters.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetWebContentsAt(0);
  if (!web_contents || web_contents->GetURL() != url ||
      app_type == SystemAppType::HELP) {
    web_contents = NavigateWebApplicationWindow(
        browser, params.app_id, url, WindowOpenDisposition::CURRENT_TAB);
  }

  // Send launch files.
  if (provider->os_integration_manager().IsFileHandlingAPIAvailable(
          params.app_id)) {
    if (provider->system_web_app_manager().AppShouldReceiveLaunchDirectory(
            app_type)) {
      web_launch::WebLaunchFilesHelper::SetLaunchDirectoryAndLaunchPaths(
          web_contents, web_contents->GetURL(),
          GetLaunchDirectory(params.launch_files), params.launch_files);
    } else {
      web_launch::WebLaunchFilesHelper::SetLaunchPaths(
          web_contents, web_contents->GetURL(), params.launch_files);
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // LaunchSystemWebAppImpl may be called with a profile associated with an
  // inactive (background) desktop (e.g. when multiple users are logged in).
  // Here we move the newly created browser window (or the existing one on the
  // inactive desktop) to the current active (visible) desktop, so the user
  // always sees the launched app.
  multi_user_util::MoveWindowToCurrentDesktop(
      browser->window()->GetNativeWindow());
#endif

  browser->window()->Show();
  return browser;
}

void FlushSystemWebAppLaunchesForTesting(Profile* profile) {
  Profile* profile_for_launch = GetProfileForSystemWebAppLaunch(profile);
  CHECK(profile_for_launch)
      << "FlushSystemWebAppLaunchesForTesting is called for a profile that "
         "can't run System Apps. Check your code.";
  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(app_service_proxy);
  app_service_proxy->FlushMojoCallsForTesting();  // IN-TEST
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
