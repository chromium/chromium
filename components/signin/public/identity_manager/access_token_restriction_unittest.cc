// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/access_token_restriction.h"

#include "build/chromeos_buildflags.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

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
 {GaiaConstants::kGoogleUserInfoEmail, OAuth2ScopeRestriction::kNoRestriction},
 {GaiaConstants::kGoogleUserInfoProfile, OAuth2ScopeRestriction::kNoRestriction},
 {GaiaConstants::kDeviceManagementServiceOAuth, OAuth2ScopeRestriction::kNoRestriction},
 {GaiaConstants::kSecureConnectOAuth2Scope, OAuth2ScopeRestriction::kNoRestriction},
 {GaiaConstants::kFCMOAuthScope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPaymentsOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPasswordsLeakCheckOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kCryptAuthOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kChromeSafeBrowsingOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kChromeSyncOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kClassifyUrlKidPermissionOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kIpProtectionAuthScope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kSupportContentOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPhotosModuleOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPhotosModuleImageOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kKidFamilyReadonlyOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kFeedOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kKAnonymityServiceOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kAccountsReauthOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPasskeysEnclaveOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kOptimizationGuideServiceGetHintsOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kOptimizationGuideServiceModelExecutionOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kCloudSearchQueryOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kOAuth1LoginScope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kCalendarReadOnlyOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
#if BUILDFLAG(IS_CHROMEOS_ASH)
 {GaiaConstants::kAssistantOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kAuditRecordingOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kCastBackdropOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kClearCutOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kDriveOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kDriveReadOnlyOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kExperimentsAndConfigsOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kGCMGroupServerOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kNearbyDevicesOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kNearbyShareOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kNearbyPresenceOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPeopleApiReadOnlyOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kPhotosOAuth2Scope, OAuth2ScopeRestriction::kSignedIn},
 {GaiaConstants::kTachyonOAuthScope, OAuth2ScopeRestriction::kSignedIn},
 #endif  // BUILDFLAG(IS_CHROMEOS_ASH)
 {GaiaConstants::kAnyApiOAuth2Scope, OAuth2ScopeRestriction::kPrivilegedOAuth2Consumer},
 {GaiaConstants::kChromeSyncSupervisedOAuth2Scope, OAuth2ScopeRestriction::kExplicitConsent},
 {GaiaConstants::kKidManagementPrivilegedOAuth2Scope, OAuth2ScopeRestriction::kExplicitConsent},
 {GaiaConstants::kKidsSupervisionSetupChildOAuth2Scope, OAuth2ScopeRestriction::kExplicitConsent},
 {GaiaConstants::kGoogleTalkOAuth2Scope, OAuth2ScopeRestriction::kExplicitConsent},
 {GaiaConstants::kParentApprovalOAuth2Scope, OAuth2ScopeRestriction::kExplicitConsent},
 {GaiaConstants::kProgrammaticChallengeOAuth2Scope, OAuth2ScopeRestriction::kExplicitConsent}
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
  EXPECT_TRUE(signin::IsPrivilegedOAuth2Consumer("extensions_identity_api"));
  EXPECT_FALSE(signin::IsPrivilegedOAuth2Consumer("IpProtectionService"));
}

}  // namespace
