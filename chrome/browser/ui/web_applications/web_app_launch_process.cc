// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_process.h"

#include "base/debug/crash_logging.h"
#include "base/files/file_path.h"
#include "base/memory/values_equivalent.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/share_target_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_navigation_handle_user_data.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/services/app_service/public/cpp/app_launch_params.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/webapps/browser/launch_queue/launch_queue.h"
#include "extensions/common/constants.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#endif

namespace web_app {

namespace {

// Returns the most recently activated tabbed (TYPE_NORMAL) browser for
// `profile`, filtered by `display_id` when it is not kInvalidDisplayId.
// Browsers scheduled for deletion are excluded.
BrowserWindowInterface* FindTabbedBrowser(Profile* profile,
                                          int64_t display_id) {
  BrowserWindowInterface* match = nullptr;
  ProfileBrowserCollection::GetForProfile(profile)->ForEach(
      [&match, display_id](BrowserWindowInterface* browser) {
        if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
            browser->IsDeleteScheduled()) {
          return true;
        }
        if (display_id != display::kInvalidDisplayId &&
            display::Screen::Get()
                    ->GetDisplayNearestWindow(
                        browser->GetWindow()->GetNativeWindow())
                    .id() != display_id) {
          return true;
        }
        match = browser;
        return false;  // stop iterating
      },
      BrowserCollection::Order::kActivation);
  return match;
}

std::optional<GURL> GetProtocolHandlingTranslatedUrl(
    OsIntegrationManager& os_integration_manager,
    const apps::AppLaunchParams& params) {
  if (!params.protocol_handler_launch_url.has_value()) {
    return std::nullopt;
  }

  GURL protocol_url(params.protocol_handler_launch_url.value());
  if (!protocol_url.is_valid()) {
    return std::nullopt;
  }

  std::optional<GURL> translated_url =
      os_integration_manager.TranslateProtocolUrl(params.app_id, protocol_url);

  return translated_url;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange
enum class LaunchUrlInScopeResult {
  kInScope = 0,
  kNotInScope = 1,
  kInScopeMissingTrainingSlash = 2,
  kMaxValue = kInScopeMissingTrainingSlash
};
// LINT.ThenChange(tools/metrics/histograms/metadata/webapps/enums.xml)

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
      !registrar_->AppMatches(params_->app_id,
                              WebAppFilter::IsAppSurfaceableToUser())) {
    return nullptr;
  }

  // Place new windows on the specified display.
  std::optional<display::ScopedDisplayForNewWindows> scoped_display;
  if (params_->display_id != display::kInvalidDisplayId) {
    scoped_display.emplace(params_->display_id);
  }

  const apps::ShareTarget* share_target = MaybeGetShareTarget();
  auto [launch_url, is_file_handling] = GetLaunchUrl(share_target);

  auto web_app_scope = registrar_->GetEffectiveScope(params_->app_id);
  CHECK(web_app_scope);
  LaunchUrlInScopeResult in_scope_result =
      web_app_scope->GetScopeScore(launch_url) > 0
          ? LaunchUrlInScopeResult::kInScope
          : LaunchUrlInScopeResult::kNotInScope;
#if BUILDFLAG(IS_CHROMEOS)
  bool is_url_in_system_web_app_scope =
      ash::GetSystemWebAppTypeForAppId(&*profile_, params_->app_id) &&
      ash::SystemWebAppManager::Get(&*profile_)
          ->GetSystemApp(
              *ash::GetSystemWebAppTypeForAppId(&*profile_, params_->app_id)) &&
      ash::SystemWebAppManager::Get(&*profile_)
          ->GetSystemApp(
              *ash::GetSystemWebAppTypeForAppId(&*profile_, params_->app_id))
          ->IsUrlInSystemAppScope(launch_url);
  if (is_url_in_system_web_app_scope) {
    in_scope_result = LaunchUrlInScopeResult::kInScope;
  }
#endif

  if (in_scope_result == LaunchUrlInScopeResult::kNotInScope) {
    if (web_app_scope->scope().spec().back() == '/') {
      // Special case to allow urls at the root of the scope to match even if
      // they are missing the trailing slash. This allows
      // http://example.com/scope/ to contain http://example.com/scope?query
      // even though it doesn't pass a StartsWith() check.
      GURL::Replacements replacements;
      replacements.ClearQuery();
      replacements.ClearRef();
      auto launch_url_without_params_and_query =
          launch_url.ReplaceComponents(replacements).spec();
      launch_url_without_params_and_query.push_back('/');
      if (web_app_scope->scope().spec() ==
          launch_url_without_params_and_query) {
        in_scope_result = LaunchUrlInScopeResult::kInScopeMissingTrainingSlash;
      }
    }
  }
  base::UmaHistogramEnumeration("WebApp.LaunchUrlIsInScope", in_scope_result);

#if BUILDFLAG(IS_CHROMEOS)
  // System Web Apps have their own launch code path.
  std::optional<ash::SystemWebAppType> system_app_type =
      ash::GetSystemWebAppTypeForAppId(&profile_.get(), params_->app_id);
  if (system_app_type) {
    ash::BrowserDelegate* browser = LaunchSystemWebAppImpl(
        &profile_.get(), *system_app_type, launch_url, *params_);
    return browser ? browser->GetActiveWebContents() : nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // During install, shortcut urls are required to be in-scope. But some users &
  // admins create custom shortcuts to launch weirdly configured apps with
  // out-of-scope but same-origin urls. So specifically also allow same-origin
  // urls to be launched here, even if they are not in scope.
  if (in_scope_result == LaunchUrlInScopeResult::kNotInScope &&
      !url::IsSameOriginWith(launch_url, web_app_scope->scope())) {
    launch_url = registrar_->GetAppStartUrl(params_->app_id);
    // If this was a file handler launch, then it would be unexpected that a
    // file handle exists if we are resetting the URL. Thus, reset to only a
    // regular launch.
    is_file_handling = false;
  }

  NavigateResult navigate_result =
      MaybeNavigateBrowser(launch_url, share_target);
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
    return WindowOpenDisposition::NEW_FOREGROUND_TAB;
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
  if (launch_handler.parsed_client_mode() == LaunchHandler::ClientMode::kAuto) {
    return LaunchHandler::ClientMode::kNavigateNew;
  }
  return launch_handler.parsed_client_mode();
}

BrowserWindowInterface* WebAppLaunchProcess::MaybeFindBrowserForLaunch() const {
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
      display_id = display::Screen::Get()->GetDisplayForNewWindows().id();
    }
#endif
    return FindTabbedBrowser(&profile_.get(), display_id);
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
    const GURL& launch_url,
    const apps::ShareTarget* share_target) {
  BrowserWindowInterface* browser = MaybeFindBrowserForLaunch();
  bool is_new_browser = false;
  if (browser) {
    browser->GetWindow()->Activate();
  } else {
    browser = CreateBrowserForLaunch();
    is_new_browser = true;
  }
  browser->GetWindow()->Show();

  WindowOpenDisposition navigation_disposition =
      GetNavigationDisposition(is_new_browser);
  content::WebContents* existing_tab =
      browser->GetFeatures().tab_strip_model()->GetActiveWebContents();
  bool open_in_new_window =
      !existing_tab ||
      navigation_disposition != WindowOpenDisposition::CURRENT_TAB;
  // In the case of prevent-close, we do not navigate but instead focus the
  // existing window.
  if (!open_in_new_window &&
      (GetLaunchHandler().NeverNavigateExistingClients() ||
       registrar_->IsPreventCloseEnabled(params_->app_id))) {
    auto* tab_helper = WebAppTabHelper::FromWebContents(existing_tab);
    if (tab_helper->pending_launch_app_id() == params_->app_id) {
      // This WebContents is already handling a launch for this app. It may
      // currently be out of scope but the in progress app launch will put it
      // back in scope. The new app launch params are added on here, so that
      // it can be tied to the current navigation.
      if (auto holder = tab_helper->pending_launch_params_holder()) {
        webapps::LaunchParams launch_params;
        launch_params.set_app_id(web_app_->app_id());
        launch_params.set_target_url(launch_url);
        launch_params.set_paths(params_->launch_files);
        holder->SetLaunchParams(std::move(launch_params));
      }
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

  const GURL& url_to_navigate =
      AppBrowserController::IsIsolatedWebApp(browser) &&
              launch_url.SchemeIsHTTPOrHTTPS() && open_in_new_window
          ? AppBrowserController::From(browser)->GetAppStartUrl()
          : launch_url;

  NavigateParams nav_params =
      share_target
          ? NavigateParamsForShareTarget(
                browser, *share_target, *params_->intent, params_->launch_files)
          : NavigateParams(browser, url_to_navigate,
                           ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  nav_params.disposition = navigation_disposition;
  if (!open_in_new_window) {
    nav_params.referrer = content::Referrer::SanitizeForRequest(
        launch_url,
        content::Referrer(existing_tab->GetURL(),
                          network::mojom::ReferrerPolicy::kDefault));
  }

  if (!nav_params.web_app_navigation_data) {
    nav_params.web_app_navigation_data.emplace();
  }

  webapps::LaunchParams launch_params;
  launch_params.set_app_id(web_app_->app_id());
  launch_params.set_target_url(launch_url);
  launch_params.set_paths(params_->launch_files);
  nav_params.web_app_navigation_data->SetLaunchParams(std::move(launch_params));

  return {.web_contents = NavigateWebAppUsingParams(nav_params),
          .did_navigate = true};
}

void WebAppLaunchProcess::MaybeEnqueueWebLaunchParams(
    const GURL& launch_url,
    bool is_file_handling,
    content::WebContents* web_contents,
    bool started_new_navigation) {
  if (started_new_navigation) {
    // If we started a new navigation, the launch parameters have already been
    // attached to the NavigateParams and will be committed once
    // DidFinishNavigation() is called.
    return;
  }

  webapps::LaunchParams launch_params;
  launch_params.set_started_new_navigation(false);
  launch_params.set_app_id(web_app_->app_id());
  launch_params.set_target_url(launch_url);
  launch_params.set_paths(is_file_handling ? params_->launch_files
                                           : std::vector<base::FilePath>());

  WebAppLaunchNavigationHandleUserData::DispatchLaunchParams(
      web_contents, std::move(launch_params));
}

}  // namespace web_app
