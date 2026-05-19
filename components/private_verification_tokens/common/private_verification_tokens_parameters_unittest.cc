// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace private_verification_tokens {

TEST(PrivateVerificationTokensParametersTest, GetParametersForVersion1) {
  auto params = GetParametersForVersion(1);
  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(params->min_batch_size, 2);
  EXPECT_EQ(params->max_batch_size, 20);
}

TEST(PrivateVerificationTokensParametersTest, GetParametersForUnknownVersion) {
  auto params = GetParametersForVersion(2);
  EXPECT_FALSE(params.has_value());
}

}  // namespace private_verification_tokens
