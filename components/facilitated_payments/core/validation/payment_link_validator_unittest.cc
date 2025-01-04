// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/validation/payment_link_validator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

TEST(PaymentLinkValidatorTest, validUrls) {
  PaymentLinkValidator validator;
  const std::vector<GURL> kValidUrls = {
      GURL("duitnow://paynet.com.my?path=fake_path"),
      GURL("shopeepay://shopeepay.com.my?path=fake_path"),
      GURL("tngd://tngdigital.com.my?path=fake_path")};

  for (const auto& link : kValidUrls) {
    EXPECT_NE(validator.GetScheme(link), PaymentLinkValidator::Scheme::kInvalid)
        << "Failed for: " << link.spec();
  }
}

TEST(PaymentLinkValidatorTest, InvalidUrls) {
  PaymentLinkValidator validator;
  const std::vector<GURL> kInvalidUrls = {
      GURL("duitnow://invalid.com"), GURL("https://www.google.com"),
      GURL("shopeepay://wrongdomain.com/order"),
      GURL("duitnow://")  // Empty after scheme
  };

  for (const auto& link : kInvalidUrls) {
    EXPECT_EQ(validator.GetScheme(link), PaymentLinkValidator::Scheme::kInvalid)
        << "Failed for: " << link.spec();
  }
}

// Additional Tests (consider edge cases)
TEST(PaymentLinkValidatorTest, EmptyLink) {
  PaymentLinkValidator validator;
  GURL link("");
  EXPECT_EQ(validator.GetScheme(link), PaymentLinkValidator::Scheme::kInvalid);
}

TEST(PaymentLinkValidatorTest, CaseSensitive) {
  PaymentLinkValidator validator;
  GURL link("tngd://TNGDIGITAL.COM.MY/abc1234");
  EXPECT_EQ(validator.GetScheme(link), PaymentLinkValidator::Scheme::kInvalid);
}

}  // namespace
}  // namespace payments::facilitated
