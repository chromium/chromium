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
#include "chrome/browser/ui/web_applications/navigation_capturing_information_forwarder.h"
#include "chrome/browser/ui/web_applications/navigation_capturing_navigation_handle_user_data.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {
bool WasCaptured(NavigationHandlingInitialResult result) {
  switch (result) {
    case NavigationHandlingInitialResult::kBrowserTab:
    case NavigationHandlingInitialResult::kForcedNewAppContextAppWindow:
    case NavigationHandlingInitialResult::kForcedNewAppContextBrowserTab:
    case NavigationHandlingInitialResult::kNotHandledByNavigationHandling:
    case NavigationHandlingInitialResult::kAuxContext:
      return false;
    case NavigationHandlingInitialResult::kNavigateCapturedNewAppWindow:
    case NavigationHandlingInitialResult::kNavigateCapturedNewBrowserTab:
    case NavigationHandlingInitialResult::kNavigateCapturingNavigateExisting:
      return true;
  }
}

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

// TODO(crbug.com/371237535): Move to TabInterface once there is support for
// getting the browser interface for web contents that are in an app window.
// For all use-cases where a reparenting to an app window happens, launch params
// need to be enqueued so as to mimic the pre redirection behavior. See
// https://bit.ly/pwa-navigation-capturing?tab=t.0#bookmark=id.60x2trlfg6iq for
// more information.
void ReparentToAppBrowser(content::WebContents* old_web_contents,
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
      (disposition == WindowOpenDisposition::NEW_WINDOW ||
       !existing_browser_window)
          ? Browser::Create(Browser::CreateParams(source_browser->profile(),
                                                  /*user_gesture=*/true))
          : existing_browser_window;

  ReparentWebContentsIntoBrowserImpl(source_browser, old_web_contents,
                                     target_browser_window);
}

std::optional<webapps::AppId> GetWebAppControllingUrl(
    Profile* profile,
    const WebAppProvider* provider,
    const GURL& url) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return std::nullopt;
  }
  return apps::FindAppIdsToLaunchForUrl(
             apps::AppServiceProxyFactory::GetForProfile(profile), url)
      .preferred;
#else
  return provider->registrar_unsafe().FindAppThatCapturesLinksInScope(url);
#endif
}

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
NavigationCapturingRedirectionThrottle::MaybeCreate(
    content::NavigationHandle* handle) {
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
  ThrottleCheckResult result = HandleResponse();

  // If the navigation is not cancelled, this is the time to enqueue launch
  // params, record launch metrics and maybe show a navigation capturing IPH.
  // Note that there is still a small chance that some other navigation throttle
  // will cancel this navigation, so ideally we would wait until the navigation
  // actually commits, but this is an easier place to hook into.
  if (result.action() != content::NavigationThrottle::CANCEL) {
    NavigationCapturingNavigationHandleUserData* handle_user_data =
        NavigationCapturingNavigationHandleUserData::GetForNavigationHandle(
            *navigation_handle());
    if (handle_user_data) {
      handle_user_data->MaybePerformAppHandlingTasksInWebContents();
    }
  }

  return result;
}

ThrottleCheckResult NavigationCapturingRedirectionThrottle::HandleResponse() {
  // Bail out if there's no Navigation Capturing data attached. Note: this
  // cannot be checked in `MaybeCreate()` since the data might get attached
  // after it's executed.
  NavigationCapturingNavigationHandleUserData* handle_user_data =
      NavigationCapturingNavigationHandleUserData::GetForNavigationHandle(
          *navigation_handle());
  if (!handle_user_data || !handle_user_data->redirection_info().has_value()) {
    return content::NavigationThrottle::PROCEED;
  }

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

  NavigationCapturingRedirectionInfo redirection_info =
      *handle_user_data->redirection_info();
  WindowOpenDisposition link_click_disposition = redirection_info.disposition();
  NavigationHandlingInitialResult initial_nav_handling_result =
      redirection_info.initial_nav_handling_result();
  std::optional<webapps::AppId> source_browser_app_id =
      redirection_info.source_browser_app_id();
  std::optional<webapps::AppId> source_tab_app_id =
      redirection_info.source_tab_app_id();
  std::optional<webapps::AppId> navigation_handling_first_stage_app =
      redirection_info.first_navigation_app_id();

  // Do not handle redirections for navigations that create an auxiliary
  // browsing context, or if the app window that opened is not a part of the
  // navigation handling flow.
  if (initial_nav_handling_result ==
          NavigationHandlingInitialResult::kNotHandledByNavigationHandling ||
      initial_nav_handling_result ==
          NavigationHandlingInitialResult::kAuxContext) {
    return content::NavigationThrottle::PROCEED;
  }

  content::WebContents* const web_contents_for_navigation =
      navigation_handle()->GetWebContents();

  WebAppProvider* provider =
      WebAppProvider::GetForWebContents(web_contents_for_navigation);
  WebAppRegistrar& registrar = provider->registrar_unsafe();
  std::optional<webapps::AppId> target_app_id =
      GetWebAppControllingUrl(&profile_.get(), provider, final_url);

  // "Same first navigation state" case:
  // First, we can exit early if the first navigation app id matches the target
  // app id (which includes if they are both std::nullopt), as this means we
  // already did the 'correct' navigation capturing behavior on the first
  // navigation.
  if (navigation_handling_first_stage_app == target_app_id) {
    return content::NavigationThrottle::PROCEED;
  }

  // Clear out the "launch app id" field. This way we ensure that in any branch
  // where the redirect does not result in an app being launched we don't
  // accidentally (try to) treat it as a launch. Any branch where an app launch
  // does happen will re-set the field to the correct value.
  handle_user_data->SetLaunchedAppState(std::nullopt, /*force_iph_off=*/false);

  // After this point:
  // - The browsing context is a top-level browsing context.
  // - The initial navigation capturing app_id does not match the final
  //   target_app_id (and either can be std::nullopt, but not both).
  // - Navigation is only triggered as part of left, middle or shift clicks.

  bool is_source_app_matching_final_target =
      target_app_id == source_browser_app_id;

  // First, handle cases where the final url is not in scope of any app. These
  // can mostly proceed as is, except for two cases where the initial navigation
  // ended up in an app window but should now be in a browser tab.
  if (!target_app_id.has_value()) {
    if (initial_nav_handling_result ==
            NavigationHandlingInitialResult::kForcedNewAppContextAppWindow ||
        initial_nav_handling_result ==
            NavigationHandlingInitialResult::kNavigateCapturedNewAppWindow) {
      ReparentWebContentsToTabbedBrowser(web_contents_for_navigation,
                                         link_click_disposition);
    }
    return content::NavigationThrottle::PROCEED;
  }

  blink::mojom::DisplayMode target_display_mode =
      registrar.GetAppEffectiveDisplayMode(*target_app_id);
  CHECK(WebAppRegistrar::IsSupportedDisplayModeForNavigationCapture(
      target_display_mode));

  // For the remaining cases we know that the navigation ended up in scope of a
  // target application.

  // First, handle the case where a new app container (app or browser) was
  // force-created for an app for user modified clicks. This refers to the
  // use-cases here:
  // https://bit.ly/pwa-navigation-capturing?tab=t.0#bookmark=id.ugh0e993wsl8,
  // where a new app container is made.
  if (initial_nav_handling_result ==
      NavigationHandlingInitialResult::kForcedNewAppContextAppWindow) {
    CHECK(redirection_info.source_browser_app_id().has_value());
    CHECK(navigation_handling_first_stage_app);
    // standalone-app -> browser-tab-app.
    if (target_display_mode == blink::mojom::DisplayMode::kBrowser) {
      handle_user_data->SetLaunchedAppState(*target_app_id,
                                            /*force_iph_off=*/true);
      ReparentWebContentsToTabbedBrowser(web_contents_for_navigation,
                                         link_click_disposition);
      return content::NavigationThrottle::PROCEED;
    }
    // standalone-app -> standalone-app.
    handle_user_data->SetLaunchedAppState(*target_app_id,
                                          /*force_iph_off=*/false);
    CHECK(target_display_mode != blink::mojom::DisplayMode::kBrowser);
    ReparentToAppBrowser(web_contents_for_navigation, *target_app_id,
                         final_url);
    return content::NavigationThrottle::PROCEED;
  }
  if (initial_nav_handling_result ==
      NavigationHandlingInitialResult::kForcedNewAppContextBrowserTab) {
    // browser-tab-app -> browser-tab-app.
    if (target_display_mode == blink::mojom::DisplayMode::kBrowser) {
      return content::NavigationThrottle::PROCEED;
    }
    // browser-tab-app -> standalone-app. This must have a source app id to
    // ensure that we cannot have a user-modified click go from a regular
    // browser tab to an app window.
    CHECK(target_display_mode != blink::mojom::DisplayMode::kBrowser);
    if (source_browser_app_id.has_value()) {
      handle_user_data->SetLaunchedAppState(*target_app_id,
                                            /*force_iph_off=*/false);
      ReparentToAppBrowser(web_contents_for_navigation, *target_app_id,
                           final_url);
      return content::NavigationThrottle::PROCEED;
    }
  }

  // Handle the last user-modified correction, where a user-modified click from
  // an app went to the browser, but needs to be reparented back into an app.
  // See
  // https://bit.ly/pwa-navigation-capturing?tab=t.0#bookmark=id.ugh0e993wsl8
  // for more information.
  bool is_user_modified =
      (link_click_disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) ||
      (link_click_disposition == WindowOpenDisposition::NEW_WINDOW);
  if (initial_nav_handling_result ==
          NavigationHandlingInitialResult::kBrowserTab &&
      is_user_modified && source_browser_app_id.has_value()) {
    // As per the UX direction in the doc, NEW_BACKGROUND_TAB only creates a
    // new app window for an app when coming from the same app window.
    // Otherwise, only NEW_WINDOW can create a new app window when coming from
    // an app.
    if ((link_click_disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB &&
         is_source_app_matching_final_target) ||
        (link_click_disposition == WindowOpenDisposition::NEW_WINDOW)) {
      // browser-tab -> browser-tab-app.
      if (target_display_mode == blink::mojom::DisplayMode::kBrowser) {
        handle_user_data->SetLaunchedAppState(*target_app_id,
                                              /*force_iph_off=*/true);
        return content::NavigationThrottle::PROCEED;
      }
      // browser-tab -> standalone app
      handle_user_data->SetLaunchedAppState(*target_app_id,
                                            /*force_iph_off=*/false);
      ReparentToAppBrowser(web_contents_for_navigation, *target_app_id,
                           final_url);
      return content::NavigationThrottle::PROCEED;
    }
  }

  // All other user-modified cases should do the default thing and navigate the
  // existing container.
  if (is_user_modified) {
    return content::NavigationThrottle::PROCEED;
  }

  // TODO(https://crbug.com/382542355): Populate
  // `navigate_params_requested_browser` by passing it through the redirection
  // info from `MaybeHandleAppNavigation`.
  ClientModeAndBrowser client_mode_and_browser =
      GetEffectiveClientModeAndBrowserForCapturing(
          *profile_, *target_app_id, source_tab_app_id,
          /*ignore_browser_tabs_for_standalone_apps=*/false,
          /*navigate_params_requested_browser=*/nullptr);

  // After this point:
  // - The navigation is non-user-modified.
  // - This is a top-level browsing context.
  // - The first navigation app_id doesn't match the target app_id (as per "Same
  //   first navigation state" case above).

  // Handle all cases where the initial navigation was captured, and that now
  // needs to be corrected. See the table at
  // bit.ly/pwa-navigation-handling-dd?tab=t.0#bookmark=id.hnvzj4iwiviz

  // First, address the 'navigate-new' or 'browser' initial capture, where the
  // final state is also an effective 'navigate-new'.
  if (client_mode_and_browser.effective_client_mode ==
          LaunchHandler::ClientMode::kNavigateNew &&
      (initial_nav_handling_result ==
           NavigationHandlingInitialResult::kBrowserTab ||
       initial_nav_handling_result ==
           NavigationHandlingInitialResult::kNavigateCapturedNewAppWindow ||
       initial_nav_handling_result ==
           NavigationHandlingInitialResult::kNavigateCapturedNewBrowserTab)) {
    // Handle all cases that result in a standalone app.
    // (browser tab, browser-tab-app, or standalone-app -> standalone-app)
    if (target_display_mode != blink::mojom::DisplayMode::kBrowser) {
      handle_user_data->SetLaunchedAppState(*target_app_id,
                                            /*force_iph_off=*/false);
      ReparentToAppBrowser(web_contents_for_navigation, *target_app_id,
                           final_url);
      return content::NavigationThrottle::PROCEED;
    }
    // Handle all cases that result in a browser-tab-app.
    // (browser tab, browser-tab-app, or standalone-app -> browser-tab-app)
    CHECK(target_display_mode == blink::mojom::DisplayMode::kBrowser);
    handle_user_data->SetLaunchedAppState(*target_app_id,
                                          /*force_iph_off=*/true);
    if (initial_nav_handling_result ==
        NavigationHandlingInitialResult::kNavigateCapturedNewAppWindow) {
      ReparentWebContentsToTabbedBrowser(web_contents_for_navigation,
                                         link_click_disposition);
    }
    return content::NavigationThrottle::PROCEED;
  }

  // Only proceed from now on if the final app can be capturable depending on
  // the result of the initial navigation handling. This involves only 2
  // use-cases, where the intermediary result is either a browser tab, or an app
  // window that opened as a result of a capturable navigation.
  // TODO(crbug.com/375619465): Implement open-in-browser-tab app support for
  // redirection.
  bool final_navigation_can_be_capturable =
      WasCaptured(initial_nav_handling_result) ||
      initial_nav_handling_result ==
          NavigationHandlingInitialResult::kBrowserTab;
  if (!final_navigation_can_be_capturable) {
    return content::NavigationThrottle::PROCEED;
  }

  // Handle the use-case where the target_app_id has a launch handling mode of
  // kFocusExisting or kNavigateExisting. In both cases this navigation gets
  // aborted, and in some cases the web contents being navigated gets closed.
  if (client_mode_and_browser.effective_client_mode ==
          LaunchHandler::ClientMode::kFocusExisting ||
      client_mode_and_browser.effective_client_mode ==
          LaunchHandler::ClientMode::kNavigateExisting) {
    CHECK(client_mode_and_browser.browser);
    CHECK(client_mode_and_browser.tab_index.has_value());
    CHECK_NE(*client_mode_and_browser.tab_index, -1);

    FocusAppContainer(client_mode_and_browser.browser,
                      *client_mode_and_browser.tab_index);

    content::WebContents* pre_existing_contents =
        client_mode_and_browser.browser->tab_strip_model()->GetWebContentsAt(
            *client_mode_and_browser.tab_index);
    CHECK(pre_existing_contents);
    CHECK_NE(pre_existing_contents, web_contents_for_navigation);

    if (client_mode_and_browser.effective_client_mode ==
        LaunchHandler::ClientMode::kNavigateExisting) {
      content::OpenURLParams params =
          content::OpenURLParams::FromNavigationHandle(navigation_handle());

      // Reset the frame_tree_node_id to make sure we're navigating the main
      // frame in the target web contents.
      params.frame_tree_node_id = {};

      pre_existing_contents->OpenURL(
          params, base::BindOnce(
                      [](const webapps::AppId& target_app_id,
                         content::NavigationHandle& navigation_handle) {
                        NavigationCapturingNavigationHandleUserData::
                            CreateForNavigationHandle(
                                navigation_handle,
                                /*redirection_info=*/std::nullopt,
                                target_app_id, /*force_iph_off=*/false);
                      },
                      *target_app_id));

    } else {
      // Perform post navigation operations, like recording app launch metrics,
      // or showing the navigation capturing IPH.
      EnqueueLaunchParams(pre_existing_contents, *target_app_id, final_url,
                          /*wait_for_navigation_to_complete=*/false);
      MaybeShowNavigationCaptureIph(*target_app_id, &profile_.get(),
                                    client_mode_and_browser.browser);
      RecordLaunchMetrics(*target_app_id,
                          apps::LaunchContainer::kLaunchContainerWindow,
                          apps::LaunchSource::kFromNavigationCapturing,
                          final_url, pre_existing_contents);
    }

    // Close the old tab or app window, if it was created as part of the current
    // navigation to mimic the behavior where the redirected url matches an
    // outcome without redirection. Any residual app windows or tabs that were
    // there before the current navigation started shouldn't be closed.
    if (initial_nav_handling_result ==
            NavigationHandlingInitialResult::kNavigateCapturedNewAppWindow ||
        initial_nav_handling_result ==
            NavigationHandlingInitialResult::kNavigateCapturedNewBrowserTab ||
        initial_nav_handling_result ==
            NavigationHandlingInitialResult::kBrowserTab) {
      web_contents_for_navigation->ClosePage();
    }
    return content::NavigationThrottle::CANCEL;
  }
  return content::NavigationThrottle::PROCEED;
}

NavigationCapturingRedirectionThrottle::NavigationCapturingRedirectionThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      profile_(*Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())) {}

}  // namespace web_app
