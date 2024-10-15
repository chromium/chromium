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
void ReparentWebContentsToAppBrowser(content::WebContents* old_web_contents,
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
  RecordLaunchMetrics(
      app_id, apps::LaunchContainer::kLaunchContainerWindow,
      apps::LaunchSource::kFromNavigationCapturing, target_url,
      target_browser->tab_strip_model()->GetActiveWebContents());
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
  // If there were no redirects, then the only url in the redirect chain should
  // be the last url to go to. Exit early in this case.
  if (navigation_handle()->GetRedirectChain().size() == 1) {
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

  bool is_intermediate_app_matching_final_target =
      target_app_id.has_value() &&
      redirection_info.first_navigation_app_id.has_value() &&
      *target_app_id == *redirection_info.first_navigation_app_id;

  // Handle all the use-cases for redirections as specified here:
  // bit.ly/pwa-navigation-handling-dd

  // First, handle the case where a new app window was force-created for an app
  // for user modified clicks. Corrections handled here:
  // - If the final url IS NOT in scope any app, then reparent this new window
  // into a tabbed browser window based on the type of the user modified click.
  // - If the final url IS in scope of an app, then create a new app window and
  // reparent this web contents into the new app window, provided the source and
  // new app ids do not match.
  if (initial_nav_handling_result ==
          NavigationHandlingInitialResult::kAppWindowForcedNewContext &&
      source_app_id.has_value() &&
      !registrar.IsLinkCapturableByApp(*source_app_id, final_url) &&
      !is_intermediate_app_matching_final_target) {
    // Verify that the current web contents belong to an app browser.
    CHECK(chrome::FindBrowserWithTab(web_contents_for_navigation)
              ->app_controller());
    if (target_app_id.has_value() && *target_app_id != *source_app_id) {
      ReparentWebContentsToAppBrowser(web_contents_for_navigation,
                                      *target_app_id, final_url);
    } else {
      ReparentWebContentsToTabbedBrowser(web_contents_for_navigation,
                                         link_click_disposition);
    }
  }

  // Second, handle the use-case where the first result of navigation handling
  // was a browser tab, and that should be moved to an app window. This also
  // happens for user modified clicks. Corrections handled here:
  // - If the final url is in scope of an app, reparent the tab into an app
  // window of the target app id. For navigations that are triggered as a result
  // of a new background tab opening, this should only happen for same scope
  // navigations.
  if (initial_nav_handling_result ==
          NavigationHandlingInitialResult::kBrowserTab &&
      target_app_id.has_value() && source_app_id.has_value()) {
    if ((link_click_disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB &&
         *target_app_id == *source_app_id) ||
        (link_click_disposition == WindowOpenDisposition::NEW_WINDOW)) {
      ReparentWebContentsToAppBrowser(web_contents_for_navigation,
                                      *target_app_id, final_url);
    }
  }

  return content::NavigationThrottle::PROCEED;
}

NavigationCapturingRedirectionThrottle::NavigationCapturingRedirectionThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

}  // namespace web_app
