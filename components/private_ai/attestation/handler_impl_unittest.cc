// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/attestation/handler_impl.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/private_ai/attestation/server_verification_key.h"
#include "components/private_ai/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

TEST(AttestationHandlerImplTest, GetAttestationRequest) {
  AttestationHandlerImpl attestation_handler;
  auto request = attestation_handler.GetAttestationRequest();
  ASSERT_TRUE(request.has_value());
  EXPECT_TRUE(request->assertions().empty());
  EXPECT_TRUE(request->endorsed_evidence().empty());
}

const char kTestEvidenceId[] = "95553023-358f-4f8c-b75c-e6e185cc05ca";
const char kTestMessageHex[] =
    "0A1C0A0C0890FDDBCA0610B1B68A9503120C0890E7A5CB0610B1B68A950312550A4104"
    "D074CF4D1BA45C2330F0A997BB9E433B42D486EC06C8BB8398B9B0040B53C7A2C7AE73"
    "D7D01DD02DAB831827D827A738A75D77271870B26AD23DF1C50DEB3BEA12100901029F"
    "D4074410B74D7F4EC3CFF4ED1A480802109CB2611A40BBD18F7746F0AEF01A35EEE28D"
    "FEB3B10863839FD57402C2394A7A5F20BD508288BC49F4AE06027D97266E68B5F1B3F9"
    "8A498C22C13F92431854EC1E5E1DED8E";
const char kTestSignatureHex[] =
    "00AD190D7B3046022100B195114B54AA390E3FB3EBD703524ACAE4585E5286C8996143"
    "88758901DBC405022100FEB2047C7F154C1F408442B9D1CE63E2E1F6AEF08D8317DDF8"
    "A326E4EE9F6D88";

class VerifyAttestationResponseTest : public ::testing::Test {
 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(VerifyAttestationResponseTest, Success) {
  base::FieldTrialParams params;
  params["url"] = "staging-private-ai.corp.google.com";
  feature_list_.InitWithFeaturesAndParameters(
      {{kPrivateAi, params}, {kPrivateAiServerAttestation, {}}}, {});

  AttestationHandlerImpl attestation_handler;

  AttestationEvidence evidence;
  {
    EndorsedEvidence endorsed_evidence;
    Endorsement endorsement;
    ASSERT_TRUE(base::HexStringToBytes(kTestMessageHex, &endorsement.message));
    ASSERT_TRUE(
        base::HexStringToBytes(kTestSignatureHex, &endorsement.signature));
    endorsed_evidence.endorsements.push_back(std::move(endorsement));
    evidence.endorsed_evidence[kTestEvidenceId] = std::move(endorsed_evidence);
  }

  EXPECT_TRUE(attestation_handler.VerifyAttestationResponse(evidence));
}

TEST_F(VerifyAttestationResponseTest, ServerAttestationDisabled) {
  feature_list_.InitAndDisableFeature(kPrivateAiServerAttestation);

  AttestationHandlerImpl attestation_handler;

  // With the feature disabled, verification should be skipped and return true,
  // even with empty evidence.
  AttestationEvidence evidence;
  EXPECT_TRUE(attestation_handler.VerifyAttestationResponse(evidence));
}

TEST_F(VerifyAttestationResponseTest, EmptyEvidence) {
  feature_list_.InitAndEnableFeature(kPrivateAiServerAttestation);
  AttestationHandlerImpl attestation_handler(
      LoadVerificationKeys(GetStagingKeysForTesting()));

  AttestationEvidence evidence;
  EXPECT_FALSE(attestation_handler.VerifyAttestationResponse(evidence));
}

TEST_F(VerifyAttestationResponseTest, EmptyEndorsements) {
  feature_list_.InitAndEnableFeature(kPrivateAiServerAttestation);
  AttestationHandlerImpl attestation_handler(
      LoadVerificationKeys(GetStagingKeysForTesting()));

  AttestationEvidence evidence;
  evidence.endorsed_evidence["123"] = EndorsedEvidence();
  EXPECT_FALSE(attestation_handler.VerifyAttestationResponse(evidence));
}

TEST_F(VerifyAttestationResponseTest, MalformedSignature) {
  feature_list_.InitAndEnableFeature(kPrivateAiServerAttestation);
  AttestationHandlerImpl attestation_handler(
      LoadVerificationKeys(GetStagingKeysForTesting()));

  AttestationEvidence evidence;
  {
    EndorsedEvidence endorsed_evidence;
    Endorsement endorsement;
    endorsement.signature = {0x01, 0x02, 0x03,
                             0x04};  // Too short, tink is 1 byte prefix + 4
                                     // bytes key id
    endorsed_evidence.endorsements.push_back(std::move(endorsement));
    evidence.endorsed_evidence["123"] = std::move(endorsed_evidence);
  }

  EXPECT_FALSE(attestation_handler.VerifyAttestationResponse(evidence));
}

TEST_F(VerifyAttestationResponseTest, VerificationKeysEmpty) {
  feature_list_.InitAndEnableFeature(kPrivateAiServerAttestation);
  // Pass in an empty vector of keys.
  std::map<uint32_t, VerificationKey> empty_keys;
  AttestationHandlerImpl attestation_handler(std::move(empty_keys));

  // Even with valid evidence, the response should be rejected because the
  // keys are empty.
  AttestationEvidence evidence;
  {
    EndorsedEvidence endorsed_evidence;
    Endorsement endorsement;
    ASSERT_TRUE(base::HexStringToBytes(kTestMessageHex, &endorsement.message));
    ASSERT_TRUE(
        base::HexStringToBytes(kTestSignatureHex, &endorsement.signature));
    endorsed_evidence.endorsements.push_back(std::move(endorsement));
    evidence.endorsed_evidence[kTestEvidenceId] = std::move(endorsed_evidence);
  }

  EXPECT_FALSE(attestation_handler.VerifyAttestationResponse(evidence));
}

TEST_F(VerifyAttestationResponseTest, KeyNotFound) {
  feature_list_.InitAndEnableFeature(kPrivateAiServerAttestation);
  AttestationHandlerImpl attestation_handler(
      LoadVerificationKeys(GetStagingKeysForTesting()));

  AttestationEvidence evidence;
  {
    EndorsedEvidence endorsed_evidence;
    Endorsement endorsement;
    // A signature with a key ID that is unlikely to exist (999).
    // Tink prefix (0x01) + key ID (0x000003E7) + dummy 64-byte signature.
    endorsement.signature = {0x01, 0xE7, 0x03, 0x00, 0x00};
    endorsement.signature.insert(endorsement.signature.end(), 64, 0x00);
    endorsed_evidence.endorsements.push_back(std::move(endorsement));
    evidence.endorsed_evidence["123"] = std::move(endorsed_evidence);
  }

  EXPECT_FALSE(attestation_handler.VerifyAttestationResponse(evidence));
}

TEST_F(VerifyAttestationResponseTest, WrongSignature) {
  base::FieldTrialParams params;
  params["url"] = "staging-private-ai.corp.google.com";
  feature_list_.InitWithFeaturesAndParameters(
      {{kPrivateAi, params}, {kPrivateAiServerAttestation, {}}}, {});

  AttestationHandlerImpl attestation_handler;

  AttestationEvidence evidence;
  {
    EndorsedEvidence endorsed_evidence;
    Endorsement endorsement;
    std::string signature_hex = kTestSignatureHex;
    signature_hex.back() = '0';  // Flip a bit to make it invalid.

    ASSERT_TRUE(base::HexStringToBytes(kTestMessageHex, &endorsement.message));
    ASSERT_TRUE(base::HexStringToBytes(signature_hex, &endorsement.signature));
    endorsed_evidence.endorsements.push_back(std::move(endorsement));
    evidence.endorsed_evidence[kTestEvidenceId] = std::move(endorsed_evidence);
  }

  EXPECT_FALSE(attestation_handler.VerifyAttestationResponse(evidence));
}

// Test to cover the VerifyInit failure.
TEST_F(VerifyAttestationResponseTest, VerifyInitFails) {
  feature_list_.InitAndEnableFeature(kPrivateAiServerAttestation);
  std::map<uint32_t, VerificationKey> keys =
      LoadVerificationKeys(GetStagingKeysForTesting());
  ASSERT_FALSE(keys.empty());

  uint32_t key_id = keys.begin()->first;

  VerificationKey& corrupted_key = keys.at(key_id);

  // Corrupt the key in place to cause VerifyInit to fail.
  // Providing an empty public key span should cause EVP_parse_public_key to
  // fail inside VerifyInit.
  corrupted_key.public_key.clear();

  AttestationHandlerImpl attestation_handler(std::move(keys));

  AttestationEvidence evidence;
  {
    EndorsedEvidence endorsed_evidence;
    Endorsement endorsement;
    endorsement.message = {1, 2, 3, 4};

    // Craft a signature header for the corrupted key ID.
    std::vector<uint8_t> signature = {0x01};  // Tink prefix
    signature.push_back((key_id >> 24) & 0xFF);
    signature.push_back((key_id >> 16) & 0xFF);
    signature.push_back((key_id >> 8) & 0xFF);
    signature.push_back(key_id & 0xFF);
    signature.insert(signature.end(), 64, 0xAA);
    endorsement.signature = std::move(signature);

    endorsed_evidence.endorsements.push_back(std::move(endorsement));
    evidence.endorsed_evidence[kTestEvidenceId] = std::move(endorsed_evidence);
  }

  EXPECT_FALSE(attestation_handler.VerifyAttestationResponse(evidence));
}

// Test to ensure the non-LEGACY key type path is taken in VerifyUpdate.
TEST_F(VerifyAttestationResponseTest, ForcedNonLegacyKeyType) {
  feature_list_.InitAndEnableFeature(kPrivateAiServerAttestation);
  base::span<const ProcessedKey> original_keys = GetStagingKeysForTesting();
  ASSERT_FALSE(original_keys.empty());

  ProcessedKey modified_processed_key = original_keys[0];
  modified_processed_key.output_prefix_type = OutputPrefixType::TINK;

  std::vector<ProcessedKey> key_vector = {modified_processed_key};
  std::map<uint32_t, VerificationKey> keys = LoadVerificationKeys(key_vector);
  AttestationHandlerImpl attestation_handler(std::move(keys));

  AttestationEvidence evidence;
  {
    EndorsedEvidence endorsed_evidence;
    Endorsement endorsement;
    endorsement.message = {1, 2, 3, 4};

    endorsement.signature = {0x01};  // Tink prefix
    uint32_t key_id = modified_processed_key.id;
    endorsement.signature.push_back((key_id >> 24) & 0xFF);
    endorsement.signature.push_back((key_id >> 16) & 0xFF);
    endorsement.signature.push_back((key_id >> 8) & 0xFF);
    endorsement.signature.push_back(key_id & 0xFF);
    endorsement.signature.insert(endorsement.signature.end(), 64, 0xAA);

    endorsed_evidence.endorsements.push_back(std::move(endorsement));
    evidence.endorsed_evidence["123"] = std::move(endorsed_evidence);
  }

  // Verification should fail because the signature is invalid
  // and the key is not actually a TINK key.
  // However, the code path will execute the 'else' block as intended.
  EXPECT_FALSE(attestation_handler.VerifyAttestationResponse(evidence));
}

}  // namespace

}  // namespace private_ai
