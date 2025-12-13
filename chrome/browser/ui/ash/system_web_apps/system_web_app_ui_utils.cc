// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"

#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/ash/browser_delegate/browser_type.h"
#include "chrome/browser/ash/browser_delegate/browser_type_conversion.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/user_manager/user.h"
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
  if (profile->IsSystemProfile()) {
    return nullptr;
  }
  if (ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }

  // For a guest sessions, launch into the primary off-the-record profile, which
  // is used for browsing in guest sessions. We do this because the "original"
  // profile of the guest session can't create windows.
  if (profile->IsGuestSession()) {
    return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  // We don't support launching SWA in incognito profiles, use the original
  // profile if an incognito profile is provided (with the exception of guest
  // session, which is implemented with an incognito profile, thus it is handled
  // above).
  if (profile->IsIncognitoProfile()) {
    return profile->GetOriginalProfile();
  }

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
  if (!app_id) {
    return std::nullopt;
  }

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
  if (!app_id) {
    return;
  }

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

BrowserDelegate* LaunchSystemWebAppImpl(Profile* profile,
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
  if (!swa_manager) {
    return nullptr;
  }

  auto* provider = web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider) {
    return nullptr;
  }

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

  BrowserDelegate* browser =
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
  web_app::UpdateLaunchStats(browser->GetActiveWebContents(), params.app_id,
                             url);

  // LaunchSystemWebAppImpl may be called with a profile associated with an
  // inactive (background) desktop (e.g. when multiple users are logged in).
  // Here we move the newly created browser window (or the existing one on the
  // inactive desktop) to the current active (visible) desktop, so the user
  // always sees the launched app.
  multi_user_util::MoveWindowToCurrentDesktop(browser->GetNativeWindow());

  browser->Show();
  return browser;
}

Browser* FindSystemWebAppBrowser(Profile* profile,
                                 SystemWebAppType app_type,
                                 Browser::Type browser_type,
                                 const GURL& url) {
  auto* browser = FindSystemWebAppBrowser(
      profile, app_type, FromInternalBrowserType(browser_type), url);
  return browser ? &browser->GetBrowser() : nullptr;
}

BrowserDelegate* FindSystemWebAppBrowser(Profile* profile,
                                         SystemWebAppType app_type,
                                         BrowserType browser_type,
                                         const GURL& url) {
  // TODO(calamity): Determine whether, during startup, we need to wait for
  // app install and then provide a valid answer here.
  std::optional<webapps::AppId> app_id =
      GetAppIdForSystemWebApp(profile, app_type);
  if (!app_id) {
    return nullptr;
  }

  auto* provider = SystemWebAppManager::GetWebAppProvider(profile);
  CHECK(provider);

  if (!provider->registrar_unsafe().IsInstallState(
          app_id.value(),
          {web_app::proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE,
           web_app::proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
           web_app::proto::InstallState::INSTALLED_WITH_OS_INTEGRATION})) {
    return nullptr;
  }

  auto* user = BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  // TODO(crbug.com/369689187): Migrate the Profile parameter to a User
  // parameter and move this check into the call sites where necessary. The only
  // known necessary place is the FindSystemWebAppBrowser call in
  // DiagnosticsDialog::ShowDialog, because it gets used in a shimless RMA
  // session (where no SWAs are run). For other non-user sessions we already
  // bail out in the app_id check above.
  if (!user) {
    return nullptr;
  }

  return BrowserController::GetInstance()->FindWebApp(
      user->GetAccountId(), app_id.value(), browser_type);
}

int CountSystemWebAppBrowsers(Profile* profile, SystemWebAppType app_type) {
  auto* const provider = SystemWebAppManager::GetWebAppProvider(profile);
  const std::optional<webapps::AppId> app_id =
      GetAppIdForSystemWebApp(profile, app_type);
  return provider && app_id.has_value()
             ? provider->ui_manager().GetNumWindowsForApp(app_id.value())
             : 0;
}

bool IsSystemWebApp(Browser* browser) {
  DCHECK(browser);
  return browser->app_controller() && browser->app_controller()->system_app();
}

bool IsBrowserForSystemWebApp(BrowserWindowInterface* browser,
                              SystemWebAppType type) {
  DCHECK(browser);
  web_app::AppBrowserController* const app_controller =
      web_app::AppBrowserController::From(browser);
  return app_controller && app_controller->system_app() &&
         app_controller->system_app()->GetType() == type;
}

std::optional<SystemWebAppType> GetCapturingSystemAppForURL(Profile* profile,
                                                            const GURL& url) {
  SystemWebAppManager* swa_manager = SystemWebAppManager::Get(profile);
  return swa_manager ? swa_manager->GetCapturingSystemAppForURL(url)
                     : std::nullopt;
}

gfx::Size GetSystemWebAppMinimumWindowSize(Browser* browser) {
  DCHECK(browser);
  if (browser->app_controller() && browser->app_controller()->system_app()) {
    return browser->app_controller()->system_app()->GetMinimumWindowSize();
  }

  return gfx::Size();
}

}  // namespace ash
