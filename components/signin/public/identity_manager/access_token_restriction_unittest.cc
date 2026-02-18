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
 {GaiaConstants::kAccountsReauthOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kAnyApiOAuth2Scope, OAuth2ScopeRestriction::kPrivilegedOAuth2Consumer},
 {GaiaConstants::kCalendarReadOnlyOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kChromeSafeBrowsingOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kChromeSyncOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kChromeSyncSupervisedOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kClassifyUrlKidPermissionOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kCloudSearchQueryOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kCryptAuthOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kDiscoveryEngineCompleteQueryOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kDriveReadOnlyOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kFCMOAuthScope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kFeedOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kGoogleTalkOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kGoogleUserInfoEmail, OAuth2ScopeRestriction::kNoRestriction},
 {GaiaConstants::kGoogleUserInfoProfile, OAuth2ScopeRestriction::kNoRestriction},
 {GaiaConstants::kIpProtectionAuthScope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kKAnonymityServiceOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kKidFamilyReadonlyOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kKidManagementPrivilegedOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kKidsSupervisionSetupChildOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kOAuth1LoginScope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kOptimizationGuideServiceGetHintsOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kOptimizationGuideServiceModelExecutionOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kParentApprovalOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPasskeysEnclaveOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPasswordsLeakCheckOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPaymentsOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPhotosModuleImageOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPhotosModuleOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kProgrammaticChallengeOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kSecureConnectOAuth2Scope, OAuth2ScopeRestriction::kNoRestriction},
 {GaiaConstants::kSupportContentOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 // keep-sorted end
 {GaiaConstants::kDeviceManagementServiceOAuth,
#if BUILDFLAG(IS_ANDROID)
  OAuth2ScopeRestriction::kNoRestriction
#else
  OAuth2ScopeRestriction::kSignedIn
#endif
 },
#if BUILDFLAG(IS_CHROMEOS)
 // keep-sorted start
 {GaiaConstants::kAssistantOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kAuditRecordingOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kCastBackdropOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kClearCutOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kContactsOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kExperimentsAndConfigsOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kGCMGroupServerOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kNearbyDevicesOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kNearbyPresenceOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kNearbyShareOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPeopleApiReadOnlyOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPhotosOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kTachyonOAuthScope, OAuth2ScopeRestriction::kSignedIn},
 // keep-sorted end
#endif  // BUILDFLAG(IS_CHROMEOS)
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
  EXPECT_TRUE(signin::IsPrivilegedOAuth2Consumer(
      signin::OAuthConsumerId::kSyncDeviceStatisticsMetrics));
  EXPECT_FALSE(signin::IsPrivilegedOAuth2Consumer(
      signin::OAuthConsumerId::kIpProtectionService));
}

}  // namespace
