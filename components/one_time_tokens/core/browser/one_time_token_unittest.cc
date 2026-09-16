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
  EXPECT_EQ(token.email_received_timestamp(), std::nullopt);
}

TEST(OneTimeToken, GmailOtpConstructor) {
  base::TimeTicks now_ticks = base::TimeTicks::Now();
  base::Time now_time = base::Time::Now();
  OneTimeToken token(OneTimeTokenType::kGmail, "value", now_ticks,
                     "no_reply@example.com", now_time);
  EXPECT_EQ(token.type(), OneTimeTokenType::kGmail);
  EXPECT_EQ(token.value(), "value");
  EXPECT_EQ(token.on_device_arrival_time(), now_ticks);
  EXPECT_EQ(token.sender_address(), "no_reply@example.com");
  EXPECT_EQ(token.email_received_timestamp(), now_time);
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
