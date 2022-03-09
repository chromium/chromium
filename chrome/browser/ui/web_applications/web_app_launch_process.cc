// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_process.h"

#include "base/files/file_path.h"
#include "base/memory/values_equivalent.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
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
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_launch_queue.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "extensions/common/constants.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#endif

namespace web_app {

namespace {

absl::optional<GURL> GetProtocolHandlingTranslatedUrl(
    OsIntegrationManager& os_integration_manager,
    const apps::AppLaunchParams& params) {
  if (!params.protocol_handler_launch_url.has_value())
    return absl::nullopt;

  GURL protocol_url(params.protocol_handler_launch_url.value());
  if (!protocol_url.is_valid())
    return absl::nullopt;

  absl::optional<GURL> translated_url =
      os_integration_manager.TranslateProtocolUrl(params.app_id, protocol_url);

  return translated_url;
}

}  // namespace

WebAppLaunchProcess::WebAppLaunchProcess(Profile& profile,
                                         const apps::AppLaunchParams& params)
    : profile_(profile),
      provider_(*WebAppProvider::GetForLocalAppsUnchecked(&profile)),
      params_(params),
      web_app_(provider_.registrar().GetAppById(params.app_id)) {}

content::WebContents* WebAppLaunchProcess::Run() {
  if (Browser::GetCreationStatusForProfile(&profile_) !=
          Browser::CreationStatus::kOk ||
      !provider_.registrar().IsInstalled(params_.app_id)) {
    return nullptr;
  }

  // Place new windows on the specified display.
  display::ScopedDisplayForNewWindows scoped_display(params_.display_id);

  const apps::ShareTarget* share_target = MaybeGetShareTarget();
  auto [launch_url, is_file_handling] = GetLaunchUrl(share_target);

#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/1265381): URL Handlers allows web apps to be opened with
  // associated origin URLs. There's no utility function to test whether a URL
  // is in a web app's extended scope at the moment.
  // Because URL Handlers is not implemented for Chrome OS we can perform this
  // DCHECK on the basic scope.
  DCHECK(provider_.registrar().IsUrlInAppScope(launch_url, params_.app_id) ||
         GetSystemWebAppTypeForAppId(&profile_, params_.app_id) &&
             provider_.system_web_app_manager().GetSystemApp(
                 *GetSystemWebAppTypeForAppId(&profile_, params_.app_id)) &&
             provider_.system_web_app_manager()
                 .GetSystemApp(
                     *GetSystemWebAppTypeForAppId(&profile_, params_.app_id))
                 ->IsUrlInSystemAppScope(launch_url));
#endif

  // System Web Apps have their own launch code path.
  // TODO(crbug.com/1231886): Don't use a separate code path so that SWAs can
  // maintain feature parity with regular web apps (e.g. launch_handler
  // behaviours).
  content::WebContents* web_contents = MaybeLaunchSystemWebApp(launch_url);
  if (web_contents)
    return web_contents;

  auto [browser, is_new_browser] = EnsureBrowser();

  NavigateResult navigate_result =
      MaybeNavigateBrowser(browser, is_new_browser, launch_url, share_target);
  web_contents = navigate_result.web_contents;
  if (!web_contents)
    return nullptr;

  MaybeEnqueueWebLaunchParams(
      launch_url, is_file_handling, web_contents,
      /*started_new_navigation=*/navigate_result.did_navigate);

  RecordMetrics(params_.app_id, params_.container,
                apps::GetAppLaunchSource(params_.launch_source), launch_url,
                web_contents);

  return web_contents;
}

const apps::ShareTarget* WebAppLaunchProcess::MaybeGetShareTarget() const {
  DCHECK(web_app_);
  bool is_share_intent =
      params_.intent && apps_util::IsShareIntent(params_.intent);
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
      params_.intent &&
      params_.intent->action == apps_util::kIntentActionCreateNote;

  if (!params_.override_url.is_empty()) {
    launch_url = params_.override_url;
  } else if (share_target) {
    // Handle share_target launch.
    launch_url = share_target->action;
  } else if (params_.url_handler_launch_url.has_value() &&
             params_.url_handler_launch_url->is_valid()) {
    // Handle url_handlers launch.
    launch_url = params_.url_handler_launch_url.value();
  } else if (absl::optional<GURL> file_handler_url =
                 provider_.os_integration_manager().GetMatchingFileHandlerURL(
                     params_.app_id, params_.launch_files)) {
    // Handle file_handlers launch. When launched from Files app, the user has
    // already selected a file_handler so use that if available.
    if (params_.intent && params_.intent->activity_name) {
      launch_url = GURL(params_.intent->activity_name.value());
    } else {
      launch_url = file_handler_url.value();
    }
    is_file_handling = true;
  } else if (absl::optional<GURL> protocol_handler_translated_url =
                 GetProtocolHandlingTranslatedUrl(
                     provider_.os_integration_manager(), params_)) {
    // Handle protocol_handlers launch.
    launch_url = protocol_handler_translated_url.value();
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

WindowOpenDisposition WebAppLaunchProcess::GetNavigationDisposition(
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

LaunchHandler::RouteTo WebAppLaunchProcess::GetLaunchRouteTo() const {
  DCHECK(web_app_);
  LaunchHandler launch_handler =
      web_app_->launch_handler().value_or(LaunchHandler());
  if (launch_handler.route_to == LaunchHandler::RouteTo::kAuto)
    return LaunchHandler::RouteTo::kNewClient;
  return launch_handler.route_to;
}

LaunchHandler::NavigateExistingClient
WebAppLaunchProcess::GetLaunchNavigateExistingClient() const {
  DCHECK(web_app_);
  return web_app_->launch_handler()
      .value_or(LaunchHandler())
      .navigate_existing_client;
}

content::WebContents* WebAppLaunchProcess::MaybeLaunchSystemWebApp(
    const GURL& launch_url) {
  absl::optional<SystemAppType> system_app_type =
      GetSystemWebAppTypeForAppId(&profile_, params_.app_id);
  if (!system_app_type)
    return nullptr;

  Browser* browser =
      LaunchSystemWebAppImpl(&profile_, *system_app_type, launch_url, params_);
  return browser->tab_strip_model()->GetActiveWebContents();
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

Browser* WebAppLaunchProcess::CreateBrowserForLaunch() {
  if (params_.container == apps::mojom::LaunchContainer::kLaunchContainerTab) {
    return Browser::Create(Browser::CreateParams(Browser::TYPE_NORMAL,
                                                 &profile_,
                                                 /*user_gesture=*/true));
  }

  return CreateWebApplicationWindow(&profile_, params_.app_id,
                                    params_.disposition, params_.restore_id);
}

WebAppLaunchProcess::NavigateResult WebAppLaunchProcess::MaybeNavigateBrowser(
    Browser* browser,
    bool is_new_browser,
    const GURL& launch_url,
    const apps::ShareTarget* share_target) {
  WindowOpenDisposition navigation_disposition =
      GetNavigationDisposition(is_new_browser);

  if (share_target) {
    // TODO(crbug.com/1213776): Expose share target in the LaunchParams and
    // don't navigate if navigate_existing_client: never is in effect.
    NavigateParams nav_params = NavigateParamsForShareTarget(
        browser, *share_target, *params_.intent, params_.launch_files);
    nav_params.disposition = navigation_disposition;
    return {
        .web_contents = NavigateWebAppUsingParams(params_.app_id, nav_params),
        .did_navigate = true};
  }

  TabStripModel* const tab_strip = browser->tab_strip_model();
  if (tab_strip->empty() ||
      navigation_disposition != WindowOpenDisposition::CURRENT_TAB) {
    return {.web_contents = NavigateWebApplicationWindow(
                browser, params_.app_id, launch_url, navigation_disposition),
            .did_navigate = true};
  }

  content::WebContents* existing_tab = tab_strip->GetActiveWebContents();
  DCHECK(existing_tab);
  if (GetLaunchNavigateExistingClient() ==
      LaunchHandler::NavigateExistingClient::kNever) {
    if (base::ValuesEquivalent(WebAppTabHelper::FromWebContents(existing_tab)
                                   ->EnsureLaunchQueue()
                                   .GetPendingLaunchAppId(),
                               &params_.app_id)) {
      // This WebContents is already handling a launch for this app. It may
      // currently be out of scope but the in progress app launch will put it
      // back in scope. The new app launch params can be queued up to fire after
      // the existing app launch completes.
      return {.web_contents = existing_tab, .did_navigate = false};
    }

    if (provider_.registrar().IsUrlInAppScope(
            existing_tab->GetLastCommittedURL(), params_.app_id)) {
      // If the web contents is currently navigating then interrupt it. The
      // current page is now being used for this app launch.
      existing_tab->Stop();
      return {.web_contents = existing_tab, .did_navigate = false};
    }
  }

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
  SetWebContentsActingAsApp(web_contents, params_.app_id);
  return {.web_contents = web_contents, .did_navigate = true};
}

void WebAppLaunchProcess::MaybeEnqueueWebLaunchParams(
    const GURL& launch_url,
    bool is_file_handling,
    content::WebContents* web_contents,
    bool started_new_navigation) {
  if (is_file_handling || web_app_->launch_handler().has_value()) {
    WebAppLaunchParams launch_params;
    launch_params.started_new_navigation = started_new_navigation;
    launch_params.app_id = web_app_->app_id();
    launch_params.target_url = launch_url;
    launch_params.paths =
        is_file_handling ? params_.launch_files : std::vector<base::FilePath>();
    WebAppTabHelper::FromWebContents(web_contents)
        ->EnsureLaunchQueue()
        .Enqueue(std::move(launch_params));
  }
}

}  // namespace web_app
