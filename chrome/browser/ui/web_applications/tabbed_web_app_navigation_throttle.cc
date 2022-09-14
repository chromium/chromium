// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/tabbed_web_app_navigation_throttle.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
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
  content::WebContents* web_contents = handle->GetWebContents();

  WebAppProvider* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider)
    return nullptr;

  absl::optional<web_app::AppId> app_id =
      provider->registrar().FindInstalledAppWithUrlInScope(
          handle->GetURL(), /*window_only=*/true);
  if (!app_id)
    return nullptr;

  absl::optional<GURL> home_tab_url =
      provider->registrar().GetAppPinnedHomeTabUrl(*app_id);

  auto* tab_helper = WebAppTabHelper::FromWebContents(web_contents);

  // Only create the throttle for tabbed web apps that have a home tab.
  if (tab_helper && tab_helper->acting_as_app() &&
      provider->registrar().IsTabbedWindowModeEnabled(*app_id) &&
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

  absl::optional<web_app::AppId> app_id =
      provider->registrar().FindInstalledAppWithUrlInScope(
          navigation_handle()->GetURL(), /*window_only=*/true);
  DCHECK(app_id);

  absl::optional<GURL> home_tab_url =
      provider->registrar().GetAppPinnedHomeTabUrl(*app_id);
  DCHECK(home_tab_url.has_value());

  auto* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  bool navigating_from_home_tab = tab_helper->is_pinned_home_tab();
  bool navigation_url_is_home_url = IsPinnedHomeTabUrl(
      provider->registrar(), *app_id, navigation_handle()->GetURL());

  // Navigations from the home tab to another URL should open in a new tab.
  if (navigating_from_home_tab && !navigation_url_is_home_url) {
    return OpenInNewTab();
  }

  // Navigations to the home tab URL should open in the home tab.
  if (!navigating_from_home_tab && navigation_url_is_home_url) {
    return FocusHomeTab();
  }

  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
TabbedWebAppNavigationThrottle::OpenInNewTab() {
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  navigation_handle()->GetWebContents()->OpenURL(std::move(params));
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

content::NavigationThrottle::ThrottleCheckResult
TabbedWebAppNavigationThrottle::FocusHomeTab() {
  Browser* browser =
      chrome::FindBrowserWithWebContents(navigation_handle()->GetWebContents());
  TabStripModel* tab_strip = browser->tab_strip_model();

  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.frame_tree_node_id = content::RenderFrameHost::kNoFrameTreeNodeId;

  if (params.url != tab_strip->GetWebContentsAt(0)->GetLastCommittedURL()) {
    // Only do the navigation if the URL has changed.
    tab_strip->GetWebContentsAt(0)->OpenURL(std::move(params));
  }
  tab_strip->ActivateTabAt(0);
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

}  // namespace web_app
