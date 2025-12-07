// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/one_time_token.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

TEST(OneTimeToken, Constructor) {
  base::Time now = base::Time::Now();
  OneTimeToken token(OneTimeTokenType::kSmsOtp, "value", now);
  EXPECT_EQ(token.type(), OneTimeTokenType::kSmsOtp);
  EXPECT_EQ(token.value(), "value");
  EXPECT_EQ(token.on_device_arrival_time(), now);
}

}  // namespace one_time_tokens
