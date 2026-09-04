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

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_DATA_H_
