// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/access_token_restriction.h"

#include "build/build_config.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "pdf/buildflags.h"  // nogncheck
#endif                       // !BUILDFLAG(IS_FUCHSIA)

namespace {
struct AccessTokenRestrictionTestParam {
  std::string scope;
  signin::OAuth2ScopeRestriction restriction;
};

using AccessTokenRestrictionParamTest =
    ::testing::TestWithParam<AccessTokenRestrictionTestParam>;

using signin::OAuth2ScopeRestriction;

// clang-format off
const AccessTokenRestrictionTestParam kTestParams[] = {
 // keep-sorted start
 {GaiaConstants::kAnyApiOAuth2Scope, OAuth2ScopeRestriction::kPrivilegedOAuth2Consumer},
 {GaiaConstants::kGoogleUserInfoEmail, OAuth2ScopeRestriction::kNoRestriction},
 {GaiaConstants::kGoogleUserInfoProfile, OAuth2ScopeRestriction::kNoRestriction},
 {GaiaConstants::kOAuth1LoginScope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kSecureConnectOAuth2Scope, OAuth2ScopeRestriction::kNoRestriction},
 // keep-sorted end
 {GaiaConstants::kDeviceManagementServiceOAuth,
#if BUILDFLAG(IS_ANDROID)
  OAuth2ScopeRestriction::kNoRestriction
#else
  OAuth2ScopeRestriction::kSignedIn
#endif
 },
#if !BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
 {GaiaConstants::kDriveOAuth2Scope, OAuth2ScopeRestriction::kNoRestriction},
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
#endif  // !BUILDFLAG(IS_FUCHSIA)
};
// clang-format on

TEST_P(AccessTokenRestrictionParamTest, VerifyScope) {
  AccessTokenRestrictionTestParam test_case = GetParam();
  EXPECT_EQ(signin::GetOAuth2ScopeRestriction(test_case.scope),
            test_case.restriction);
}

INSTANTIATE_TEST_SUITE_P(,
                         AccessTokenRestrictionParamTest,
                         ::testing::ValuesIn(kTestParams));

TEST(AccessTokenRestrictionTest, PrivilegedOAuth2Consumer) {
  EXPECT_TRUE(signin::IsPrivilegedOAuth2Consumer(
      signin::OAuthConsumerId::kExtensionsIdentityAPI));
  EXPECT_FALSE(signin::IsPrivilegedOAuth2Consumer(
      signin::OAuthConsumerId::kSyncDeviceStatisticsMetrics));
  EXPECT_FALSE(signin::IsPrivilegedOAuth2Consumer(
      signin::OAuthConsumerId::kOptimizationGuideGetHints));
}

TEST(AccessTokenRestrictionTest, ConsumerAllowlistedForScope) {
  EXPECT_TRUE(signin::IsConsumerAllowlistedForScope(
      signin::OAuthConsumerId::kSyncDeviceStatisticsMetrics,
      GaiaConstants::kChromeSyncOAuth2Scope));

  // Same consumer, different scope.
  EXPECT_FALSE(signin::IsConsumerAllowlistedForScope(
      signin::OAuthConsumerId::kSyncDeviceStatisticsMetrics,
      GaiaConstants::kGoogleUserInfoEmail));

  // Different consumer, same scope.
  EXPECT_FALSE(signin::IsConsumerAllowlistedForScope(
      signin::OAuthConsumerId::kOptimizationGuideGetHints,
      GaiaConstants::kChromeSyncOAuth2Scope));
}

}  // namespace
