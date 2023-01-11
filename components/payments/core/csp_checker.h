// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_CSP_CHECKER_H_
#define COMPONENTS_PAYMENTS_CORE_CSP_CHECKER_H_

#include "base/functional/callback_forward.h"

class GURL;

namespace payments {

// Interface for checking Content Security Policy (CSP).
class CSPChecker {
 public:
  virtual ~CSPChecker() = default;

  // Checks whether CSP connect-src directive allows the given `url`. The
  // parameters match ContentSecurityPolicy::AllowConnectToSource() in:
  //   third_party/blink/renderer/core/frame/csp/content_security_policy.h
  virtual void AllowConnectToSource(
      const GURL& url,
      const GURL& url_before_redirects,
      bool did_follow_redirect,
      base::OnceCallback<void(bool)> result_callback) = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_CSP_CHECKER_H_
