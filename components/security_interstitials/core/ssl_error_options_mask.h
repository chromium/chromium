// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_SSL_ERROR_OPTIONS_MASK_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_SSL_ERROR_OPTIONS_MASK_H_

namespace security_interstitials {

enum SSLErrorOptionsMask {
  // Indicates that the error UI should support dismissing the error and
  // loading the page. By default, the errors cannot be overridden via the UI.
  SOFT_OVERRIDE_ENABLED = 1 << 0,
  // Indicates that the user should NOT be allowed to use a "secret code" to
  // dismiss the error and load the page, even if the UI does not support it.
  // By default, an error can be overridden via the "secret code."
  HARD_OVERRIDE_DISABLED = 1 << 1,
  // Indicates that the site the user is trying to connect to has requested
  // strict enforcement of certificate validation (e.g. with HTTP
  // Strict-Transport-Security). By default, the error assumes strict
  // enforcement was not requested.
  STRICT_ENFORCEMENT = 1 << 2,
};

// Calculates a mask encoded via the SSLErrorOptionsMaskFlag bitfields based
// on the passed-in parameters.
int CalculateSSLErrorOptionsMask(int cert_error,
                                 bool hard_override_disabled,
                                 bool should_ssl_errors_be_fatal);
}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_SSL_ERROR_OPTIONS_MASK_H_
