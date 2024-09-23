// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/util/payment_link_validator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

TEST(PaymentLinkValidatorTest, validUrls) {
  PaymentLinkValidator validator;
  const std::vector<std::string> kValidUrls = {
      "duitnow://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd",
      "duitnow://tngdigital.com.my?code=https://qr.tngdigital.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd",
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd",
      "tngditial://tngdigital.com.my?code=https://qr.tngdigital.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd"};

  for (const auto& link : kValidUrls) {
    EXPECT_TRUE(validator.IsValid(link)) << "Failed for: " << link;
  }
}

TEST(PaymentLinkValidatorTest, InvalidUrls) {
  PaymentLinkValidator validator;
  const std::vector<std::string> kInvalidUrls = {
      "duitnow://invalid.com", "https://www.google.com",
      "shopeepay://wrongdomain.com/order",
      "duitnow://"  // Empty after scheme
  };

  for (const auto& link : kInvalidUrls) {
    EXPECT_FALSE(validator.IsValid(link)) << "Failed for: " << link;
  }
}

// Additional Tests (consider edge cases)
TEST(PaymentLinkValidatorTest, EmptyLink) {
  PaymentLinkValidator validator;
  EXPECT_FALSE(validator.IsValid(""));
}

TEST(PaymentLinkValidatorTest, CaseSensitive) {
  PaymentLinkValidator validator;
  EXPECT_FALSE(validator.IsValid("duitnow://TNGDIGITAL.COM.MY/abc1234"));
}

}  // namespace
}  // namespace payments::facilitated
