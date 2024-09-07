// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/safe_sites_navigation_throttle.h"

#include "base/functional/bind.h"
#include "components/policy/content/safe_search_service.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

// Use of Unretained for is safe because it is called synchronously from this
// object.
SafeSitesNavigationThrottle::SafeSitesNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    content::BrowserContext* context,
    std::optional<std::string_view> safe_sites_error_page_content)
    : Client(navigation_handle),
      safe_search_service_(SafeSearchFactory::GetForBrowserContext(context)),
      safe_sites_error_page_content_(std::move(safe_sites_error_page_content)) {
  SetDeferredResultCallback(base::BindRepeating(
      &SafeSitesNavigationThrottle::OnDeferredResult, base::Unretained(this)));
}

SafeSitesNavigationThrottle::~SafeSitesNavigationThrottle() = default;

void SafeSitesNavigationThrottle::SetDeferredResultCallback(
    const ProceedUntilResponseNavigationThrottle::DeferredResultCallback&
        deferred_result_callback) {
  deferred_result_callback_ = deferred_result_callback;
}

content::NavigationThrottle::ThrottleCheckResult
SafeSitesNavigationThrottle::WillStartRequest() {
  const GURL& url = navigation_handle()->GetURL();

  // Ignore blob scheme because we may use it to deliver navigation responses
  // to the renderer process.
  if (url.SchemeIs(url::kBlobScheme)) {
    return PROCEED;
  }

  // Safe Sites filter applies to top-level HTTP[S] requests.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return PROCEED;
  }

  GURL effective_url = url_matcher::util::GetEmbeddedURL(url);
  if (!effective_url.is_valid()) {
    effective_url = url;
  }

  const bool synchronous = safe_search_service_->CheckSafeSearchURL(
      effective_url,
      base::BindOnce(&SafeSitesNavigationThrottle::CheckSafeSearchCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  if (!synchronous) {
    deferred_ = true;
    return DEFER;
  }

  if (should_cancel_) {
    return CreateCancelResult();
  }
  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
SafeSitesNavigationThrottle::WillRedirectRequest() {
  return WillStartRequest();
}

const char* SafeSitesNavigationThrottle::GetNameForLogging() {
  return "SafeSitesNavigationThrottle";
}

void SafeSitesNavigationThrottle::CheckSafeSearchCallback(bool is_safe) {
  if (!deferred_) {
    should_cancel_ = !is_safe;
    return;
  }

  deferred_ = false;
  deferred_result_callback_.Run(is_safe, CreateCancelResult());
}

void SafeSitesNavigationThrottle::OnDeferredResult(
    bool proceed,
    std::optional<ThrottleCheckResult> result) {
  if (proceed) {
    Resume();
  } else {
    CHECK(result.has_value());
    CancelDeferredNavigation(*result);
  }
}

content::NavigationThrottle::ThrottleCheckResult
SafeSitesNavigationThrottle::CreateCancelResult() const {
  return ThrottleCheckResult(CANCEL, net::ERR_BLOCKED_BY_ADMINISTRATOR,
                             safe_sites_error_page_content_);
}
