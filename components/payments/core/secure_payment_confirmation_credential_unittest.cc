// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/secure_payment_confirmation_credential.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Regression test for https://crbug.com/1122764#c10 - ensure we are recording
// the correct credential ID size.
TEST(SecurePaymentConfirmationCredentialTest, CredentialIdSizeHistogram) {
  const std::string rp_id = "relyingparty.com";
  const std::vector<uint8_t> user_id = {'u', 's', 'e', 'r'};

  base::HistogramTester histogram_tester;

  std::vector<uint8_t> credential_id(8);
  SecurePaymentConfirmationCredential i1(credential_id, rp_id, user_id);
  credential_id = std::vector<uint8_t>(64);
  SecurePaymentConfirmationCredential i2(credential_id, rp_id, user_id);
  credential_id = std::vector<uint8_t>(120);
  SecurePaymentConfirmationCredential i3(credential_id, rp_id, user_id);
  credential_id = std::vector<uint8_t>(1024);
  SecurePaymentConfirmationCredential i4(credential_id, rp_id, user_id);
  credential_id = std::vector<uint8_t>(9000);
  SecurePaymentConfirmationCredential i5(credential_id, rp_id, user_id);

  const std::string kHistogram =
      "PaymentRequest.SecurePaymentConfirmationCredentialIdSizeInBytes";
  histogram_tester.ExpectTotalCount(kHistogram, 5);
  histogram_tester.ExpectBucketCount(kHistogram, 8, 1);
  histogram_tester.ExpectBucketCount(kHistogram, 64, 1);
  histogram_tester.ExpectBucketCount(kHistogram, 120, 1);
  histogram_tester.ExpectBucketCount(kHistogram, 1024, 1);
  histogram_tester.ExpectBucketCount(kHistogram, 9000, 1);
}

}  // namespace payments
