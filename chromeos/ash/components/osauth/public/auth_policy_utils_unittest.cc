// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "auth_policy_utils.h"

#include <optional>

#include "base/values.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(AuthPolicyUtilsTest, EmptyValueReturnsEmptySet) {
  base::Value::List policy_allowed_auth_factors;
  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(&policy_allowed_auth_factors),
            AuthFactorsSet());
}

TEST(AuthPolicyUtilsTest, NullPtrReturnsNullOpt) {
  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(nullptr), std::nullopt);
}

TEST(AuthPolicyUtilsTest, AllValueReturnsAllFactors) {
  auto policy_allowed_auth_factors = base::Value::List().Append("ALL");

  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(&policy_allowed_auth_factors),
            AuthFactorsSet({AshAuthFactor::kCryptohomePin,
                            AshAuthFactor::kLocalPassword}));
}

TEST(AuthPolicyUtilsTest, LocalPasswordReturnsLocalPassword) {
  auto policy_allowed_auth_factors =
      base::Value::List().Append("LOCAL_PASSWORD");

  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(&policy_allowed_auth_factors),
            AuthFactorsSet({AshAuthFactor::kLocalPassword}));
}

TEST(AuthPolicyUtilsTest, PINReturnsPIN) {
  auto policy_allowed_auth_factors = base::Value::List().Append("PIN");

  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(&policy_allowed_auth_factors),
            AuthFactorsSet({AshAuthFactor::kCryptohomePin}));
}

TEST(AuthPolicyUtilsTest, MultipleFactorsReturnMultipleFactors) {
  base::Value::List policy_allowed_auth_factors =
      base::Value::List().Append("LOCAL_PASSWORD").Append("PIN");

  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(&policy_allowed_auth_factors),
            AuthFactorsSet({AshAuthFactor::kCryptohomePin,
                            AshAuthFactor::kLocalPassword}));
}

TEST(AuthPolicyUtilsTest, MultipleFactorsWithAllReturnAll) {
  base::Value::List policy_allowed_auth_factors =
      base::Value::List().Append("LOCAL_PASSWORD").Append("PIN").Append("ALL");

  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(&policy_allowed_auth_factors),
            AuthFactorsSet({AshAuthFactor::kCryptohomePin,
                            AshAuthFactor::kLocalPassword}));
}

}  // namespace ash
