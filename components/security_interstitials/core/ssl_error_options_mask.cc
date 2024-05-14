// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/ssl_error_options_mask.h"

#include "base/notreached.h"
#include "net/base/net_errors.h"

namespace security_interstitials {

namespace {

int IsCertErrorFatal(int cert_error) {
  switch (cert_error) {
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
    case net::ERR_CERT_NO_REVOCATION_MECHANISM:
    case net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
    case net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
    case net::ERR_CERT_NON_UNIQUE_NAME:
    case net::ERR_CERT_WEAK_KEY:
    case net::ERR_CERT_NAME_CONSTRAINT_VIOLATION:
    case net::ERR_CERT_VALIDITY_TOO_LONG:
    case net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED:
    case net::ERR_CERT_SYMANTEC_LEGACY:
    case net::ERR_CERT_KNOWN_INTERCEPTION_BLOCKED:
      return false;
    case net::ERR_CERT_CONTAINS_ERRORS:
    case net::ERR_CERT_REVOKED:
    case net::ERR_CERT_INVALID:
    case net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN:
      return true;
    default:
      NOTREACHED_IN_MIGRATION();
      return true;
  }
}

}  // namespace

int CalculateSSLErrorOptionsMask(int cert_error,
                                 bool hard_override_disabled,
                                 bool should_ssl_errors_be_fatal) {
  int options_mask = 0;
  if (!IsCertErrorFatal(cert_error) && !hard_override_disabled &&
      !should_ssl_errors_be_fatal) {
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED;
  }
  if (hard_override_disabled) {
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::HARD_OVERRIDE_DISABLED;
  }
  if (should_ssl_errors_be_fatal) {
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::STRICT_ENFORCEMENT;
  }
  return options_mask;
}

}  // namespace security_interstitials
