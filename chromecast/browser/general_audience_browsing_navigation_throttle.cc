// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/general_audience_browsing_navigation_throttle.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chromecast/browser/general_audience_browsing_service.h"
#include "components/policy/core/browser/url_util.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

namespace chromecast {

GeneralAudienceBrowsingNavigationThrottle::
    GeneralAudienceBrowsingNavigationThrottle(
        content::NavigationHandle* navigation_handle,
        GeneralAudienceBrowsingService* general_audience_browsing_service)
    : NavigationThrottle(navigation_handle),
      general_audience_browsing_service_(general_audience_browsing_service),
      weak_ptr_factory_(this) {
  DCHECK(general_audience_browsing_service_);
}

GeneralAudienceBrowsingNavigationThrottle::
    ~GeneralAudienceBrowsingNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
GeneralAudienceBrowsingNavigationThrottle::CheckURL() {
  deferred_ = false;
  const GURL& url = navigation_handle()->GetURL();
  DVLOG(1) << "Check URL " << url.spec();

  // Only apply filters to HTTP[s] URLs.
  if (!url.SchemeIsHTTPOrHTTPS())
    return PROCEED;

  GURL effective_url = policy::url_util::GetEmbeddedURL(url);
  if (!effective_url.is_valid())
    effective_url = url;
  GURL normalized_url = policy::url_util::Normalize(effective_url);

  bool synchronous = general_audience_browsing_service_->CheckURL(
      effective_url,
      base::BindOnce(
          &GeneralAudienceBrowsingNavigationThrottle::CheckURLCallback,
          weak_ptr_factory_.GetWeakPtr()));

  if (!synchronous) {
    deferred_ = true;
    return DEFER;
  }

  if (should_cancel_) {
    DVLOG(1) << "Unsafe URL blocked";
    return ThrottleCheckResult(CANCEL, net::ERR_BLOCKED_BY_ADMINISTRATOR);
  }
  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
GeneralAudienceBrowsingNavigationThrottle::WillStartRequest() {
  return CheckURL();
}

content::NavigationThrottle::ThrottleCheckResult
GeneralAudienceBrowsingNavigationThrottle::WillRedirectRequest() {
  return CheckURL();
}

const char* GeneralAudienceBrowsingNavigationThrottle::GetNameForLogging() {
  return "GeneralAudienceBrowsingNavigationThrottle";
}

void GeneralAudienceBrowsingNavigationThrottle::CheckURLCallback(bool is_safe) {
  if (!deferred_) {
    should_cancel_ = !is_safe;
    return;
  }

  deferred_ = false;
  if (is_safe) {
    Resume();
  } else {
    DVLOG(1) << "Unsafe URL blocked";
    CancelDeferredNavigation(
        ThrottleCheckResult(CANCEL, net::ERR_BLOCKED_BY_ADMINISTRATOR));
  }
}

}  // namespace chromecast
