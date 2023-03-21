// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_registration_verifier.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/driver/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class TrustedVaultRegistrationVerifierTest : public testing::Test {
 protected:
  TrustedVaultRegistrationVerifierTest()
      : account_info_(identity_environment_.MakePrimaryAccountAvailable(
            "test@gmail.com",
            signin::ConsentLevel::kSignin)),
        verifier_(
            identity_environment_.identity_manager(),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    identity_environment_.SetAutomaticIssueOfAccessTokens(true);
  }

  ~TrustedVaultRegistrationVerifierTest() override = default;

  bool RespondToGetSecurityDomainMemberRequest(
      const std::vector<uint8_t>& public_key,
      net::HttpStatusCode response_http_code) {
    // Allow request to reach |test_url_loader_factory_|.
    task_environment_.RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullGetSecurityDomainMemberURLForTesting(
            ExtractTrustedVaultServiceURLFromCommandLine(), public_key)
            .spec(),
        /*content=*/std::string(), response_http_code);
  }

  // Arbitrary key, with an appropriate length.
  const std::vector<uint8_t> kTestPublicKey{
      0x4,  0xF2, 0x4C, 0x45, 0xBA, 0xF4, 0xF8, 0x6C, 0xF9, 0x73, 0xCE,
      0x75, 0xC,  0xC9, 0xD4, 0xF,  0x4A, 0x53, 0xB7, 0x85, 0x46, 0x41,
      0xFB, 0x31, 0x17, 0xF,  0xEB, 0xB,  0x45, 0xE4, 0x29, 0x69, 0x9B,
      0xB2, 0x7,  0x12, 0xC1, 0x9,  0x3D, 0xEF, 0xBB, 0x57, 0xDC, 0x56,
      0x12, 0x29, 0xF2, 0x73, 0xE1, 0xC5, 0x99, 0x1C, 0x49, 0x3A, 0xA2,
      0x30, 0xF9, 0xBA, 0x3B, 0xB1, 0x83, 0xCF, 0x1B, 0x5D, 0xE8};

  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_environment_;
  const AccountInfo account_info_;
  TrustedVaultRegistrationVerifier verifier_;
};

TEST_F(TrustedVaultRegistrationVerifierTest, ShouldReportExistingRegistration) {
  base::HistogramTester histogram_tester;
  verifier_.VerifyMembership(account_info_.gaia, kTestPublicKey);

  EXPECT_TRUE(
      RespondToGetSecurityDomainMemberRequest(kTestPublicKey, net::HTTP_OK));

  histogram_tester.ExpectUniqueSample(
      /*name=*/"Sync.TrustedVaultVerifyDeviceRegistrationStateV1",
      /*sample=*/
      static_cast<int>(
          TrustedVaultDownloadKeysStatusForUMA::kKeyProofsVerificationFailed),
      /*expected_bucket_count=*/1);
}

TEST_F(TrustedVaultRegistrationVerifierTest, ShouldReportMissingRegistration) {
  base::HistogramTester histogram_tester;
  verifier_.VerifyMembership(account_info_.gaia, kTestPublicKey);

  EXPECT_TRUE(RespondToGetSecurityDomainMemberRequest(kTestPublicKey,
                                                      net::HTTP_NOT_FOUND));

  histogram_tester.ExpectUniqueSample(
      /*name=*/"Sync.TrustedVaultVerifyDeviceRegistrationStateV1",
      /*sample=*/
      static_cast<int>(TrustedVaultDownloadKeysStatusForUMA::kMemberNotFound),
      /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace syncer
