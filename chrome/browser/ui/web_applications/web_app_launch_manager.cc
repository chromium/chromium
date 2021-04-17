// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/share_target_utils.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_launch/web_launch_files_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "third_party/blink/public/common/features.h"
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

content::WebContents* NavigateWebAppUsingParams(const std::string& app_id,
                                                NavigateParams& nav_params) {
  Navigate(&nav_params);

  content::WebContents* const web_contents =
      nav_params.navigated_or_inserted_contents;

  SetTabHelperAppId(web_contents, app_id);
  web_app::SetAppPrefsForWebContents(web_contents);

  return web_contents;
}

base::Optional<GURL> GetUrlHandlingLaunchUrl(
    WebAppProvider& provider,
    const apps::AppLaunchParams& params) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableUrlHandlers) ||
      !params.url_handler_launch_url.has_value()) {
    return base::nullopt;
  }

  GURL url = params.url_handler_launch_url.value();
  DCHECK(url.is_valid());

  // If URL is not in scope, default to launch URL for now.
  // TODO(crbug.com/1072058): Allow developers to handle out-of-scope URL
  // launch.
  const std::string app_scope =
      provider.registrar().GetAppScope(params.app_id).spec();
  DCHECK(!app_scope.empty());
  return base::StartsWith(url.spec(), app_scope, base::CompareCase::SENSITIVE)
             ? url
             : provider.registrar().GetAppLaunchUrl(params.app_id);
}

// TODO(crbug.com/1019239): Passing a WebAppProvider seems to be a bit of an
// anti-pattern. We should refactor this and other existing functions in this
// file to receive an OsIntegrationManager instead.
base::Optional<GURL> GetProtocolHandlingTranslatedUrl(
    WebAppProvider& provider,
    const apps::AppLaunchParams& params) {
  if (!params.protocol_handler_launch_url.has_value())
    return base::nullopt;

  GURL protocol_url(params.protocol_handler_launch_url.value());
  if (!protocol_url.is_valid())
    return base::nullopt;

  base::Optional<GURL> translated_url =
      provider.os_integration_manager().TranslateProtocolUrl(params.app_id,
                                                             protocol_url);

  return translated_url;
}

GURL GetLaunchUrl(WebAppProvider& provider,
                  const apps::AppLaunchParams& params,
                  const apps::ShareTarget* share_target) {
  if (!params.override_url.is_empty())
    return params.override_url;

  // Handle url_handlers launch
  base::Optional<GURL> url_handler_launch_url =
      GetUrlHandlingLaunchUrl(provider, params);
  if (url_handler_launch_url.has_value())
    return url_handler_launch_url.value();

  // Handle file_handlers launch
  base::Optional<GURL> file_handler_url =
      provider.os_integration_manager().GetMatchingFileHandlerURL(
          params.app_id, params.launch_files);
  if (file_handler_url.has_value())
    return file_handler_url.value();

  // Handle protocol_handlers launch
  base::Optional<GURL> protocol_handler_translated_url =
      GetProtocolHandlingTranslatedUrl(provider, params);
  if (protocol_handler_translated_url.has_value())
    return protocol_handler_translated_url.value();

  // Handle share_target launch
  if (share_target)
    return share_target->action;

  // Default launch
  return provider.registrar().GetAppLaunchUrl(params.app_id);
}

bool IsProtocolHandlerCommandLineArg(const base::CommandLine::StringType& arg) {
#if defined(OS_WIN)
  GURL url(base::WideToUTF16(arg));
#else
  GURL url(arg);
#endif

  if (url.is_valid() && url.has_scheme()) {
    bool has_custom_scheme_prefix = false;
    return blink::IsValidCustomHandlerScheme(url.scheme(),
                                             /* allow_ext_plus_prefix */ false,
                                             has_custom_scheme_prefix);
  }
  return false;
}

bool DoesCommandLineContainProtocolUrl(const base::CommandLine& command_line) {
  for (const auto& arg : command_line.GetArgs()) {
    if (IsProtocolHandlerCommandLineArg(arg)) {
      return true;
    }
  }
  return false;
}

}  // namespace

Browser* CreateWebApplicationWindow(Profile* profile,
                                    const std::string& app_id,
                                    WindowOpenDisposition disposition,
                                    int32_t restore_id,
                                    bool can_resize,
                                    bool can_maximize) {
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  browser_params.restore_id = restore_id;
#endif
  browser_params.can_resize = can_resize;
  browser_params.can_maximize = can_maximize;
  return Browser::Create(browser_params);
}

content::WebContents* NavigateWebApplicationWindow(
    Browser* browser,
    const std::string& app_id,
    const GURL& url,
    WindowOpenDisposition disposition) {
  NavigateParams nav_params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  nav_params.disposition = disposition;
  return NavigateWebAppUsingParams(app_id, nav_params);
}

WebAppLaunchManager::WebAppLaunchManager(Profile* profile)
    : profile_(profile), provider_(WebAppProvider::Get(profile)) {}

WebAppLaunchManager::~WebAppLaunchManager() = default;

content::WebContents* WebAppLaunchManager::OpenApplication(
    apps::AppLaunchParams&& params) {
  if (Browser::GetCreationStatusForProfile(profile_) !=
          Browser::CreationStatus::kOk ||
      !provider_->registrar().IsInstalled(params.app_id)) {
    return nullptr;
  }

  if (params.container == apps::mojom::LaunchContainer::kLaunchContainerWindow)
    RecordAppWindowLaunch(profile_, params.app_id);

  if (GetOpenApplicationCallback())
    return GetOpenApplicationCallback().Run(std::move(params));

  const apps::ShareTarget* const share_target =
      params.intent ? provider_->registrar().GetAppShareTarget(params.app_id)
                    : nullptr;
  const GURL url = GetLaunchUrl(*provider_, params, share_target);
  DCHECK(url.is_valid());

  // Place new windows on the specified display.
  display::ScopedDisplayForNewWindows scoped_display(params.display_id);

  // System Web Apps go through their own launch path.
  base::Optional<SystemAppType> system_app_type =
      GetSystemWebAppTypeForAppId(profile_, params.app_id);
  if (system_app_type) {
    Browser* browser =
        LaunchSystemWebAppImpl(profile_, *system_app_type, url, params);
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
          Browser::Create(Browser::CreateParams(Browser::TYPE_NORMAL, profile_,
                                                /*user_gesture=*/true));
    }
  } else {
    if (params.disposition == WindowOpenDisposition::CURRENT_TAB &&
        provider_->registrar().IsInExperimentalTabbedWindowMode(
            params.app_id)) {
      for (Browser* open_browser : *BrowserList::GetInstance()) {
        if (AppBrowserController::IsForWebApp(open_browser, params.app_id)) {
          browser = open_browser;
          break;
        }
      }
    }
    if (!browser) {
      browser = CreateWebApplicationWindow(
          profile_, params.app_id, params.disposition, params.restore_id);
    }
  }

  content::WebContents* web_contents;
  if (share_target) {
    NavigateParams nav_params =
        NavigateParamsForShareTarget(browser, *share_target, *params.intent);
    nav_params.disposition = disposition;
    web_contents = NavigateWebAppUsingParams(params.app_id, nav_params);
  } else if (disposition == WindowOpenDisposition::CURRENT_TAB) {
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

  web_app::OsIntegrationManager& os_integration_manager =
      provider_->os_integration_manager();
  if (os_integration_manager.IsFileHandlingAPIAvailable(params.app_id)) {
    web_launch::WebLaunchFilesHelper::SetLaunchPaths(web_contents, url,
                                                     params.launch_files);
  }

  browser->window()->Show();

  // TODO(crbug.com/1014328): Populate WebApp metrics instead of Extensions.
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
  site_engagement::SiteEngagementService::Get(profile_)
      ->SetLastShortcutLaunchTime(web_contents, url);
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
    const base::Optional<GURL>& url_handler_launch_url,
    const base::Optional<GURL>& protocol_handler_launch_url,
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)> callback) {
  if (!provider_)
    return;

  apps::mojom::AppLaunchSource launch_source =
      apps::mojom::AppLaunchSource::kSourceCommandLine;
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin) &&
      command_line.HasSwitch(switches::kAppRunOnOsLoginMode)) {
    launch_source = apps::mojom::AppLaunchSource::kSourceRunOnOsLogin;
  }

  apps::AppLaunchParams params(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, launch_source);
  params.command_line = command_line;
  params.current_directory = current_directory;
  if (!DoesCommandLineContainProtocolUrl(command_line)) {
    params.launch_files = apps::GetLaunchFilesFromCommandLine(command_line);
  }
  params.url_handler_launch_url = url_handler_launch_url;
  params.protocol_handler_launch_url = protocol_handler_launch_url;

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
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(params), std::move(callback)));
}

// static
void WebAppLaunchManager::SetOpenApplicationCallbackForTesting(
    OpenApplicationCallback callback) {
  GetOpenApplicationCallback() = std::move(callback);
}

void WebAppLaunchManager::LaunchWebApplication(
    apps::AppLaunchParams&& params,
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)> callback) {
  apps::mojom::LaunchContainer container;
  Browser* browser;
  if (provider_->registrar().IsInstalled(params.app_id)) {
    if (provider_->registrar().GetAppEffectiveDisplayMode(params.app_id) ==
        blink::mojom::DisplayMode::kBrowser) {
      params.container = apps::mojom::LaunchContainer::kLaunchContainerTab;
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    }

    container = params.container;
    const content::WebContents* web_contents =
        OpenApplication(std::move(params));
    browser = chrome::FindBrowserWithWebContents(web_contents);
    DCHECK(browser);
  } else {
    // Open an empty browser window as the app_id is invalid.
    container = apps::mojom::LaunchContainer::kLaunchContainerNone;
    browser = apps::CreateBrowserWithNewTabPage(profile_);
  }
  std::move(callback).Run(browser, container);
}

// static
WebAppLaunchManager::OpenApplicationCallback&
WebAppLaunchManager::GetOpenApplicationCallback() {
  static base::NoDestructor<OpenApplicationCallback> callback;

  return *callback;
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
