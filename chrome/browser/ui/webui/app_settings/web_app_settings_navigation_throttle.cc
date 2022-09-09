// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_settings/web_app_settings_navigation_throttle.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

bool g_disable_throttle_for_testing_ = false;

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
WebAppSettingsNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  // Check the current url scheme is chrome://
  if (!handle->GetURL().SchemeIs(content::kChromeUIScheme))
    return nullptr;
  // Check the current url is chrome://app-settings
  if (handle->GetURL().host_piece() != chrome::kChromeUIWebAppSettingsHost)
    return nullptr;

  return std::make_unique<WebAppSettingsNavigationThrottle>(handle);
}

// static
void WebAppSettingsNavigationThrottle::DisableForTesting() {
  g_disable_throttle_for_testing_ = true;
}

WebAppSettingsNavigationThrottle::WebAppSettingsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

WebAppSettingsNavigationThrottle::~WebAppSettingsNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
WebAppSettingsNavigationThrottle::WillStartRequest() {
  if (g_disable_throttle_for_testing_)
    return content::NavigationThrottle::PROCEED;

  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!web_app::HasAppSettingsPage(profile, navigation_handle()->GetURL())) {
    return content::NavigationThrottle::BLOCK_REQUEST;
  }
  return content::NavigationThrottle::PROCEED;
}

const char* WebAppSettingsNavigationThrottle::GetNameForLogging() {
  return "WebAppSettingsNavigationThrottle";
}
