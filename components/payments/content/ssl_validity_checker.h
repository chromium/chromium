// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SSL_VALIDITY_CHECKER_H_
#define COMPONENTS_PAYMENTS_CONTENT_SSL_VALIDITY_CHECKER_H_

#include <string>

#include "components/security_state/core/security_state.h"

namespace content {
class WebContents;
}

namespace payments {

class SslValidityChecker {
 public:
  SslValidityChecker() = delete;
  SslValidityChecker(const SslValidityChecker&) = delete;
  SslValidityChecker& operator=(const SslValidityChecker&) = delete;

  // Returns a developer-facing error message for invalid SSL certificate state
  // or an empty string when the SSL certificate is valid. Only SECURE and
  // SECURE_WITH_POLICY_INSTALLED_CERT are considered valid for web payments,
  // unless --ignore-certificate-errors is specified on the command line.
  //
  // The |web_contents| parameter should not be null. A null
  // |web_contents| parameter will return an "Invalid certificate" error
  // message.
  static std::string GetInvalidSslCertificateErrorMessage(
      content::WebContents* web_contents);

  // Whether the given page should be allowed to be displayed in a payment
  // handler window.
  static bool IsValidPageInPaymentHandlerWindow(
      content::WebContents* web_contents);

  // Returns the security level of `web_contents`. The `web_contents` parameter
  // should not be null.
  static security_state::SecurityLevel GetSecurityLevel(
      content::WebContents* web_contents);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SSL_VALIDITY_CHECKER_H_
