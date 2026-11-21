// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_DATA_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_DATA_H_

#include <optional>

#include "url/gurl.h"

namespace enterprise_net {

// Base representation of enterprise proxy error data when an enterprise proxy
// disguised error (e.g. 403, 500, 502, 503, 504 realm challenge) is
// intercepted.
class EnterpriseProxyErrorData {
 public:
  // High-level category of the proxy error, used to select the appropriate
  // error page UI variation (e.g. action buttons, messaging).
  enum class ErrorCategory {
    // Authentication failure where the user needs to sign in or re-auth with
    // their account (e.g. invalid auth token, unauthenticated profile).
    // The error page prompts the user with a "Sign in" action.
    kAuthentication = 0,
    // Authorization failure where the user is signed in but lacks permission
    // to access the resource (e.g. HTTP 403 from proxy).
    // The error page displays an access denied message.
    kAuthorization = 1,
    // Uncategorized error cases such as proxy internal error (e.g. HTTP 500,
    // 502, 503, 504 from proxy) or network errors (OAuth fetch network
    // failure).
    kOther = 2,
    kMaxValue = kOther,
  };

  EnterpriseProxyErrorData();
  EnterpriseProxyErrorData(GURL destination_url,
                           GURL proxy_url,
                           int error_code,
                           ErrorCategory error_category = ErrorCategory::kOther,
                           std::optional<GURL> redirect_url = std::nullopt);
  EnterpriseProxyErrorData(const EnterpriseProxyErrorData&);
  EnterpriseProxyErrorData& operator=(const EnterpriseProxyErrorData&);
  EnterpriseProxyErrorData(EnterpriseProxyErrorData&&);
  EnterpriseProxyErrorData& operator=(EnterpriseProxyErrorData&&);
  virtual ~EnterpriseProxyErrorData();

  // The target destination URL that was being requested.
  const GURL& destination_url() const { return destination_url_; }
  // The enterprise proxy server URL.
  const GURL& proxy_url() const { return proxy_url_; }
  // The disguised HTTP error code (e.g. 403, 502).
  int error_code() const { return error_code_; }
  // The error category, used for deciding the error page UI variations.
  ErrorCategory error_category() const { return error_category_; }
  // Optional validated HTTPS URL for a static partner error page advertised via
  // the 407 `Proxy-Authenticate` `url=` parameter. When present on `//content`
  // platforms, `EnterpriseProxyTabHelper` redirects the tab to this URL instead
  // of rendering the built-in error page (on iOS, it is recorded to NetLog
  // while `EnterpriseProxyBlockingPage` renders the built-in interstitial).
  const std::optional<GURL>& redirect_url() const { return redirect_url_; }

 private:
  GURL destination_url_;
  GURL proxy_url_;
  int error_code_ = 0;
  ErrorCategory error_category_ = ErrorCategory::kOther;
  std::optional<GURL> redirect_url_;
};

// Events of the Secure Gateway error page, recorded in the
// "Enterprise.SecureGateway.ErrorPage.Event" histograms.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SecureGatewayErrorPageEvent)
enum class SecureGatewayErrorPageEvent {
  // The page is created for a failed navigation, right before it's displayed.
  kShown = 0,
  // The user taps "Go back".
  kGoBack = 1,
  // The user taps "Continue" to sign in again, whatever happens next.
  kContinue = 2,
  // The user leaves the page without using its button.
  kDismissed = 3,
  // The page is destroyed without having been displayed, which only happens
  // when its navigation doesn't commit.
  kNotDisplayed = 4,
  kMaxValue = kNotDisplayed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:EnterpriseSecureGatewayErrorPageEvent)

// Records `event` for a Secure Gateway error page of `category`, emitting both
// to the aggregate "Enterprise.SecureGateway.ErrorPage.Event" histogram and to
// the category-specific ".Authentication", ".Authorization", or ".Other"
// variant.
void RecordEnterpriseProxyErrorPageEvent(
    EnterpriseProxyErrorData::ErrorCategory category,
    SecureGatewayErrorPageEvent event);

// Outcomes of evaluating and executing a static error page `url=` redirect on a
// primary main-frame disguised-error 407 challenge, recorded in
// "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome".
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SecureGatewayStaticErrorPageRedirectOutcome)
enum class SecureGatewayStaticErrorPageRedirectOutcome {
  // Valid `url=` started a replacement navigation to the static error page.
  kRedirected = 0,
  // `Proxy-Authenticate` challenge failed strict `Basic` parameter/header
  // parsing (e.g. malformed quotes, non-token names/values, duplicate
  // parameters, or duplicate same-realm headers).
  kRejectedInvalidChallenge = 1,
  // `url=` parameter or `EnterpriseProxyErrorData::redirect_url()` was invalid
  // or unsafe (e.g. non-`https://` scheme, embedded credentials, or whitespace)
  // or arrived over a non-HTTPS proxy challenger.
  kRejectedInvalidUrl = 2,
  // `url=` pointed back to the failed destination URL or was triggered on an
  // existing static-error-page redirect navigation (loop protection).
  kRejectedLoopDetected = 3,
  // Challenge destination URL did not match the active primary main-frame
  // navigation URL.
  kRejectedDestinationMismatch = 4,
  // Active navigation changed or could not start the replacement navigation
  // before the posted redirect task executed.
  kCanceledBeforeRedirect = 5,
  kMaxValue = kCanceledBeforeRedirect,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:EnterpriseSecureGatewayStaticErrorPageRedirectOutcome)

// Records `outcome` to the
// "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome" histogram.
void RecordStaticErrorPageRedirectOutcome(
    SecureGatewayStaticErrorPageRedirectOutcome outcome);

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_DATA_H_
