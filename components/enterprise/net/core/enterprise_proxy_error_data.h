// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_DATA_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_DATA_H_

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
  EnterpriseProxyErrorData(
      GURL destination_url,
      GURL proxy_url,
      int error_code,
      ErrorCategory error_category = ErrorCategory::kOther);
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

 private:
  GURL destination_url_;
  GURL proxy_url_;
  int error_code_ = 0;
  ErrorCategory error_category_ = ErrorCategory::kOther;
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

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_DATA_H_
