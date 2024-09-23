// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_side_panel_navigation_helper.h"

#include <string_view>

#include "chrome/browser/ui/browser.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/page_transition_types.h"

namespace {

// List of domains that are safe to happen in the background. To be in the list,
// the site must not be reachable by user navigation.
static constexpr std::string_view BACKGROUND_THROTTLE_EXCEPTIONS[] = {
    "https://feedback.googleusercontent.com"};

bool IsSameSite(const GURL& url1, const GURL& url2) {
  return url1.SchemeIs(url2.scheme()) &&
         net::registry_controlled_domains::SameDomainOrHost(
             url1, url2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// Helper that returns if the given URL is in the exception set
bool HasThrottleException(const GURL& url) {
  for (std::string_view safe_site : BACKGROUND_THROTTLE_EXCEPTIONS) {
    if (IsSameSite(url, GURL(safe_site)))
      return true;
  }
  return false;
}

class LensSidePanelNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit LensSidePanelNavigationThrottle(
      content::NavigationHandle* navigation_handle)
      : NavigationThrottle(navigation_handle) {}

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override {
    return HandleSidePanelRequest();
  }

  ThrottleCheckResult WillRedirectRequest() override {
    return HandleSidePanelRequest();
  }

  const char* GetNameForLogging() override {
    return "LensSidePanelNavigationThrottle";
  }

 private:
  ThrottleCheckResult HandleSidePanelRequest() {
    const auto& url = navigation_handle()->GetURL();

    // All navigations on a separate subdomain open in a new tab
    auto* navigation_helper =
        lens::LensSidePanelNavigationHelper::FromWebContents(
            navigation_handle()->GetWebContents());
    DCHECK(navigation_helper);

    // Allow navigations that belong to the same site as the image search
    // engine or those that have an exception.
    if (IsSameSite(navigation_helper->GetOriginUrl(), url) ||
        HasThrottleException(url))
      return content::NavigationThrottle::PROCEED;

    auto params =
        content::OpenURLParams::FromNavigationHandle(navigation_handle());

    // All user clicks to a destination otuside of the origin URL should open in
    // a new tab
    if (ui::PageTransitionCoreTypeIs(params.transition,
                                     ui::PAGE_TRANSITION_LINK)) {
      navigation_helper->OpenInNewTab(params);
    }
    return content::NavigationThrottle::CANCEL;
  }
};

}  // namespace

namespace lens {
LensSidePanelNavigationHelper::LensSidePanelNavigationHelper(
    content::WebContents* web_contents,
    Browser* browser,
    const std::string& origin_url)
    : content::WebContentsUserData<LensSidePanelNavigationHelper>(
          *web_contents),
      origin_url_(origin_url) {
  // If Lens side panel every becomes contextual (i.e. not tied to one browser
  // window) this implementation needs to be revisited to update the browser
  // pointer when Lens side panel changes windows.
  browser_ = browser;
}

LensSidePanelNavigationHelper::~LensSidePanelNavigationHelper() = default;

void LensSidePanelNavigationHelper::OpenInNewTab(
    content::OpenURLParams params) {
  if (browser_ == nullptr)
    return;
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  browser_->OpenURL(params, /*navigation_handle_callback=*/{});
}

const GURL& LensSidePanelNavigationHelper::GetOriginUrl() {
  return origin_url_;
}

std::unique_ptr<content::NavigationThrottle>
LensSidePanelNavigationHelper::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  // The helper will only return an address if a Lens side panel has been
  // initialized
  auto* helper =
      LensSidePanelNavigationHelper::FromWebContents(handle->GetWebContents());
  if (!helper)
    return nullptr;

  return std::make_unique<LensSidePanelNavigationThrottle>(handle);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LensSidePanelNavigationHelper);
}  // namespace lens
