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
      "duitnow://paynet.com.my?path=fake_path",
      "shopeepay://shopeepay.com.my?path=fake_path",
      "tngd://tngdigital.com.my?path=fake_path"};

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
  EXPECT_FALSE(validator.IsValid("tngd://TNGDIGITAL.COM.MY/abc1234"));
}

}  // namespace
}  // namespace payments::facilitated
