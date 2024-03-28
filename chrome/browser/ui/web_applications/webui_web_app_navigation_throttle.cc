// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/webui_web_app_navigation_throttle.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "url/gurl.h"

namespace web_app {

WebUIWebAppNavigationThrottle::WebUIWebAppNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

WebUIWebAppNavigationThrottle::~WebUIWebAppNavigationThrottle() = default;

const char* WebUIWebAppNavigationThrottle::GetNameForLogging() {
  return "WebUIWebAppNavigationThrottle";
}

// static
std::unique_ptr<content::NavigationThrottle>
WebUIWebAppNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame()) {
    return nullptr;
  }

  // Reloading the page should not cause the tab to change.
  if (handle->GetReloadType() != content::ReloadType::NONE) {
    return nullptr;
  }

  content::WebContents* web_contents = handle->GetWebContents();

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser || !browser->app_controller()) {
    return nullptr;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Exclude system web apps.
  if (browser->app_controller()->system_app()) {
    return nullptr;
  }
#endif

  // Proceed only if the app is coming from Chrome WebUI.
  GURL start_url = browser->app_controller()->GetAppStartUrl();
  if (!content::HasWebUIScheme(start_url)) {
    return nullptr;
  }

  return std::make_unique<WebUIWebAppNavigationThrottle>(handle);
}

content::NavigationThrottle::ThrottleCheckResult
WebUIWebAppNavigationThrottle::WillStartRequest() {
  GURL navigation_url = navigation_handle()->GetURL();

  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  DCHECK(browser);
  web_app::AppBrowserController* app_controller = browser->app_controller();
  DCHECK(app_controller);
  GURL start_url = browser->app_controller()->GetAppStartUrl();

  if (content::HasWebUIScheme(navigation_url) &&
      !url::IsSameOriginWith(navigation_url, start_url)) {
    content::OpenURLParams params =
        content::OpenURLParams::FromNavigationHandle(navigation_handle());
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    navigation_handle()->GetWebContents()->OpenURL(
        std::move(params), /*navigation_handle_callback=*/{});
    // Deactivate app window to foreground the browser with new tab.
    browser->window()->Deactivate();
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  } else {
    return content::NavigationThrottle::PROCEED;
  }
}

content::NavigationThrottle::ThrottleCheckResult
WebUIWebAppNavigationThrottle::WillRedirectRequest() {
  return WillStartRequest();
}

}  // namespace web_app
