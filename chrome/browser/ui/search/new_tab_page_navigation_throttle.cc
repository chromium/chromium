// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/new_tab_page_navigation_throttle.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

NewTabPageNavigationThrottle::NewTabPageNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

NewTabPageNavigationThrottle::~NewTabPageNavigationThrottle() = default;

const char* NewTabPageNavigationThrottle::GetNameForLogging() {
  return "NewTabPageNavigationThrottle";
}

// static
std::unique_ptr<content::NavigationThrottle>
NewTabPageNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (web_contents->GetVisibleURL() != chrome::kChromeUINewTabURL ||
      !search::IsInstantNTPURL(handle->GetURL(), profile)) {
    return nullptr;
  }

  return std::make_unique<NewTabPageNavigationThrottle>(handle);
}

content::NavigationThrottle::ThrottleCheckResult
NewTabPageNavigationThrottle::WillProcessResponse() {
  const net::HttpResponseHeaders* headers =
      navigation_handle()->GetResponseHeaders();
  if (!headers)
    return content::NavigationThrottle::PROCEED;

  int response_code = headers->response_code();
  if (response_code < 400 && response_code != net::HTTP_NO_CONTENT)
    return content::NavigationThrottle::PROCEED;

  return OpenLocalNewTabPage();
}

content::NavigationThrottle::ThrottleCheckResult
NewTabPageNavigationThrottle::WillFailRequest() {
  return OpenLocalNewTabPage();
}

content::NavigationThrottle::ThrottleCheckResult
NewTabPageNavigationThrottle::OpenLocalNewTabPage() {
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.url = GURL(chrome::kChromeUINewTabPageThirdPartyURL);
  params.is_renderer_initiated = false;
  navigation_handle()->GetWebContents()->OpenURL(
      std::move(params), /*navigation_handle_callback=*/{});
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}
