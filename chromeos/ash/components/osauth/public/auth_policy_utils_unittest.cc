// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "auth_policy_utils.h"

#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "base/values.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class AuthPolicyUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    pref_service_.registry()->RegisterListPref(prefs::kAllowedLocalAuthFactors);
  }

  void SetAllowedFactors(const base::ListValue& factors) {
    pref_service_.SetUserPref(prefs::kAllowedLocalAuthFactors,
                              base::Value(factors.Clone()));
  }

  TestingPrefServiceSimple pref_service_;
};

TEST_F(AuthPolicyUtilsTest, EmptyValueReturnsEmptySet) {
  base::ListValue policy_allowed_auth_factors;
  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(&policy_allowed_auth_factors),
            AuthFactorsSet());
}

TEST_F(AuthPolicyUtilsTest, NullPtrReturnsNullOpt) {
  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(nullptr), std::nullopt);
}

TEST_F(AuthPolicyUtilsTest, AllValueReturnsAllFactors) {
  auto policy_allowed_auth_factors = base::ListValue().Append("ALL");

  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(&policy_allowed_auth_factors),
            AuthFactorsSet({AshAuthFactor::kCryptohomePin,
                            AshAuthFactor::kLocalPassword}));
}

TEST_F(AuthPolicyUtilsTest, LocalPasswordReturnsLocalPassword) {
  auto policy_allowed_auth_factors = base::ListValue().Append("LOCAL_PASSWORD");

  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(&policy_allowed_auth_factors),
            AuthFactorsSet({AshAuthFactor::kLocalPassword}));
}

TEST_F(AuthPolicyUtilsTest, PINReturnsPIN) {
  auto policy_allowed_auth_factors = base::ListValue().Append("PIN");

  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(&policy_allowed_auth_factors),
            AuthFactorsSet({AshAuthFactor::kCryptohomePin}));
}

TEST_F(AuthPolicyUtilsTest, MultipleFactorsReturnMultipleFactors) {
  base::ListValue policy_allowed_auth_factors =
      base::ListValue().Append("LOCAL_PASSWORD").Append("PIN");

  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(&policy_allowed_auth_factors),
            AuthFactorsSet({AshAuthFactor::kCryptohomePin,
                            AshAuthFactor::kLocalPassword}));
}

TEST_F(AuthPolicyUtilsTest, MultipleFactorsWithAllReturnAll) {
  base::ListValue policy_allowed_auth_factors =
      base::ListValue().Append("LOCAL_PASSWORD").Append("PIN").Append("ALL");

  EXPECT_EQ(GetAuthFactorsSetFromPolicyList(&policy_allowed_auth_factors),
            AuthFactorsSet({AshAuthFactor::kCryptohomePin,
                            AshAuthFactor::kLocalPassword}));
}

TEST_F(AuthPolicyUtilsTest, PinEnabledWhenListed) {
  base::ListValue factors =
      base::ListValue().Append("PIN").Append("LOCAL_PASSWORD");
  SetAllowedFactors(factors);
  EXPECT_TRUE(IsPinEnabledAsMainFactorByPolicy(&pref_service_));
}

TEST_F(AuthPolicyUtilsTest, PinEnabledWhenAll) {
  base::ListValue factors = base::ListValue().Append("ALL");
  SetAllowedFactors(factors);
  EXPECT_TRUE(IsPinEnabledAsMainFactorByPolicy(&pref_service_));
}

TEST_F(AuthPolicyUtilsTest, PinDisabledWhenNotListed) {
  base::ListValue factors = base::ListValue().Append("LOCAL_PASSWORD");
  SetAllowedFactors(factors);
  EXPECT_FALSE(IsPinEnabledAsMainFactorByPolicy(&pref_service_));
}

TEST_F(AuthPolicyUtilsTest, PinDisabledWhenEmptyList) {
  base::ListValue factors;
  SetAllowedFactors(factors);
  EXPECT_FALSE(IsPinEnabledAsMainFactorByPolicy(&pref_service_));
}

}  // namespace ash
