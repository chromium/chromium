// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_process.h"

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/values_equivalent.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/share_target_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_launch_queue.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "extensions/common/constants.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#endif

namespace web_app {

namespace {

std::optional<GURL> GetProtocolHandlingTranslatedUrl(
    OsIntegrationManager& os_integration_manager,
    const apps::AppLaunchParams& params) {
  if (!params.protocol_handler_launch_url.has_value())
    return std::nullopt;

  GURL protocol_url(params.protocol_handler_launch_url.value());
  if (!protocol_url.is_valid())
    return std::nullopt;

  std::optional<GURL> translated_url =
      os_integration_manager.TranslateProtocolUrl(params.app_id, protocol_url);

  return translated_url;
}

}  // namespace

// static
content::WebContents* WebAppLaunchProcess::CreateAndRun(
    Profile& profile,
    WebAppRegistrar& registrar,
    OsIntegrationManager& os_integration_manager,
    const apps::AppLaunchParams& params) {
  return WebAppLaunchProcess(profile, registrar, os_integration_manager, params)
      .Run();
}

// static
void WebAppLaunchProcess::SetOpenApplicationCallbackForTesting(
    OpenApplicationCallback callback) {
  GetOpenApplicationCallbackForTesting() = std::move(callback);  // IN-TEST
}

// static
WebAppLaunchProcess::OpenApplicationCallback&
WebAppLaunchProcess::GetOpenApplicationCallbackForTesting() {
  static base::NoDestructor<WebAppLaunchProcess::OpenApplicationCallback>
      callback;
  return *callback;
}

WebAppLaunchProcess::WebAppLaunchProcess(
    Profile& profile,
    WebAppRegistrar& registrar,
    OsIntegrationManager& os_integration_manager,
    const apps::AppLaunchParams& params)
    : profile_(profile),
      registrar_(registrar),
      os_integration_manager_(os_integration_manager),
      params_(params),
      web_app_(registrar_->GetAppById(params.app_id)) {}

content::WebContents* WebAppLaunchProcess::Run() {
  if (Browser::GetCreationStatusForProfile(&profile_.get()) !=
          Browser::CreationStatus::kOk ||
      !registrar_->IsInstalled(params_->app_id)) {
    return nullptr;
  }

  // Place new windows on the specified display.
  std::optional<display::ScopedDisplayForNewWindows> scoped_display;
  if (params_->display_id != display::kInvalidDisplayId) {
    scoped_display.emplace(params_->display_id);
  }

  const apps::ShareTarget* share_target = MaybeGetShareTarget();
  auto [launch_url, is_file_handling] = GetLaunchUrl(share_target);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_url_in_system_web_app_scope =
      ash::GetSystemWebAppTypeForAppId(&*profile_, params_->app_id) &&
      ash::SystemWebAppManager::Get(&*profile_)
          ->GetSystemApp(
              *ash::GetSystemWebAppTypeForAppId(&*profile_, params_->app_id)) &&
      ash::SystemWebAppManager::Get(&*profile_)
          ->GetSystemApp(
              *ash::GetSystemWebAppTypeForAppId(&*profile_, params_->app_id))
          ->IsUrlInSystemAppScope(launch_url);

  // TODO(crbug.com/40071115): Figure out why this is getting hit.
  if (!registrar_->IsUrlInAppExtendedScope(launch_url, params_->app_id) &&
      !is_url_in_system_web_app_scope) {
    SCOPED_CRASH_KEY_STRING256("crbug1477991", "launch_url", launch_url.spec());
    SCOPED_CRASH_KEY_STRING256("crbug1477991", "app_scope",
                               web_app_->scope().spec());
    base::debug::DumpWithoutCrashing();
    DCHECK(false) << "Url " << launch_url.spec() << " not in scope for app "
                  << params_->app_id;
  }
#else
  // TODO(crbug.com/338406726): Figure out why this is failing. If no longer
  // failing, then we can reject the launch by returning a nullptr.
  if (!registrar_->IsUrlInAppExtendedScope(launch_url, params_->app_id)) {
    SCOPED_CRASH_KEY_STRING256("crbug338406726", "launch_url",
                               launch_url.spec());
    SCOPED_CRASH_KEY_STRING256("crbug338406726", "app_scope",
                               web_app_->scope().spec());
    base::debug::DumpWithoutCrashing();
    DCHECK(false) << "Url " << launch_url.spec() << " not in scope for app "
                  << params_->app_id;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // System Web Apps have their own launch code path.
  std::optional<ash::SystemWebAppType> system_app_type =
      ash::GetSystemWebAppTypeForAppId(&profile_.get(), params_->app_id);
  if (system_app_type) {
    Browser* browser = LaunchSystemWebAppImpl(&profile_.get(), *system_app_type,
                                              launch_url, *params_);

    return browser ? browser->tab_strip_model()->GetActiveWebContents()
                   : nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto [browser, is_new_browser] = EnsureBrowser();

  NavigateResult navigate_result =
      MaybeNavigateBrowser(browser, is_new_browser, launch_url, share_target);
  content::WebContents* web_contents = navigate_result.web_contents;
  if (!web_contents) {
    return nullptr;
  }

  MaybeEnqueueWebLaunchParams(
      launch_url, is_file_handling, web_contents,
      /*started_new_navigation=*/navigate_result.did_navigate);

  UpdateLaunchStats(web_contents, params_->app_id, launch_url);
  RecordLaunchMetrics(params_->app_id, params_->container,
                      params_->launch_source, launch_url, web_contents);

  return web_contents;
}

const apps::ShareTarget* WebAppLaunchProcess::MaybeGetShareTarget() const {
  DCHECK(web_app_);
  bool is_share_intent = params_->intent && params_->intent->IsShareIntent();
  return is_share_intent && web_app_->share_target().has_value()
             ? &web_app_->share_target().value()
             : nullptr;
}

std::tuple<GURL, bool /*is_file_handling*/> WebAppLaunchProcess::GetLaunchUrl(
    const apps::ShareTarget* share_target) const {
  DCHECK(web_app_);
  GURL launch_url;
  bool is_file_handling = false;
  bool is_note_taking_intent =
      params_->intent &&
      params_->intent->action == apps_util::kIntentActionCreateNote;

  if (share_target) {
    // Handle share_target launch.
    launch_url = share_target->action;
  } else if (!params_->override_url.is_empty()) {
    launch_url = params_->override_url;
    is_file_handling = !params_->launch_files.empty();
  } else if (params_->url_handler_launch_url.has_value() &&
             params_->url_handler_launch_url->is_valid()) {
    // Handle url_handlers launch.
    launch_url = params_->url_handler_launch_url.value();
  } else if (std::optional<GURL> protocol_handler_translated_url =
                 GetProtocolHandlingTranslatedUrl(*os_integration_manager_,
                                                  *params_)) {
    // Handle protocol_handlers launch.
    launch_url = protocol_handler_translated_url.value();
  } else if (is_note_taking_intent &&
             web_app_->note_taking_new_note_url().is_valid()) {
    // Handle Create Note launch.
    launch_url = web_app_->note_taking_new_note_url();
  } else {
    // This is a default launch.
    launch_url = registrar_->GetAppLaunchUrl(params_->app_id);
  }
  DCHECK(launch_url.is_valid());

  return {launch_url, is_file_handling};
}

WindowOpenDisposition WebAppLaunchProcess::GetNavigationDisposition(
    bool is_new_browser) const {
  // For prevent-close, we always want to focus the existing window
  if (registrar_->IsPreventCloseEnabled(params_->app_id)) {
    return WindowOpenDisposition::CURRENT_TAB;
  }

  if (registrar_->IsTabbedWindowModeEnabled(params_->app_id)) {
    return WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }

  if (is_new_browser) {
    // By opening a new window we've already performed part of a "disposition",
    // the only remaining thing for Navigate() to do is navigate the new window.
    return WindowOpenDisposition::CURRENT_TAB;
    // TODO(crbug.com/40762104): Use NEW_FOREGROUND_TAB instead of CURRENT_TAB.
    // The window has no tabs so it doesn't make sense to open the "current"
    // tab. We use it anyway because it happens to work.
    // If NEW_FOREGROUND_TAB is used the the WindowCanOpenTabs() check fails
    // when `launch_url` is out of scope for web app windows causing it to
    // open another separate browser window. It should be updated to check the
    // extended scope.
  }

  // If launch handler is routing to an existing client, we want to use the
  // existing WebContents rather than opening a new tab.
  if (GetLaunchHandler().TargetsExistingClients()) {
    return WindowOpenDisposition::CURRENT_TAB;
  }

  // Only CURRENT_TAB and NEW_FOREGROUND_TAB dispositions are supported for web
  // app launches.
  return params_->disposition == WindowOpenDisposition::CURRENT_TAB
             ? WindowOpenDisposition::CURRENT_TAB
             : WindowOpenDisposition::NEW_FOREGROUND_TAB;
}

LaunchHandler WebAppLaunchProcess::GetLaunchHandler() const {
  DCHECK(web_app_);
  return web_app_->launch_handler().value_or(LaunchHandler());
}

LaunchHandler::ClientMode WebAppLaunchProcess::GetLaunchClientMode() const {
  LaunchHandler launch_handler = GetLaunchHandler();
  if (launch_handler.client_mode == LaunchHandler::ClientMode::kAuto)
    return LaunchHandler::ClientMode::kNavigateNew;
  return launch_handler.client_mode;
}

std::tuple<Browser*, bool /*is_new_browser*/>
WebAppLaunchProcess::EnsureBrowser() {
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

Browser* WebAppLaunchProcess::MaybeFindBrowserForLaunch() const {
  if (params_->container == apps::LaunchContainer::kLaunchContainerTab) {
    // In general, when opening a web application in a tab, we want to open the
    // application in a tab in the most recently used browser window.
    // Chrome OS however prefers opening new tabs in windows on a specific
    // display, specifically the one returned by GetDisplayForNewWindows(), even
    // if no browser windows are currently open on that display (except when
    // we're specifically opening the app in the current tab, rather than a new
    // tab).
    int64_t display_id = display::kInvalidDisplayId;
#if BUILDFLAG(IS_CHROMEOS)
    if (params_->disposition != WindowOpenDisposition::CURRENT_TAB) {
      display_id = display::Screen::GetScreen()->GetDisplayForNewWindows().id();
    }
#endif
    return chrome::FindTabbedBrowser(
        &profile_.get(), /*match_original_profiles=*/false, display_id,
        /*ignore_closing_browsers=*/true);
  }

  if (params_->disposition == WindowOpenDisposition::NEW_WINDOW) {
    return nullptr;
  }

  // In the case of prevent-close, we do not want to create a new browser, but
  // instead continue to find the existing browser window.
  if (!registrar_->IsTabbedWindowModeEnabled(params_->app_id) &&
      GetLaunchClientMode() == LaunchHandler::ClientMode::kNavigateNew &&
      !registrar_->IsPreventCloseEnabled(params_->app_id)) {
    return nullptr;
  }

  return AppBrowserController::FindForWebApp(*profile_, params_->app_id);
}

Browser* WebAppLaunchProcess::CreateBrowserForLaunch() {
  if (params_->container == apps::LaunchContainer::kLaunchContainerTab) {
    return Browser::Create(Browser::CreateParams(Browser::TYPE_NORMAL,
                                                 &profile_.get(),
                                                 /*user_gesture=*/true));
  }

  Browser::CreateParams browser_params = web_app::CreateParamsForApp(
      params_->app_id,
      /*is_popup=*/params_->disposition == WindowOpenDisposition::NEW_POPUP,
      /*trusted_source=*/true, /*window_bounds=*/gfx::Rect(),
      /*profile=*/&profile_.get(),
      /*user_gesture*/ true);
#if BUILDFLAG(IS_CHROMEOS)
  browser_params.restore_id = params_->restore_id;
#endif
  return CreateWebAppWindowMaybeWithHomeTab(params_->app_id, browser_params);
}

WebAppLaunchProcess::NavigateResult WebAppLaunchProcess::MaybeNavigateBrowser(
    Browser* browser,
    bool is_new_browser,
    const GURL& launch_url,
    const apps::ShareTarget* share_target) {
  WindowOpenDisposition navigation_disposition =
      GetNavigationDisposition(is_new_browser);

  if (share_target) {
    // TODO(crbug.com/40768956): Expose share target in the LaunchParams and
    // don't navigate if navigate_existing_client: never is in effect.
    NavigateParams nav_params = NavigateParamsForShareTarget(
        browser, *share_target, *params_->intent, params_->launch_files);
    nav_params.disposition = navigation_disposition;
    return {
        .web_contents = NavigateWebAppUsingParams(params_->app_id, nav_params),
        .did_navigate = true};
  }

  TabStripModel* const tab_strip = browser->tab_strip_model();
  if (tab_strip->empty() ||
      navigation_disposition != WindowOpenDisposition::CURRENT_TAB) {
    NavigateParams nav_params(browser, launch_url,
                              ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    nav_params.disposition = navigation_disposition;
    return {
        .web_contents = NavigateWebAppUsingParams(params_->app_id, nav_params),
        .did_navigate = true};
  }

  content::WebContents* existing_tab = tab_strip->GetActiveWebContents();
  DCHECK(existing_tab);
  // In the case of prevent-close, we do not navigate but instead focus the
  // existing window
  if (GetLaunchHandler().NeverNavigateExistingClients() ||
      registrar_->IsPreventCloseEnabled(params_->app_id)) {
    if (base::ValuesEquivalent(WebAppTabHelper::FromWebContents(existing_tab)
                                   ->EnsureLaunchQueue()
                                   .GetPendingLaunchAppId(),
                               &params_->app_id)) {
      // This WebContents is already handling a launch for this app. It may
      // currently be out of scope but the in progress app launch will put it
      // back in scope. The new app launch params can be queued up to fire after
      // the existing app launch completes.
      return {.web_contents = existing_tab, .did_navigate = false};
    }

    if (registrar_->IsUrlInAppExtendedScope(existing_tab->GetLastCommittedURL(),
                                            params_->app_id)) {
      // If the web contents is currently navigating then interrupt it. The
      // current page is now being used for this app launch.
      existing_tab->Stop();
      return {.web_contents = existing_tab, .did_navigate = false};
    }
  }

  const int tab_index = tab_strip->GetIndexOfWebContents(existing_tab);

  existing_tab->OpenURL(
      content::OpenURLParams(
          launch_url,
          content::Referrer::SanitizeForRequest(
              launch_url,
              content::Referrer(existing_tab->GetURL(),
                                network::mojom::ReferrerPolicy::kDefault)),
          navigation_disposition, ui::PAGE_TRANSITION_AUTO_BOOKMARK,
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});

  content::WebContents* web_contents = tab_strip->GetActiveWebContents();
  tab_strip->ActivateTabAt(
      tab_index, TabStripUserGestureDetails(
                     TabStripUserGestureDetails::GestureType::kOther));
  return {.web_contents = web_contents, .did_navigate = true};
}

void WebAppLaunchProcess::MaybeEnqueueWebLaunchParams(
    const GURL& launch_url,
    bool is_file_handling,
    content::WebContents* web_contents,
    bool started_new_navigation) {
  WebAppLaunchParams launch_params;
  launch_params.started_new_navigation = started_new_navigation;
  launch_params.app_id = web_app_->app_id();
  launch_params.target_url = launch_url;
  launch_params.paths =
      is_file_handling ? params_->launch_files : std::vector<base::FilePath>();
  WebAppTabHelper::FromWebContents(web_contents)
      ->EnsureLaunchQueue()
      .Enqueue(std::move(launch_params));
}

}  // namespace web_app
