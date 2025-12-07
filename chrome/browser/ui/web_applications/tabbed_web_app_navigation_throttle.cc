// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/tabbed_web_app_navigation_throttle.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace web_app {

TabbedWebAppNavigationThrottle::TabbedWebAppNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

TabbedWebAppNavigationThrottle::~TabbedWebAppNavigationThrottle() = default;

const char* TabbedWebAppNavigationThrottle::GetNameForLogging() {
  return "TabbedWebAppNavigationThrottle";
}

// static
void TabbedWebAppNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame()) {
    return;
  }

  // Reloading the page should not cause the tab to change.
  if (handle.GetReloadType() != content::ReloadType::NONE) {
    return;
  }

  content::WebContents* web_contents = handle.GetWebContents();
  const tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return;
  }

  const BrowserWindowInterface* browser_window =
      tab->GetBrowserWindowInterface();
  if (!browser_window || !AppBrowserController::IsWebApp(browser_window)) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!AreWebAppsEnabled(profile)) {
    return;
  }

  const WebAppProvider* provider =
      WebAppProvider::GetForWebContents(web_contents);
  CHECK(provider);

  const webapps::AppId& app_id =
      AppBrowserController::From(browser_window)->app_id();

  std::optional<GURL> home_tab_url =
      provider->registrar_unsafe().GetAppPinnedHomeTabUrl(app_id);

  // Only create the throttle for tabbed web apps that have a home tab.
  if (WebAppTabHelper::GetAppId(web_contents) &&
      provider->registrar_unsafe().IsTabbedWindowModeEnabled(app_id) &&
      home_tab_url.has_value()) {
    registry.AddThrottle(
        std::make_unique<TabbedWebAppNavigationThrottle>(registry));
  }
}

content::NavigationThrottle::ThrottleCheckResult
TabbedWebAppNavigationThrottle::WillStartRequest() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  CHECK(tab);
  BrowserWindowInterface* browser_window = tab->GetBrowserWindowInterface();
  CHECK(browser_window);
  const AppBrowserController* app_controller =
      AppBrowserController::From(browser_window);
  CHECK(app_controller);
  const WebAppProvider* provider =
      WebAppProvider::GetForWebContents(web_contents);
  CHECK(provider);

  const webapps::AppId& app_id = app_controller->app_id();

  if (!app_controller->GetPinnedHomeTab() ||
      !provider->registrar_unsafe()
           .GetAppPinnedHomeTabUrl(app_id)
           .has_value()) {
    return content::NavigationThrottle::PROCEED;
  }

  bool navigating_from_home_tab =
      app_controller->GetPinnedHomeTab() == web_contents;
  bool navigation_url_is_home_url =
      app_controller->IsUrlInHomeTabScope(navigation_handle()->GetURL());

  // Navigations from the home tab to another URL should open in a new tab.
  if (navigating_from_home_tab && !navigation_url_is_home_url) {
    return OpenInNewTab();
  }

  // Navigations to the home tab URL should open in the home tab.
  if (!navigating_from_home_tab && navigation_url_is_home_url) {
    // target=_blank links to the home tab cause a blank tab to be opened. We
    // should close it.
    if (browser_window->GetTabStripModel()->count() > 1 &&
        !web_contents->GetLastCommittedURL().is_valid()) {
      web_contents->ClosePage();
    }
    return FocusHomeTab(*app_controller, *browser_window->GetTabStripModel());
  }

  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
TabbedWebAppNavigationThrottle::WillRedirectRequest() {
  // TODO(crbug.com/400761084): Figure out how redirects should be handled.
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
TabbedWebAppNavigationThrottle::OpenInNewTab() {
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  // Clear the FrameTreeNode id, as the new navigation will be in a new tab
  // rather than the frame of the original navigation.
  params.frame_tree_node_id = content::FrameTreeNodeId();
  navigation_handle()->GetWebContents()->OpenURL(
      std::move(params), /*navigation_handle_callback=*/{});
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

content::NavigationThrottle::ThrottleCheckResult
TabbedWebAppNavigationThrottle::FocusHomeTab(
    const AppBrowserController& app_controller,
    TabStripModel& tab_strip) {
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.frame_tree_node_id = content::FrameTreeNodeId();

  content::WebContents* pinned_home_tab = app_controller.GetPinnedHomeTab();
  CHECK(pinned_home_tab);
  CHECK_NE(pinned_home_tab, navigation_handle()->GetWebContents());
  if (params.url != pinned_home_tab->GetLastCommittedURL()) {
    // Only do the navigation if the URL has changed.
    pinned_home_tab->OpenURL(std::move(params),
                             /*navigation_handle_callback=*/{});
  }
  tab_strip.ActivateTabAt(tab_strip.GetIndexOfWebContents(pinned_home_tab));
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

}  // namespace web_app
