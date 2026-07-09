// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/one_time_token.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

TEST(OneTimeToken, SmsOtpConstructor) {
  base::TimeTicks now = base::TimeTicks::Now();
  OneTimeToken token(OneTimeTokenType::kSmsOtp, "value", now);
  EXPECT_EQ(token.type(), OneTimeTokenType::kSmsOtp);
  EXPECT_EQ(token.value(), "value");
  EXPECT_EQ(token.on_device_arrival_time(), now);
  EXPECT_EQ(token.sender_address(), std::nullopt);
}

TEST(OneTimeToken, GmailOtpConstructor) {
  base::TimeTicks now = base::TimeTicks::Now();
  OneTimeToken token(OneTimeTokenType::kGmail, "value", now,
                     "no_reply@example.com");
  EXPECT_EQ(token.type(), OneTimeTokenType::kGmail);
  EXPECT_EQ(token.value(), "value");
  EXPECT_EQ(token.on_device_arrival_time(), now);
  EXPECT_EQ(token.sender_address(), "no_reply@example.com");
}

TEST(OneTimeToken, Equality) {
  base::TimeTicks now = base::TimeTicks::Now();
  OneTimeToken token1(OneTimeTokenType::kGmail, "value", now, "example.com");
  OneTimeToken token2(OneTimeTokenType::kGmail, "value", now, "example.com");
  OneTimeToken token3(OneTimeTokenType::kGmail, "value", now, "other.com");
  OneTimeToken token4(OneTimeTokenType::kSmsOtp, "value", now, std::nullopt);
  EXPECT_EQ(token1, token2);
  EXPECT_NE(token1, token3);
  EXPECT_NE(token1, token4);
}

TEST(OneTimeToken, IsPotentialOtp) {
  EXPECT_TRUE(OneTimeToken::IsPotentialOtp(u"1234"));
  EXPECT_TRUE(OneTimeToken::IsPotentialOtp(u"12345"));
  EXPECT_TRUE(OneTimeToken::IsPotentialOtp(u"123456"));

  EXPECT_FALSE(OneTimeToken::IsPotentialOtp(u""));
  EXPECT_FALSE(OneTimeToken::IsPotentialOtp(u"1"));
  EXPECT_FALSE(OneTimeToken::IsPotentialOtp(u"12"));
  EXPECT_FALSE(OneTimeToken::IsPotentialOtp(u"123"));
  EXPECT_FALSE(OneTimeToken::IsPotentialOtp(u"1234567"));
  EXPECT_FALSE(OneTimeToken::IsPotentialOtp(u"abcde"));
  EXPECT_FALSE(OneTimeToken::IsPotentialOtp(u"1234a"));
}

}  // namespace one_time_tokens
