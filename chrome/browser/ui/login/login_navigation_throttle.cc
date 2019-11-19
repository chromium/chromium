// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_navigation_throttle.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/login/login_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"

LoginNavigationThrottle::LoginNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

LoginNavigationThrottle::~LoginNavigationThrottle() {}

LoginNavigationThrottle::ThrottleCheckResult
LoginNavigationThrottle::WillProcessResponse() {
  if (!base::FeatureList::IsEnabled(features::kHTTPAuthCommittedInterstitials))
    return PROCEED;
  if (navigation_handle()->IsSameDocument())
    return PROCEED;
  if (!navigation_handle()->IsInMainFrame())
    return PROCEED;

  LoginTabHelper* helper =
      LoginTabHelper::FromWebContents(navigation_handle()->GetWebContents());
  // The helper may not have been created yet if there was no auth challennge.
  if (!helper)
    return PROCEED;

  const net::HttpResponseHeaders* headers =
      navigation_handle()->GetResponseHeaders();
  if (!headers)
    return PROCEED;
  if (headers->response_code() != net::HTTP_UNAUTHORIZED &&
      headers->response_code() != net::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
    return PROCEED;
  }

  return helper->WillProcessMainFrameUnauthorizedResponse(navigation_handle());
}

const char* LoginNavigationThrottle::GetNameForLogging() {
  return "LoginNavigationThrottle";
}
