// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/tabbed_web_app_navigation_throttle.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace web_app {

TabbedWebAppNavigationThrottle::TabbedWebAppNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

TabbedWebAppNavigationThrottle::~TabbedWebAppNavigationThrottle() = default;

const char* TabbedWebAppNavigationThrottle::GetNameForLogging() {
  return "TabbedWebAppNavigationThrottle";
}

// static
std::unique_ptr<content::NavigationThrottle>
TabbedWebAppNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame())
    return nullptr;

  // Reloading the page should not cause the tab to change.
  if (handle->GetReloadType() != content::ReloadType::NONE) {
    return nullptr;
  }

  content::WebContents* web_contents = handle->GetWebContents();

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser || !browser->app_controller())
    return nullptr;

  WebAppProvider* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider)
    return nullptr;

  const webapps::AppId& app_id = browser->app_controller()->app_id();

  std::optional<GURL> home_tab_url =
      provider->registrar_unsafe().GetAppPinnedHomeTabUrl(app_id);

  // Only create the throttle for tabbed web apps that have a home tab.
  if (WebAppTabHelper::GetAppId(web_contents) &&
      provider->registrar_unsafe().IsTabbedWindowModeEnabled(app_id) &&
      home_tab_url.has_value()) {
    return std::make_unique<TabbedWebAppNavigationThrottle>(handle);
  }

  return nullptr;
}

content::NavigationThrottle::ThrottleCheckResult
TabbedWebAppNavigationThrottle::WillStartRequest() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();

  WebAppProvider* provider = WebAppProvider::GetForWebContents(web_contents);
  DCHECK(provider);

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  DCHECK(browser);
  web_app::AppBrowserController* app_controller = browser->app_controller();
  DCHECK(app_controller);

  const webapps::AppId& app_id = app_controller->app_id();

  std::optional<GURL> home_tab_url =
      provider->registrar_unsafe().GetAppPinnedHomeTabUrl(app_id);
  DCHECK(home_tab_url.has_value());

  auto* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  bool navigating_from_home_tab = tab_helper->is_pinned_home_tab();
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
    if (browser->tab_strip_model()->count() > 1 &&
        !web_contents->GetLastCommittedURL().is_valid()) {
      web_contents->ClosePage();
    }
    return FocusHomeTab();
  }

  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
TabbedWebAppNavigationThrottle::WillRedirectRequest() {
  // TODO(crbug.com/40598974): Figure out how redirects should be handled.
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
TabbedWebAppNavigationThrottle::OpenInNewTab() {
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  navigation_handle()->GetWebContents()->OpenURL(
      std::move(params), /*navigation_handle_callback=*/{});
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

content::NavigationThrottle::ThrottleCheckResult
TabbedWebAppNavigationThrottle::FocusHomeTab() {
  Browser* browser =
      chrome::FindBrowserWithTab(navigation_handle()->GetWebContents());
  TabStripModel* tab_strip = browser->tab_strip_model();

  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.frame_tree_node_id = content::FrameTreeNodeId();

  if (params.url != tab_strip->GetWebContentsAt(0)->GetLastCommittedURL()) {
    // Only do the navigation if the URL has changed.
    tab_strip->GetWebContentsAt(0)->OpenURL(std::move(params),
                                            /*navigation_handle_callback=*/{});
  }
  tab_strip->ActivateTabAt(0);
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

}  // namespace web_app
