// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
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
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/share_target_utils.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_launch/web_launch_files_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
  WebAppTabHelper* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  tab_helper->SetAppId(app_id);
}

content::WebContents* NavigateWebAppUsingParams(const std::string& app_id,
                                                NavigateParams& nav_params) {
  Browser* browser = nav_params.browser;
  const absl::optional<web_app::SystemAppType> capturing_system_app_type =
      web_app::GetCapturingSystemAppForURL(browser->profile(), nav_params.url);
  // TODO(crbug.com/1201820): This block creates conditions where Navigate()
  // returns early and causes a crash. Fail gracefully instead. Further
  // debugging state will be implemented via Chrometto UMA traces.
  if (capturing_system_app_type &&
      (!browser || !web_app::IsBrowserForSystemWebApp(
                       browser, capturing_system_app_type.value()))) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto* user_manager = user_manager::UserManager::Get();
    bool is_kiosk = user_manager && user_manager->IsLoggedInAsAnyKioskApp();
    AppBrowserController* app_controller = browser->app_controller();
    WebAppProvider* web_app_provider =
        WebAppProvider::GetForLocalAppsUnchecked(browser->profile());
    TRACE_EVENT_INSTANT(
        "system_apps", "BadNavigate", [&](perfetto::EventContext ctx) {
          auto* bad_navigate =
              ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                  ->set_chrome_web_app_bad_navigate();
          bad_navigate->set_is_kiosk(is_kiosk);
          bad_navigate->set_has_hosted_app_controller(!!app_controller);
          bad_navigate->set_app_name(browser->app_name());
          if (app_controller && app_controller->system_app()) {
            bad_navigate->set_system_app_type(
                static_cast<uint32_t>(app_controller->system_app()->GetType()));
          }
          bad_navigate->set_web_app_provider_registry_ready(
              web_app_provider->on_registry_ready().is_signaled());
          bad_navigate->set_system_web_app_manager_synchronized(
              web_app_provider->system_web_app_manager()
                  .on_apps_synchronized()
                  .is_signaled());
        });
    UMA_HISTOGRAM_ENUMERATION("WebApp.SystemApps.BadNavigate.Type",
                              capturing_system_app_type.value());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    return nullptr;
  }

  Navigate(&nav_params);

  content::WebContents* const web_contents =
      nav_params.navigated_or_inserted_contents;

  if (web_contents) {
    SetTabHelperAppId(web_contents, app_id);
    web_app::SetAppPrefsForWebContents(web_contents);
  }

  return web_contents;
}

// TODO(crbug.com/1019239): Passing a WebAppProvider seems to be a bit of an
// anti-pattern. We should refactor this and other existing functions in this
// file to receive an OsIntegrationManager instead.
absl::optional<GURL> GetProtocolHandlingTranslatedUrl(
    WebAppProvider& provider,
    const apps::AppLaunchParams& params) {
  if (!params.protocol_handler_launch_url.has_value())
    return absl::nullopt;

  GURL protocol_url(params.protocol_handler_launch_url.value());
  if (!protocol_url.is_valid())
    return absl::nullopt;

  absl::optional<GURL> translated_url =
      provider.os_integration_manager().TranslateProtocolUrl(params.app_id,
                                                             protocol_url);

  return translated_url;
}

WebAppLaunchManager::OpenApplicationCallback&
GetOpenApplicationCallbackForTesting() {
  static base::NoDestructor<WebAppLaunchManager::OpenApplicationCallback>
      callback;
  return *callback;
}

class LaunchProcess {
 public:
  LaunchProcess(Profile& profile, const apps::AppLaunchParams& params);

  content::WebContents* Run();

 private:
  const apps::ShareTarget* MaybeGetShareTarget() const;
  std::tuple<GURL, bool /*is_file_handling*/> GetLaunchUrl(
      const apps::ShareTarget* share_target) const;
  WindowOpenDisposition GetNavigationDisposition(bool is_new_browser) const;
  content::WebContents* MaybeLaunchSystemWebApp(const GURL& launch_url);
  std::tuple<Browser*, bool /*is_new_browser*/> EnsureBrowser();
  LaunchHandler::RouteTo GetLaunchRouteTo() const;

  Browser* MaybeFindBrowserForLaunch() const;
  Browser* CreateBrowserForLaunch();
  content::WebContents* NavigateBrowser(Browser* browser,
                                        bool is_new_browser,
                                        const GURL& launch_url,
                                        const apps::ShareTarget* share_target);
  void MaybeEnqueueWebLaunchParams(const GURL& launch_url,
                                   bool is_file_handling,
                                   content::WebContents* web_contents);
  void RecordMetrics(const GURL& launch_url,
                     content::WebContents* web_contents);

  Profile& profile_;
  WebAppProvider& provider_;
  const apps::AppLaunchParams& params_;
  const WebApp* web_app_ = nullptr;
};

LaunchProcess::LaunchProcess(Profile& profile,
                             const apps::AppLaunchParams& params)
    : profile_(profile),
      provider_(*WebAppProvider::GetForLocalAppsUnchecked(&profile)),
      params_(params),
      web_app_(provider_.registrar().GetAppById(params.app_id)) {}

content::WebContents* LaunchProcess::Run() {
  if (Browser::GetCreationStatusForProfile(&profile_) !=
          Browser::CreationStatus::kOk ||
      !provider_.registrar().IsInstalled(params_.app_id)) {
    return nullptr;
  }

  // Place new windows on the specified display.
  display::ScopedDisplayForNewWindows scoped_display(params_.display_id);

  const apps::ShareTarget* share_target = MaybeGetShareTarget();
  GURL launch_url;
  bool is_file_handling = false;
  std::tie(launch_url, is_file_handling) = GetLaunchUrl(share_target);

#if defined(OS_CHROMEOS)
  // TODO(crbug.com/1265381): URL Handlers allows web apps to be opened with
  // associated origin URLs. There's no utility function to test whether a URL
  // is in a web app's extended scope at the moment.
  // Because URL Handlers is not implemented for Chrome OS we can perform this
  // DCHECK on the basic scope.
  DCHECK(provider_.registrar().IsUrlInAppScope(launch_url, params_.app_id));
#endif

  // System Web Apps have their own launch code path.
  // TODO(crbug.com/1231886): Don't use a separate code path so that SWAs can
  // maintain feature parity with regular web apps (e.g. launch_handler
  // behaviours).
  content::WebContents* web_contents = MaybeLaunchSystemWebApp(launch_url);
  if (web_contents)
    return web_contents;

  Browser* browser = nullptr;
  bool is_new_browser;
  std::tie(browser, is_new_browser) = EnsureBrowser();

  web_contents =
      NavigateBrowser(browser, is_new_browser, launch_url, share_target);
  if (!web_contents)
    return nullptr;

  MaybeEnqueueWebLaunchParams(launch_url, is_file_handling, web_contents);

  RecordMetrics(launch_url, web_contents);

  return web_contents;
}

const apps::ShareTarget* LaunchProcess::MaybeGetShareTarget() const {
  DCHECK(web_app_);
  bool is_share_intent =
      params_.intent &&
      (params_.intent->action == apps_util::kIntentActionSend ||
       params_.intent->action == apps_util::kIntentActionSendMultiple);
  return is_share_intent && web_app_->share_target().has_value()
             ? &web_app_->share_target().value()
             : nullptr;
}

std::tuple<GURL, bool /*is_file_handling*/> LaunchProcess::GetLaunchUrl(
    const apps::ShareTarget* share_target) const {
  DCHECK(web_app_);
  GURL launch_url;
  bool is_file_handling = false;
  bool is_note_taking_intent =
      params_.intent &&
      params_.intent->action == apps_util::kIntentActionCreateNote;

  if (!params_.override_url.is_empty()) {
    launch_url = params_.override_url;
  } else if (params_.url_handler_launch_url.has_value() &&
             params_.url_handler_launch_url->is_valid()) {
    // Handle url_handlers launch.
    launch_url = params_.url_handler_launch_url.value();
  } else if (absl::optional<GURL> file_handler_url =
                 provider_.os_integration_manager().GetMatchingFileHandlerURL(
                     params_.app_id, params_.launch_files)) {
    // Handle file_handlers launch.
    launch_url = file_handler_url.value();
    is_file_handling = true;
  } else if (absl::optional<GURL> protocol_handler_translated_url =
                 GetProtocolHandlingTranslatedUrl(provider_, params_)) {
    // Handle protocol_handlers launch.
    launch_url = protocol_handler_translated_url.value();
  } else if (share_target) {
    // Handle share_target launch.
    launch_url = share_target->action;
  } else if (is_note_taking_intent &&
             web_app_->note_taking_new_note_url().is_valid()) {
    // Handle Create Note launch.
    launch_url = web_app_->note_taking_new_note_url();
  } else {
    // This is a default launch.
    launch_url = provider_.registrar().GetAppLaunchUrl(params_.app_id);
  }
  DCHECK(launch_url.is_valid());

  return {launch_url, is_file_handling};
}

WindowOpenDisposition LaunchProcess::GetNavigationDisposition(
    bool is_new_browser) const {
  if (is_new_browser) {
    // By opening a new window we've already performed part of a "disposition",
    // the only remaining thing for Navigate() to do is navigate the new window.
    return WindowOpenDisposition::CURRENT_TAB;
    // TODO(crbug.com/1200944): Use NEW_FOREGROUND_TAB instead of CURRENT_TAB.
    // The window has no tabs so it doesn't make sense to open the "current"
    // tab. We use it anyway because it happens to work.
    // If NEW_FOREGROUND_TAB is used the the WindowCanOpenTabs() check fails
    // when `launch_url` is out of scope for web app windows causing it to
    // open another separate browser window. It should be updated to check the
    // extended scope.
  }

  // If launch handler is routing to an existing client, we want to use the
  // existing WebContents rather than opening a new tab.
  if (GetLaunchRouteTo() == LaunchHandler::RouteTo::kExistingClient) {
    return WindowOpenDisposition::CURRENT_TAB;
  }

  // Only CURRENT_TAB and NEW_FOREGROUND_TAB dispositions are supported for web
  // app launches.
  return params_.disposition == WindowOpenDisposition::CURRENT_TAB
             ? WindowOpenDisposition::CURRENT_TAB
             : WindowOpenDisposition::NEW_FOREGROUND_TAB;
}

LaunchHandler::RouteTo LaunchProcess::GetLaunchRouteTo() const {
  DCHECK(web_app_);
  LaunchHandler launch_handler =
      web_app_->launch_handler().value_or(LaunchHandler());
  if (launch_handler.route_to == LaunchHandler::RouteTo::kAuto)
    return LaunchHandler::RouteTo::kNewClient;
  return launch_handler.route_to;
}

content::WebContents* LaunchProcess::MaybeLaunchSystemWebApp(
    const GURL& launch_url) {
  absl::optional<SystemAppType> system_app_type =
      GetSystemWebAppTypeForAppId(&profile_, params_.app_id);
  if (!system_app_type)
    return nullptr;

  Browser* browser =
      LaunchSystemWebAppImpl(&profile_, *system_app_type, launch_url, params_);
  return browser->tab_strip_model()->GetActiveWebContents();
}

std::tuple<Browser*, bool /*is_new_browser*/> LaunchProcess::EnsureBrowser() {
  Browser* browser = MaybeFindBrowserForLaunch();
  bool is_new_browser = false;
  if (browser) {
    browser->window()->Activate();
  } else {
    browser = CreateBrowserForLaunch();
    is_new_browser = true;
  }
  browser->window()->Show();
  return {browser, is_new_browser};
}

Browser* LaunchProcess::MaybeFindBrowserForLaunch() const {
  if (params_.container == apps::mojom::LaunchContainer::kLaunchContainerTab) {
    return chrome::FindTabbedBrowser(
        &profile_, /*match_original_profiles=*/false,
        display::Screen::GetScreen()->GetDisplayForNewWindows().id());
  }

  if (!provider_.registrar().IsTabbedWindowModeEnabled(params_.app_id) &&
      GetLaunchRouteTo() == LaunchHandler::RouteTo::kNewClient) {
    return nullptr;
  }

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == &profile_ &&
        AppBrowserController::IsForWebApp(browser, params_.app_id)) {
      return browser;
    }
  }

  return nullptr;
}

Browser* LaunchProcess::CreateBrowserForLaunch() {
  if (params_.container == apps::mojom::LaunchContainer::kLaunchContainerTab) {
    return Browser::Create(Browser::CreateParams(Browser::TYPE_NORMAL,
                                                 &profile_,
                                                 /*user_gesture=*/true));
  }

  return CreateWebApplicationWindow(&profile_, params_.app_id,
                                    params_.disposition, params_.restore_id);
}

content::WebContents* LaunchProcess::NavigateBrowser(
    Browser* browser,
    bool is_new_browser,
    const GURL& launch_url,
    const apps::ShareTarget* share_target) {
  WindowOpenDisposition navigation_disposition =
      GetNavigationDisposition(is_new_browser);

  if (share_target) {
    NavigateParams nav_params =
        NavigateParamsForShareTarget(browser, *share_target, *params_.intent);
    nav_params.disposition = navigation_disposition;
    return NavigateWebAppUsingParams(params_.app_id, nav_params);
  }

  TabStripModel* const tab_strip = browser->tab_strip_model();
  if (tab_strip->empty() ||
      navigation_disposition != WindowOpenDisposition::CURRENT_TAB) {
    return NavigateWebApplicationWindow(browser, params_.app_id, launch_url,
                                        navigation_disposition);
  }

  content::WebContents* existing_tab = tab_strip->GetActiveWebContents();
  DCHECK(existing_tab);
  const int tab_index = tab_strip->GetIndexOfWebContents(existing_tab);

  existing_tab->OpenURL(content::OpenURLParams(
      launch_url,
      content::Referrer::SanitizeForRequest(
          launch_url,
          content::Referrer(existing_tab->GetURL(),
                            network::mojom::ReferrerPolicy::kDefault)),
      navigation_disposition, ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      /*is_renderer_initiated=*/false));

  content::WebContents* web_contents = tab_strip->GetActiveWebContents();
  tab_strip->ActivateTabAt(tab_index, {TabStripModel::GestureType::kOther});
  SetTabHelperAppId(web_contents, params_.app_id);
  return web_contents;
}

void LaunchProcess::MaybeEnqueueWebLaunchParams(
    const GURL& launch_url,
    bool is_file_handling,
    content::WebContents* web_contents) {
  if (is_file_handling || web_app_->launch_handler().has_value()) {
    web_launch::WebLaunchFilesHelper::EnqueueLaunchParams(
        web_contents, launch_url,
        /*launch_dir=*/{},
        is_file_handling ? params_.launch_files
                         : std::vector<base::FilePath>());
  }
}

void LaunchProcess::RecordMetrics(const GURL& launch_url,
                                  content::WebContents* web_contents) {
  // TODO(crbug.com/1014328): Populate WebApp metrics instead of Extensions.
  if (params_.container == apps::mojom::LaunchContainer::kLaunchContainerTab) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.AppTabLaunchType",
                              extensions::LAUNCH_TYPE_REGULAR, 100);
  } else if (params_.container ==
             apps::mojom::LaunchContainer::kLaunchContainerWindow) {
    RecordAppWindowLaunch(&profile_, params_.app_id);
  }
  UMA_HISTOGRAM_ENUMERATION("Extensions.BookmarkAppLaunchSource",
                            apps::GetAppLaunchSource(params_.launch_source));
  UMA_HISTOGRAM_ENUMERATION("Extensions.BookmarkAppLaunchContainer",
                            params_.container);

  // Record the launch time in the site engagement service. A recent web
  // app launch will provide an engagement boost to the origin.
  site_engagement::SiteEngagementService::Get(&profile_)
      ->SetLastShortcutLaunchTime(web_contents, launch_url);
  provider_.sync_bridge().SetAppLastLaunchTime(params_.app_id,
                                               base::Time::Now());
  // Refresh the app banner added to homescreen event. The user may have
  // cleared their browsing data since installing the app, which removes the
  // event and will potentially permit a banner to be shown for the site.
  RecordAppBanner(web_contents, launch_url);
}

}  // namespace

Browser* CreateWebApplicationWindow(Profile* profile,
                                    const std::string& app_id,
                                    WindowOpenDisposition disposition,
                                    int32_t restore_id,
                                    bool omit_from_session_restore,
                                    bool can_resize,
                                    bool can_maximize,
                                    const gfx::Rect initial_bounds) {
  std::string app_name = GenerateApplicationNameFromAppId(app_id);
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
  browser_params.omit_from_session_restore = omit_from_session_restore;
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
    : profile_(profile),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(profile)) {}

WebAppLaunchManager::~WebAppLaunchManager() = default;

content::WebContents* WebAppLaunchManager::OpenApplication(
    apps::AppLaunchParams&& params) {
  if (GetOpenApplicationCallbackForTesting())
    return GetOpenApplicationCallbackForTesting().Run(std::move(params));

  return LaunchProcess(*profile_, params).Run();
}

void WebAppLaunchManager::LaunchApplication(
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    const absl::optional<GURL>& url_handler_launch_url,
    const absl::optional<GURL>& protocol_handler_launch_url,
    const std::vector<base::FilePath>& launch_files,
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)> callback) {
  if (!provider_)
    return;

  // At most one of these parameters should be non-empty.
  DCHECK_LE(url_handler_launch_url.has_value() +
                protocol_handler_launch_url.has_value() + !launch_files.empty(),
            1);

  apps::mojom::LaunchSource launch_source =
      apps::mojom::LaunchSource::kFromCommandLine;

  if (url_handler_launch_url.has_value())
    launch_source = apps::mojom::LaunchSource::kFromUrlHandler;
  else if (!launch_files.empty())
    launch_source = apps::mojom::LaunchSource::kFromFileManager;

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin) &&
      command_line.HasSwitch(switches::kAppRunOnOsLoginMode)) {
    launch_source = apps::mojom::LaunchSource::kFromOsLogin;
  } else if (protocol_handler_launch_url.has_value()) {
    launch_source = apps::mojom::LaunchSource::kFromProtocolHandler;
  }

  apps::AppLaunchParams params(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, launch_source);
  params.command_line = command_line;
  params.current_directory = current_directory;
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsFileHandlingSettingsGated)) {
    params.launch_files = launch_files;
  } else if (!protocol_handler_launch_url) {
    params.launch_files = apps::GetLaunchFilesFromCommandLine(command_line);
  }
  params.url_handler_launch_url = url_handler_launch_url;
  params.protocol_handler_launch_url = protocol_handler_launch_url;
  params.override_url = GURL(command_line.GetSwitchValueASCII(
      switches::kAppLaunchUrlForShortcutsMenuItem));

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
  GetOpenApplicationCallbackForTesting() = std::move(callback);
}

void WebAppLaunchManager::LaunchWebApplication(
    apps::AppLaunchParams&& params,
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)> callback) {
  apps::mojom::LaunchContainer container;
  Browser* browser = nullptr;
  if (provider_->registrar().IsInstalled(params.app_id)) {
    if (provider_->registrar().GetAppEffectiveDisplayMode(params.app_id) ==
        blink::mojom::DisplayMode::kBrowser) {
      params.container = apps::mojom::LaunchContainer::kLaunchContainerTab;
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    }

    container = params.container;
    const content::WebContents* web_contents =
        OpenApplication(std::move(params));
    if (web_contents)
      browser = chrome::FindBrowserWithWebContents(web_contents);
  } else {
    // Open an empty browser window as the app_id is invalid.
    container = apps::mojom::LaunchContainer::kLaunchContainerNone;
    browser = apps::CreateBrowserWithNewTabPage(profile_);
  }
  std::move(callback).Run(browser, container);
}

void RecordAppWindowLaunch(Profile* profile, const std::string& app_id) {
  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
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
