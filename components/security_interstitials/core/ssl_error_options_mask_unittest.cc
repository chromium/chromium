// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/ssl_error_options_mask.h"

#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace security_interstitials {

TEST(SSLErrorOptionsMask, CalculateSSLErrorOptionsMask) {
  int mask;

  // Non-overridable cert error.
  mask = CalculateSSLErrorOptionsMask(
      net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN, /* cert_error */
      false,                                     /* hard_override_disabled */
      false /* should_ssl_errors_be_fatal */
  );
  EXPECT_EQ(0, mask);
  mask = CalculateSSLErrorOptionsMask(
      net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN, /* cert_error */
      true,                                      /* hard_override_disabled */
      false /* should_ssl_errors_be_fatal */
  );
  EXPECT_EQ(SSLErrorOptionsMask::HARD_OVERRIDE_DISABLED, mask);
  mask = CalculateSSLErrorOptionsMask(
      net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN, /* cert_error */
      false,                                     /* hard_override_disabled */
      true /* should_ssl_errors_be_fatal */
  );
  EXPECT_EQ(SSLErrorOptionsMask::STRICT_ENFORCEMENT, mask);

  // Overridable cert error.
  mask =
      CalculateSSLErrorOptionsMask(net::ERR_CERT_DATE_INVALID, /* cert_error */
                                   false, /* hard_override_disabled */
                                   false  /* should_ssl_errors_be_fatal */
      );
  EXPECT_EQ(SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED, mask);
  mask =
      CalculateSSLErrorOptionsMask(net::ERR_CERT_DATE_INVALID, /* cert_error */
                                   true, /* hard_override_disabled */
                                   false /* should_ssl_errors_be_fatal */
      );
  EXPECT_EQ(SSLErrorOptionsMask::HARD_OVERRIDE_DISABLED, mask);
  mask =
      CalculateSSLErrorOptionsMask(net::ERR_CERT_DATE_INVALID, /* cert_error */
                                   false, /* hard_override_disabled */
                                   true   /* should_ssl_errors_be_fatal */
      );
  EXPECT_EQ(SSLErrorOptionsMask::STRICT_ENFORCEMENT, mask);
}

}  // namespace security_interstitials
