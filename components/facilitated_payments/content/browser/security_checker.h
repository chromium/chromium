// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_SECURITY_CHECKER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_SECURITY_CHECKER_H_

#include "components/security_state/core/security_state.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace payments::facilitated {

// This class is responsible for performing various security checks related to
// facilitated payments, such as SSL validity and payment Permissions Policy
// checks.
class SecurityChecker {
 public:
  SecurityChecker();
  SecurityChecker(const SecurityChecker&) = delete;
  SecurityChecker& operator=(const SecurityChecker&) = delete;
  virtual ~SecurityChecker();

  // Checks if the given RenderFrameHost is secure enough for payment link
  // handling. This includes verifying if the URL is potentially trustworthy,
  // uses HTTPS, has the payment Permissions Policy enabled, and has a valid SSL
  // certificate.
  virtual bool IsSecureForPaymentLinkHandling(content::RenderFrameHost& rfh);

 private:
  // Check whether SSL is valid according to its certificate state. Only SECURE
  // is considered valid for web payments, unless --ignore-certificate-errors is
  // specified on the command line.
  bool IsSslValid(content::WebContents& web_contents);

  // Returns the security level of `web_contents`.
  security_state::SecurityLevel GetSecurityLevel(
      content::WebContents& web_contents);

  // Returns whether the payment[1] permissions policy[2] is enabled.
  // [1]https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Permissions-Policy/payment
  // [2]https://developer.mozilla.org/en-US/docs/Web/HTTP/Permissions_Policy
  bool IsPaymentPermissionsPolicyEnabled(content::RenderFrameHost& rfh);
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_SECURITY_CHECKER_H_
