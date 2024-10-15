// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/navigation_capturing_redirection_throttle.h"

#include <memory>
#include <optional>

#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/navigation_capturing_information_forwarder.h"
#include "chrome/browser/web_applications/navigation_capturing_navigation_handle_user_data.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

// TODO(crbug.com/371237535): Move to TabInterface once there is support for
// getting the browser interface for web contents that are in an app window.
// For all use-cases where a reparenting to an app window happens, launch params
// need to be enqueued so as to mimic the pre redirection behavior. See
// https://bit.ly/pwa-navigation-capturing?tab=t.0#bookmark=id.60x2trlfg6iq for
// more information.
void ReparentToAppBrowserEnqueueLaunchParams(
    content::WebContents* old_web_contents,
    const webapps::AppId& app_id,
    const GURL& target_url) {
  Browser* main_browser = chrome::FindBrowserWithTab(old_web_contents);
  Browser* target_browser = CreateWebAppWindowMaybeWithHomeTab(
      app_id, CreateParamsForApp(
                  app_id, /*is_popup=*/false, /*trusted_source=*/true,
                  gfx::Rect(), main_browser->profile(), /*user_gesture=*/true));
  CHECK(target_browser->app_controller());
  ReparentWebContentsIntoBrowserImpl(
      main_browser, old_web_contents, target_browser,
      target_browser->app_controller()->IsUrlInHomeTabScope(target_url));
  CHECK(old_web_contents);
  RecordLaunchMetrics(app_id, apps::LaunchContainer::kLaunchContainerWindow,
                      apps::LaunchSource::kFromNavigationCapturing, target_url,
                      old_web_contents);
  EnqueueLaunchParams(old_web_contents, app_id, target_url,
                      /*wait_for_navigation_to_complete=*/true);
}

// TODO(crbug.com/371237535): Move to TabInterface once there is support for
// getting the browser interface for web contents that are in an app window.
void ReparentWebContentsToTabbedBrowser(content::WebContents* old_web_contents,
                                        WindowOpenDisposition disposition) {
  Browser* source_browser = chrome::FindBrowserWithTab(old_web_contents);
  Browser* existing_browser_window = chrome::FindTabbedBrowser(
      source_browser->profile(), /*match_original_profiles=*/false);

  // Create a new browser window if the navigation was triggered via a
  // shift-click, or if there are no open tabbed browser windows at the moment.
  Browser* target_browser_window =
      (disposition == WindowOpenDisposition::NEW_WINDOW || !source_browser)
          ? Browser::Create(Browser::CreateParams(source_browser->profile(),
                                                  /*user_gesture=*/true))
          : existing_browser_window;

  ReparentWebContentsIntoBrowserImpl(source_browser, old_web_contents,
                                     target_browser_window);
}

LaunchHandler::ClientMode GetLaunchHandlerMode(WebAppRegistrar& registrar,
                                               const webapps::AppId& app_id) {
  LaunchHandler::ClientMode client_mode = registrar.GetAppById(app_id)
                                              ->launch_handler()
                                              .value_or(LaunchHandler())
                                              .client_mode;
  if (client_mode == LaunchHandler::ClientMode::kAuto) {
    return LaunchHandler::ClientMode::kNavigateNew;
  }
  return client_mode;
}

// TODO(msiem): Support use-case where NavigateExisting also opens a new app
// window.
bool ShouldAlwaysOpenNewAppWindow(WebAppRegistrar& registrar,
                                  const webapps::AppId& intermediary_app_id,
                                  const webapps::AppId& target_app_id) {
  return GetLaunchHandlerMode(registrar, intermediary_app_id) ==
             LaunchHandler::ClientMode::kNavigateNew &&
         GetLaunchHandlerMode(registrar, target_app_id) ==
             LaunchHandler::ClientMode::kNavigateNew;
}

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
NavigationCapturingRedirectionThrottle::MaybeCreate(
    content::NavigationHandle* handle) {
  if (!apps::features::IsNavigationCapturingReimplEnabled()) {
    return nullptr;
  }
  return base::WrapUnique(new NavigationCapturingRedirectionThrottle(handle));
}

NavigationCapturingRedirectionThrottle::
    ~NavigationCapturingRedirectionThrottle() = default;

const char* NavigationCapturingRedirectionThrottle::GetNameForLogging() {
  return "NavigationCapturingWebAppRedirectThrottle";
}

ThrottleCheckResult
NavigationCapturingRedirectionThrottle::WillProcessResponse() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return HandleRequest();
}

ThrottleCheckResult NavigationCapturingRedirectionThrottle::HandleRequest() {
  // See https://bit.ly/pwa-navigation-capturing and
  // https://bit.ly/pwa-navigation-handling-dd for more context.
  // Exit early if:
  // 1. If there were no redirects, then the only url in the redirect chain
  // should be the last url to go to.
  // 2. This is not a server side redirect.
  // 3. The navigation was started from the context menu.
  if (navigation_handle()->GetRedirectChain().size() == 1 ||
      !navigation_handle()->WasServerRedirect() ||
      (navigation_handle()->WasStartedFromContextMenu())) {
    return content::NavigationThrottle::PROCEED;
  }
  const GURL& final_url = navigation_handle()->GetURL();
  if (!final_url.is_valid()) {
    return content::NavigationThrottle::PROCEED;
  }

  // Only http-style schemes are allowed.
  if (!final_url.SchemeIsHTTPOrHTTPS()) {
    return content::NavigationThrottle::PROCEED;
  }

  NavigationCapturingNavigationHandleUserData* handle_user_data =
      NavigationCapturingNavigationHandleUserData::GetForNavigationHandle(
          *navigation_handle());
  if (!handle_user_data) {
    return content::NavigationThrottle::PROCEED;
  }

  NavigationCapturingRedirectionInfo redirection_info =
      handle_user_data->redirection_info();
  WindowOpenDisposition link_click_disposition = redirection_info.disposition;
  NavigationHandlingInitialResult initial_nav_handling_result =
      redirection_info.initial_nav_handling_result;
  std::optional<webapps::AppId> source_app_id =
      redirection_info.app_id_initial_browser;
  std::optional<webapps::AppId> navigation_handling_first_stage_app =
      redirection_info.first_navigation_app_id;

  // Do not handle redirections for navigations that create an auxiliary
  // browsing context, or if the app window that opened is not a part of the
  // navigation handling flow.
  if (initial_nav_handling_result ==
          NavigationHandlingInitialResult::kNotHandledByNavigationHandling ||
      initial_nav_handling_result ==
          NavigationHandlingInitialResult::kAppWindowAuxContext) {
    return content::NavigationThrottle::PROCEED;
  }

  content::WebContents* const web_contents_for_navigation =
      navigation_handle()->GetWebContents();

  WebAppProvider* provider =
      WebAppProvider::GetForWebContents(web_contents_for_navigation);
  WebAppRegistrar& registrar = provider->registrar_unsafe();
  std::optional<webapps::AppId> target_app_id =
      registrar.FindAppThatCapturesLinksInScope(final_url);

  // "Same first navigation state" case:
  // First, we can exit early if the first navigation app id matches the target
  // app id (which includes if they are both std::nullopt), as this means we
  // already did the 'correct' navigation capturing behavior on the first
  // navigation.
  if (navigation_handling_first_stage_app == target_app_id) {
    return content::NavigationThrottle::PROCEED;
  }

  // After this point:
  // - The browsing context is a top-level browsing context.
  // - The initial navigation capturing app_id does not match the final
  // navigation
  //   captured app_id (and either can be std::nullopt, but not both).
  // - Navigation is only triggered as part of left, middle or shift clicks.

  bool is_source_app_matching_final_target =
      target_app_id && source_app_id && target_app_id == source_app_id;

  // First, handle the case where a new app window was force-created for an app
  // for user modified clicks. This refers to the use-cases here:
  // https://bit.ly/pwa-navigation-capturing?tab=t.0#bookmark=id.ugh0e993wsl8,
  // where a new app container is made. Corrections handled here:
  // - If the final url IS NOT in scope of any app, then reparent this new
  // window into a tabbed browser window based on the type of the user modified
  // click.
  // - If the final url IS in scope of an app, then create a new app window and
  // reparent this web contents into the new app window, provided the source and
  // new app ids do not match.
  bool is_user_modified =
      (link_click_disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) ||
      (link_click_disposition == WindowOpenDisposition::NEW_WINDOW);
  if (initial_nav_handling_result ==
      NavigationHandlingInitialResult::kAppWindowForcedNewContext) {
    CHECK(redirection_info.app_id_initial_browser.has_value());
    // Note:
    // - kAppWindowForcedNewContext implies we started from an app window, and
    // the intermediary container must be an app.
    CHECK(chrome::FindBrowserWithTab(web_contents_for_navigation)
              ->app_controller());
    if (target_app_id.has_value() && !is_source_app_matching_final_target) {
      ReparentToAppBrowserEnqueueLaunchParams(web_contents_for_navigation,
                                              *target_app_id, final_url);
      return content::NavigationThrottle::PROCEED;
    } else {
      ReparentWebContentsToTabbedBrowser(web_contents_for_navigation,
                                         link_click_disposition);
      return content::NavigationThrottle::PROCEED;
    }
  }

  // Handle the last user-modified correction, where a user-modified click from
  // an app went to the browser, but needs to be reparented back into an app.
  // See
  // https://bit.ly/pwa-navigation-capturing?tab=t.0#bookmark=id.ugh0e993wsl8
  // for more information.
  if (initial_nav_handling_result ==
          NavigationHandlingInitialResult::kBrowserTab &&
      is_user_modified && target_app_id.has_value() &&
      source_app_id.has_value()) {
    if ((link_click_disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB &&
         is_source_app_matching_final_target) ||
        (link_click_disposition == WindowOpenDisposition::NEW_WINDOW)) {
      // As per the UX direction in the doc, NEW_BACKGROUND_TAB only creates a
      // new app window for an app when coming from the same app window.
      // Otherwise, only NEW_WINDOW can create a new app window when coming from
      // an app.
      ReparentToAppBrowserEnqueueLaunchParams(web_contents_for_navigation,
                                              *target_app_id, final_url);
      return content::NavigationThrottle::PROCEED;
    }
  }

  // All other user-modified cases should do the default thing and navigate the
  // existing container.
  if (is_user_modified) {
    return content::NavigationThrottle::PROCEED;
  }

  // After this point:
  // - The navigation is non-user-modified.
  // - This is a top-level browsing context.
  // - The first navigation app_id doesn't match the target app_id (as per "Same
  //   first navigation state" case above).
  // Handle all cases where the initial navigation was captured, and that now
  // needs to be corrected. See the table at
  // bit.ly/pwa-navigation-handling-dd?tab=t.0#bookmark=id.hnvzj4iwiviz

  // TODO(msiem): Support use-case where NavigateExisting also opens a new app
  // window.
  // Handle the use-case where the first result was navigation captured
  // into an app window and both apps had NavigateNew as launch handlers,
  // triggered via a non user modified.
  if (initial_nav_handling_result ==
          NavigationHandlingInitialResult::kAppWindowNavigationCaptured &&
      target_app_id.has_value() &&
      navigation_handling_first_stage_app.has_value() &&
      ShouldAlwaysOpenNewAppWindow(
          registrar, *navigation_handling_first_stage_app, *target_app_id)) {
    ReparentToAppBrowserEnqueueLaunchParams(web_contents_for_navigation,
                                            *target_app_id, final_url);
    return content::NavigationThrottle::PROCEED;
  }

  return content::NavigationThrottle::PROCEED;
}

NavigationCapturingRedirectionThrottle::NavigationCapturingRedirectionThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

}  // namespace web_app
