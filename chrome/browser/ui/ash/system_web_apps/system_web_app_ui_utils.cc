// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_factory.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/scoped_display_for_new_windows.h"

namespace ash {

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
  if (ProfileHelper::IsSigninProfile(profile))
    return nullptr;

  // For a guest sessions, launch into the primary off-the-record profile, which
  // is used for browsing in guest sessions. We do this because the "original"
  // profile of the guest session can't create windows.
  if (profile->IsGuestSession())
    return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

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

std::optional<SystemWebAppType> GetSystemWebAppTypeForAppId(
    Profile* profile,
    const webapps::AppId& app_id) {
  auto* swa_manager = SystemWebAppManager::Get(profile);
  return swa_manager ? swa_manager->GetSystemAppTypeForAppId(app_id)
                     : std::nullopt;
}

std::optional<webapps::AppId> GetAppIdForSystemWebApp(
    Profile* profile,
    SystemWebAppType app_type) {
  auto* swa_manager = SystemWebAppManager::Get(profile);
  return swa_manager ? swa_manager->GetAppIdForSystemApp(app_type)
                     : std::nullopt;
}

std::optional<apps::AppLaunchParams> CreateSystemWebAppLaunchParams(
    Profile* profile,
    SystemWebAppType app_type,
    int64_t display_id) {
  std::optional<webapps::AppId> app_id =
      GetAppIdForSystemWebApp(profile, app_type);
  // TODO(calamity): Decide whether to report app launch failure or CHECK fail.
  if (!app_id)
    return std::nullopt;

  auto* provider = SystemWebAppManager::GetWebAppProvider(profile);
  DCHECK(provider);

  web_app::DisplayMode display_mode =
      provider->registrar_unsafe().GetAppEffectiveDisplayMode(app_id.value());

  // TODO(crbug.com/40143506): Plumb through better launch sources from
  // callsites.
  apps::AppLaunchParams params = apps::CreateAppIdLaunchParamsWithEventFlags(
      app_id.value(), /*event_flags=*/0,
      apps::LaunchSource::kFromChromeInternal, display_id,
      /*fallback_container=*/
      web_app::ConvertDisplayModeToAppLaunchContainer(display_mode));

  return params;
}

SystemAppLaunchParams::SystemAppLaunchParams() = default;
SystemAppLaunchParams::SystemAppLaunchParams(
    const SystemAppLaunchParams& params) = default;
SystemAppLaunchParams::~SystemAppLaunchParams() = default;

namespace {
void LaunchSystemWebAppAsyncContinue(Profile* profile_for_launch,
                                     const SystemWebAppType type,
                                     const SystemAppLaunchParams& params,
                                     apps::WindowInfoPtr window_info,
                                     apps::LaunchCallback callback) {
  if (profile_for_launch->ShutdownStarted()) {
    return;
  }

  const std::optional<webapps::AppId> app_id =
      GetAppIdForSystemWebApp(profile_for_launch, type);
  if (!app_id)
    return;

  auto* app_service =
      apps::AppServiceProxyFactory::GetForProfile(profile_for_launch);
  DCHECK(app_service);

  auto event_flags = apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                                         /* prefer_container */ false);

  if (!params.launch_paths.empty()) {
    DCHECK(!params.url.has_value())
        << "Launch URL can't be used with launch_paths.";
    app_service->LaunchAppWithFiles(*app_id, event_flags, params.launch_source,
                                    params.launch_paths);
    return;
  }

  if (params.url) {
    DCHECK(params.url->is_valid());
    app_service->LaunchAppWithUrl(*app_id, event_flags, *params.url,
                                  params.launch_source, std::move(window_info),
                                  std::move(callback));
    return;
  }

  app_service->Launch(*app_id, event_flags, params.launch_source,
                      std::move(window_info));
}
}  // namespace

void LaunchSystemWebAppAsync(Profile* profile,
                             const SystemWebAppType type,
                             const SystemAppLaunchParams& params,
                             apps::WindowInfoPtr window_info,
                             std::optional<apps::LaunchCallback> callback) {
  DCHECK(profile);
  // Terminal should be launched with crostini::LaunchTerminal*.
  DCHECK(type != SystemWebAppType::TERMINAL);
  // Callback is only supported when launching with an URL.
  DCHECK(!callback || params.url.has_value());

  // TODO(crbug.com/40723875): Implement a confirmation dialog when
  // changing to a different profile.
  Profile* profile_for_launch = GetProfileForSystemWebAppLaunch(profile);
  if (profile_for_launch == nullptr) {
    // We can't find a suitable profile to launch. Complain about this so we
    // can identify the call site, and ask them to pick the right profile.
    // Note that this is fatal in developer builds.
    DUMP_WILL_BE_NOTREACHED()
        << "LaunchSystemWebAppAsync is called on a profile that can't launch "
           "system web apps. The launch request is ignored. Please check the "
           "profile you are using is correct.";

    // Early return if we can't find a profile to launch.
    return;
  }

  if (type == SystemWebAppType::PERSONALIZATION &&
      profile_for_launch == profile) {
    scalable_iph::ScalableIph* scalable_iph =
        ScalableIphFactory::GetForBrowserContext(profile_for_launch);
    if (scalable_iph) {
      scalable_iph->RecordEvent(
          scalable_iph::ScalableIph::Event::kOpenPersonalizationApp);
    }
  }

  SystemWebAppManager* manager = SystemWebAppManager::Get(profile_for_launch);
  if (!manager) {
    return;
  }

  // Wait for all SWAs to be registered before continuing.
  manager->on_apps_synchronized().Post(
      FROM_HERE,
      base::BindOnce(&LaunchSystemWebAppAsyncContinue, profile_for_launch, type,
                     params, std::move(window_info),
                     callback.has_value() ? std::move(callback.value())
                                          : base::DoNothing()));
}

Browser* LaunchSystemWebAppImpl(Profile* profile,
                                SystemWebAppType app_type,
                                const GURL& url,
                                const apps::AppLaunchParams& params) {
  // Exit early if we can't create browser windows (e.g. when browser is
  // shutting down, or a wrong profile is given).
  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    return nullptr;
  }

  SystemWebAppManager* swa_manager = SystemWebAppManager::Get(profile);
  if (!swa_manager)
    return nullptr;

  auto* provider = web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider)
    return nullptr;

  auto* system_app = swa_manager->GetSystemApp(app_type);

#if BUILDFLAG(IS_CHROMEOS)
  DCHECK(url.DeprecatedGetOriginAsURL() == provider->registrar_unsafe()
                                               .GetAppLaunchUrl(params.app_id)
                                               .DeprecatedGetOriginAsURL() ||
         system_app && system_app->IsUrlInSystemAppScope(url));
#endif

  if (!system_app) {
    LOG(ERROR) << "Can't find delegate for system app url: " << url
               << " Not launching.";
    return nullptr;
  }

  // Place new windows on the specified display.
  display::ScopedDisplayForNewWindows scoped_display(params.display_id);

  Browser* browser =
      system_app->LaunchAndNavigateSystemWebApp(profile, provider, url, params);
  if (!browser) {
    return nullptr;
  }

  // We update web application launch stats (e.g. last launch time), but don't
  // record web app launch metrics.
  //
  // Web app launch metrics reflect user's preference about app launch behavior
  // (e.g. open in a tab or open in a window). This information used to make
  // decisions about web application UI flow.
  //
  // Since users can't configure SWA launch behavior, we don't report these
  // metrics to avoid skewing web app metrics.
  web_app::UpdateLaunchStats(browser->tab_strip_model()->GetActiveWebContents(),
                             params.app_id, url);

  // LaunchSystemWebAppImpl may be called with a profile associated with an
  // inactive (background) desktop (e.g. when multiple users are logged in).
  // Here we move the newly created browser window (or the existing one on the
  // inactive desktop) to the current active (visible) desktop, so the user
  // always sees the launched app.
  multi_user_util::MoveWindowToCurrentDesktop(
      browser->window()->GetNativeWindow());

  browser->window()->Show();
  return browser;
}

Browser* FindSystemWebAppBrowser(Profile* profile,
                                 SystemWebAppType app_type,
                                 Browser::Type browser_type,
                                 const GURL& url) {
  // TODO(calamity): Determine whether, during startup, we need to wait for
  // app install and then provide a valid answer here.
  std::optional<webapps::AppId> app_id =
      GetAppIdForSystemWebApp(profile, app_type);
  if (!app_id)
    return nullptr;

  auto* provider = SystemWebAppManager::GetWebAppProvider(profile);
  DCHECK(provider);

  if (!provider->registrar_unsafe().IsInstalled(app_id.value()))
    return nullptr;

  // Look through all the windows, find a browser for this app. Prefer the most
  // recently active app window.
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (browser->profile() != profile || browser->type() != browser_type ||
        browser->is_delete_scheduled()) {
      continue;
    }

    if (web_app::GetAppIdFromApplicationName(browser->app_name()) !=
        app_id.value()) {
      continue;
    }

    if (!url.is_empty()) {
      // In case a URL is provided, only allow a browser which shows it.
      content::WebContents* content =
          browser->tab_strip_model()->GetActiveWebContents();
      if (!content->GetVisibleURL().EqualsIgnoringRef(url))
        continue;
    }

    return browser;
  }

  return nullptr;
}

bool IsSystemWebApp(Browser* browser) {
  DCHECK(browser);
  return browser->app_controller() && browser->app_controller()->system_app();
}

bool IsBrowserForSystemWebApp(Browser* browser, SystemWebAppType type) {
  DCHECK(browser);
  return browser->app_controller() && browser->app_controller()->system_app() &&
         browser->app_controller()->system_app()->GetType() == type;
}

std::optional<SystemWebAppType> GetCapturingSystemAppForURL(Profile* profile,
                                                            const GURL& url) {
  SystemWebAppManager* swa_manager = SystemWebAppManager::Get(profile);
  return swa_manager ? swa_manager->GetCapturingSystemAppForURL(url)
                     : std::nullopt;
}

gfx::Size GetSystemWebAppMinimumWindowSize(Browser* browser) {
  DCHECK(browser);
  if (browser->app_controller() && browser->app_controller()->system_app())
    return browser->app_controller()->system_app()->GetMinimumWindowSize();

  return gfx::Size();
}

}  // namespace ash
