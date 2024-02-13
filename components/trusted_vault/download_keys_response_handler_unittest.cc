// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/download_keys_response_handler.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_crypto.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;

const char kEncodedPrivateKey[] =
    "49e052293c29b5a50b0013eec9d030ac2ad70a42fe093be084264647cb04e16f";

std::unique_ptr<SecureBoxKeyPair> MakeTestKeyPair() {
  std::vector<uint8_t> private_key_bytes;
  bool success = base::HexStringToBytes(kEncodedPrivateKey, &private_key_bytes);
  DCHECK(success);
  return SecureBoxKeyPair::CreateByPrivateKeyImport(private_key_bytes);
}

void AddSecurityDomainMembership(
    const std::string security_domain_path,
    const SecureBoxPublicKey& member_public_key,
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
    const std::vector<int>& trusted_vault_keys_versions,
    const std::vector<std::vector<uint8_t>>& signing_keys,
    trusted_vault_pb::SecurityDomainMember* member) {
  DCHECK(member);
  DCHECK_EQ(trusted_vault_keys.size(), trusted_vault_keys_versions.size());
  DCHECK_EQ(trusted_vault_keys.size(), signing_keys.size());

  trusted_vault_pb::SecurityDomainMember::SecurityDomainMembership* membership =
      member->add_memberships();
  membership->set_security_domain(std::move(security_domain_path));
  for (size_t i = 0; i < trusted_vault_keys.size(); ++i) {
    trusted_vault_pb::SharedMemberKey* shared_key = membership->add_keys();
    shared_key->set_epoch(trusted_vault_keys_versions[i]);
    AssignBytesToProtoString(
        ComputeTrustedVaultWrappedKey(member_public_key, trusted_vault_keys[i]),
        shared_key->mutable_wrapped_key());

    if (!signing_keys[i].empty()) {
      trusted_vault_pb::RotationProof* rotation_proof =
          membership->add_rotation_proofs();
      rotation_proof->set_new_epoch(trusted_vault_keys_versions[i]);
      AssignBytesToProtoString(ComputeRotationProofForTesting(
                                   /*trusted_vault_key=*/trusted_vault_keys[i],
                                   /*prev_trusted_vault_key=*/signing_keys[i]),
                               rotation_proof->mutable_rotation_proof());
    }
  }
}

std::string CreateGetSecurityDomainMemberResponseWithSyncMembership(
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
    const std::vector<int>& trusted_vault_keys_versions,
    const std::vector<std::vector<uint8_t>>& signing_keys) {
  trusted_vault_pb::SecurityDomainMember member;
  AddSecurityDomainMembership(
      GetSecurityDomainPath(SecurityDomainId::kChromeSync),
      MakeTestKeyPair()->public_key(), trusted_vault_keys,
      trusted_vault_keys_versions, signing_keys, &member);
  return member.SerializeAsString();
}

class DownloadKeysResponseHandlerTest : public testing::Test {
 public:
  DownloadKeysResponseHandlerTest()
      : handler_(SecurityDomainId::kChromeSync,
                 TrustedVaultKeyAndVersion(kKnownTrustedVaultKey,
                                           kKnownTrustedVaultKeyVersion),
                 MakeTestKeyPair()) {}

  ~DownloadKeysResponseHandlerTest() override = default;

  const DownloadKeysResponseHandler& handler() const { return handler_; }

  const int kKnownTrustedVaultKeyVersion = 5;
  const std::vector<uint8_t> kKnownTrustedVaultKey = {1, 2, 3, 4};
  const std::vector<uint8_t> kTrustedVaultKey1 = {1, 2, 3, 5};
  const std::vector<uint8_t> kTrustedVaultKey2 = {1, 2, 3, 6};
  const std::vector<uint8_t> kTrustedVaultKey3 = {1, 2, 3, 7};

 private:
  const DownloadKeysResponseHandler handler_;
};

// All HttpStatuses except kSuccess should end up in kOtherError, kNetworkError
// or kMemberNotFound reporting.
TEST_F(DownloadKeysResponseHandlerTest, ShouldHandleHttpErrors) {
  EXPECT_THAT(
      handler()
          .ProcessResponse(
              /*http_status=*/TrustedVaultRequest::HttpStatus::kNotFound,
              /*response_body=*/std::string())
          .status,
      Eq(TrustedVaultDownloadKeysStatus::kMemberNotFound));
  EXPECT_THAT(
      handler()
          .ProcessResponse(
              /*http_status=*/TrustedVaultRequest::HttpStatus::kBadRequest,
              /*response_body=*/std::string())
          .status,
      Eq(TrustedVaultDownloadKeysStatus::kOtherError));
  EXPECT_THAT(
      handler()
          .ProcessResponse(
              /*http_status=*/TrustedVaultRequest::HttpStatus::kConflict,
              /*response_body=*/std::string())
          .status,
      Eq(TrustedVaultDownloadKeysStatus::kOtherError));
  EXPECT_THAT(
      handler()
          .ProcessResponse(
              /*http_status=*/TrustedVaultRequest::HttpStatus::kNetworkError,
              /*response_body=*/std::string())
          .status,
      Eq(TrustedVaultDownloadKeysStatus::kNetworkError));
  EXPECT_THAT(
      handler()
          .ProcessResponse(
              /*http_status=*/TrustedVaultRequest::HttpStatus::kOtherError,
              /*response_body=*/std::string())
          .status,
      Eq(TrustedVaultDownloadKeysStatus::kOtherError));
}

// Simplest legitimate case of key rotation, server side state corresponds to
// kKnownTrustedVaultKey -> kTrustedVaultKey1 key chain.
TEST_F(DownloadKeysResponseHandlerTest, ShouldHandleSingleKeyRotation) {
  const DownloadKeysResponseHandler::ProcessedResponse processed_response =
      handler().ProcessResponse(
          /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
          /*response_body=*/
          CreateGetSecurityDomainMemberResponseWithSyncMembership(
              /*trusted_vault_keys=*/{kKnownTrustedVaultKey, kTrustedVaultKey1},
              /*trusted_vault_keys_versions=*/
              {kKnownTrustedVaultKeyVersion, kKnownTrustedVaultKeyVersion + 1},
              /*signing_keys=*/{{}, kKnownTrustedVaultKey}));

  EXPECT_THAT(processed_response.status,
              Eq(TrustedVaultDownloadKeysStatus::kSuccess));
  EXPECT_THAT(processed_response.downloaded_keys,
              ElementsAre(kKnownTrustedVaultKey, kTrustedVaultKey1));
  EXPECT_THAT(processed_response.last_key_version,
              Eq(kKnownTrustedVaultKeyVersion + 1));
}

// Multiple key rotations may happen while client is offline, server-side key
// chain is kKnownTrustedVaultKey -> kTrustedVaultKey1 -> kTrustedVaultKey2.
TEST_F(DownloadKeysResponseHandlerTest, ShouldHandleMultipleKeyRotations) {
  const DownloadKeysResponseHandler::ProcessedResponse processed_response =
      handler().ProcessResponse(
          /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
          /*response_body=*/
          CreateGetSecurityDomainMemberResponseWithSyncMembership(
              /*trusted_vault_keys=*/
              {kKnownTrustedVaultKey, kTrustedVaultKey1, kTrustedVaultKey2},
              /*trusted_vault_keys_versions=*/
              {kKnownTrustedVaultKeyVersion, kKnownTrustedVaultKeyVersion + 1,
               kKnownTrustedVaultKeyVersion + 2},
              /*signing_keys=*/{{}, kKnownTrustedVaultKey, kTrustedVaultKey1}));

  EXPECT_THAT(processed_response.status,
              Eq(TrustedVaultDownloadKeysStatus::kSuccess));
  EXPECT_THAT(
      processed_response.downloaded_keys,
      ElementsAre(kKnownTrustedVaultKey, kTrustedVaultKey1, kTrustedVaultKey2));
  EXPECT_THAT(processed_response.last_key_version,
              Eq(kKnownTrustedVaultKeyVersion + 2));
}

// Server can already clean-up kKnownTrustedVaultKey, but it might still be
// possible to validate the key-chain.
// Full key chain is: kKnownTrustedVaultKey -> kTrustedVaultKey1 ->
// kTrustedVaultKey2.
// Server-side key chain is: kTrustedVaultKey1 -> kTrustedVaultKey2.
TEST_F(DownloadKeysResponseHandlerTest,
       ShouldHandleAbsenseOfKnownKeyWhenKeyChainIsRecoverable) {
  const DownloadKeysResponseHandler::ProcessedResponse processed_response =
      handler().ProcessResponse(
          /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
          /*response_body=*/
          CreateGetSecurityDomainMemberResponseWithSyncMembership(
              /*trusted_vault_keys=*/
              {kTrustedVaultKey1, kTrustedVaultKey2},
              /*trusted_vault_keys_versions=*/
              {kKnownTrustedVaultKeyVersion + 1,
               kKnownTrustedVaultKeyVersion + 2},
              /*signing_keys=*/
              {kKnownTrustedVaultKey, kTrustedVaultKey1}));

  EXPECT_THAT(processed_response.status,
              Eq(TrustedVaultDownloadKeysStatus::kSuccess));
  EXPECT_THAT(processed_response.downloaded_keys,
              ElementsAre(kTrustedVaultKey1, kTrustedVaultKey2));
  EXPECT_THAT(processed_response.last_key_version,
              Eq(kKnownTrustedVaultKeyVersion + 2));
}

// Server can already clean-up kKnownTrustedVaultKey and the following key. In
// this case client state is not sufficient to silently download keys and
// kKeyProofsVerificationFailed should be reported.
// Possible full key chain is: kKnownTrustedVaultKey -> kTrustedVaultKey1 ->
// kTrustedVaultKey2 -> kTrustedVaultKey3.
// Server side key chain is: kTrustedVaultKey2 -> kTrustedVaultKey3.
TEST_F(DownloadKeysResponseHandlerTest,
       ShouldHandleAbsenseOfKnownKeyWhenKeyChainIsNotRecoverable) {
  const DownloadKeysResponseHandler::ProcessedResponse processed_response =
      handler().ProcessResponse(
          /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
          /*response_body=*/
          CreateGetSecurityDomainMemberResponseWithSyncMembership(
              /*trusted_vault_keys=*/
              {kTrustedVaultKey2, kTrustedVaultKey3},
              /*trusted_vault_keys_versions=*/
              {kKnownTrustedVaultKeyVersion + 2,
               kKnownTrustedVaultKeyVersion + 3},
              /*signing_keys=*/
              {kTrustedVaultKey1, kTrustedVaultKey2}));

  EXPECT_THAT(processed_response.status,
              Eq(TrustedVaultDownloadKeysStatus::kKeyProofsVerificationFailed));
  EXPECT_THAT(processed_response.downloaded_keys, IsEmpty());
}

// The test populates undecryptable/corrupted |wrapped_key| field, handler
// should return kMembershipCorrupted to allow client to restore the member by
// re-registration.
TEST_F(DownloadKeysResponseHandlerTest, ShouldHandleUndecryptableKey) {
  trusted_vault_pb::SecurityDomainMember member;
  AddSecurityDomainMembership(
      GetSecurityDomainPath(SecurityDomainId::kChromeSync),
      MakeTestKeyPair()->public_key(),
      /*trusted_vault_keys=*/{kKnownTrustedVaultKey, kTrustedVaultKey1},
      /*trusted_vault_keys_versions=*/
      {kKnownTrustedVaultKeyVersion, kKnownTrustedVaultKeyVersion + 1},
      /*signing_keys=*/{{}, kKnownTrustedVaultKey}, &member);

  // Corrupt wrapped key corresponding to kTrustedVaultKey1.
  member.mutable_memberships(0)->mutable_keys(1)->set_wrapped_key(
      "undecryptable_key");

  EXPECT_THAT(handler()
                  .ProcessResponse(
                      /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
                      /*response_body=*/member.SerializeAsString())
                  .status,
              Eq(TrustedVaultDownloadKeysStatus::kMembershipCorrupted));
}

// The test populates invalid |rotation_proof| field for the single key
// rotation. kTrustedVaultKey1 is expected to be signed with
// kKnownTrustedVaultKey, but instead it's signed with kTrustedVaultKey2.
TEST_F(DownloadKeysResponseHandlerTest,
       ShouldHandleInvalidKeyProofOnSingleKeyRotation) {
  const DownloadKeysResponseHandler::ProcessedResponse processed_response =
      handler().ProcessResponse(
          /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
          /*response_body=*/
          CreateGetSecurityDomainMemberResponseWithSyncMembership(
              /*trusted_vault_keys=*/{kKnownTrustedVaultKey, kTrustedVaultKey1},
              /*trusted_vault_keys_versions=*/
              {kKnownTrustedVaultKeyVersion, kKnownTrustedVaultKeyVersion + 1},
              /*signing_keys=*/{{}, kTrustedVaultKey2}));

  EXPECT_THAT(processed_response.status,
              Eq(TrustedVaultDownloadKeysStatus::kKeyProofsVerificationFailed));
  EXPECT_THAT(processed_response.downloaded_keys, IsEmpty());
}

// The test populates invalid |rotation_proof| field for intermediate key when
// multiple key rotations have happened.
// kTrustedVaultKey1 is expected to be signed with kKnownTrustedVaultKey, but
// instead it's signed with kTrustedVaultKey2.
TEST_F(DownloadKeysResponseHandlerTest,
       ShouldHandleInvalidKeyProofOnMultipleKeyRotations) {
  const DownloadKeysResponseHandler::ProcessedResponse processed_response =
      handler().ProcessResponse(
          /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
          /*response_body=*/
          CreateGetSecurityDomainMemberResponseWithSyncMembership(
              /*trusted_vault_keys=*/{kKnownTrustedVaultKey, kTrustedVaultKey1,
                                      kTrustedVaultKey2},
              /*trusted_vault_keys_versions=*/
              {kKnownTrustedVaultKeyVersion, kKnownTrustedVaultKeyVersion + 1,
               kKnownTrustedVaultKeyVersion + 2},
              /*signing_keys=*/{{}, kTrustedVaultKey2, kTrustedVaultKey1}));

  EXPECT_THAT(processed_response.status,
              Eq(TrustedVaultDownloadKeysStatus::kKeyProofsVerificationFailed));
  EXPECT_THAT(processed_response.downloaded_keys, IsEmpty());
}

// In this scenario client already has most recent trusted vault key.
TEST_F(DownloadKeysResponseHandlerTest, ShouldHandleAbsenseOfNewKeys) {
  const DownloadKeysResponseHandler::ProcessedResponse processed_response =
      handler().ProcessResponse(
          /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
          /*response_body=*/
          CreateGetSecurityDomainMemberResponseWithSyncMembership(
              /*trusted_vault_keys=*/{kKnownTrustedVaultKey},
              /*trusted_vault_keys_versions=*/
              {kKnownTrustedVaultKeyVersion},
              /*signing_keys=*/{{}}));
  EXPECT_THAT(processed_response.status,
              Eq(TrustedVaultDownloadKeysStatus::kNoNewKeys));
  EXPECT_THAT(processed_response.downloaded_keys,
              ElementsAre(kKnownTrustedVaultKey));
}

// Tests handling the situation, when response isn't a valid serialized
// SecurityDomainMemberProto proto.
TEST_F(DownloadKeysResponseHandlerTest, ShouldHandleCorruptedResponseProto) {
  EXPECT_THAT(handler()
                  .ProcessResponse(
                      /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
                      /*response_body=*/"corrupted_proto")
                  .status,
              Eq(TrustedVaultDownloadKeysStatus::kOtherError));
}

// Client expects that the sync security domain membership exists, but the
// response indicates it doesn't by having no memberships.
TEST_F(DownloadKeysResponseHandlerTest, ShouldHandleAbsenseOfMemberships) {
  EXPECT_THAT(handler()
                  .ProcessResponse(
                      /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
                      /*response_body=*/trusted_vault_pb::SecurityDomainMember()
                          .SerializeAsString())
                  .status,
              Eq(TrustedVaultDownloadKeysStatus::kMembershipNotFound));
}

// Same as above, but there is a different security domain membership.
TEST_F(DownloadKeysResponseHandlerTest, ShouldHandleAbsenseOfSyncMembership) {
  trusted_vault_pb::SecurityDomainMember member;
  AddSecurityDomainMembership(
      "other_domain", MakeTestKeyPair()->public_key(),
      /*trusted_vault_keys=*/{kTrustedVaultKey1},
      /*trusted_vault_keys_versions=*/{kKnownTrustedVaultKeyVersion + 1},
      /*signing_keys=*/{kKnownTrustedVaultKey}, &member);

  EXPECT_THAT(handler()
                  .ProcessResponse(
                      /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
                      /*response_body=*/member.SerializeAsString())
                  .status,
              Eq(TrustedVaultDownloadKeysStatus::kMembershipNotFound));
}

TEST_F(DownloadKeysResponseHandlerTest, ShouldHandleEmptyMembership) {
  trusted_vault_pb::SecurityDomainMember member;
  AddSecurityDomainMembership(
      GetSecurityDomainPath(SecurityDomainId::kChromeSync),
      MakeTestKeyPair()->public_key(),
      /*trusted_vault_keys=*/{},
      /*trusted_vault_keys_versions=*/{},
      /*signing_keys=*/{}, &member);

  EXPECT_THAT(handler()
                  .ProcessResponse(
                      /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
                      /*response_body=*/member.SerializeAsString())
                  .status,
              Eq(TrustedVaultDownloadKeysStatus::kMembershipEmpty));
}

TEST_F(DownloadKeysResponseHandlerTest, ShouldHandleAllSecurityDomains) {
  for (const SecurityDomainId security_domain : kAllSecurityDomainIdValues) {
    trusted_vault_pb::SecurityDomainMember member;
    AddSecurityDomainMembership(
        GetSecurityDomainPath(security_domain), MakeTestKeyPair()->public_key(),
        /*trusted_vault_keys=*/{kTrustedVaultKey1},
        /*trusted_vault_keys_versions=*/{kKnownTrustedVaultKeyVersion + 1},
        /*signing_keys=*/{kKnownTrustedVaultKey}, &member);

    const DownloadKeysResponseHandler::ProcessedResponse processed_response =
        DownloadKeysResponseHandler(
            security_domain,
            TrustedVaultKeyAndVersion(kKnownTrustedVaultKey,
                                      kKnownTrustedVaultKeyVersion),
            MakeTestKeyPair())
            .ProcessResponse(
                /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
                /*response_body=*/member.SerializeAsString());

    EXPECT_THAT(processed_response.status,
                Eq(TrustedVaultDownloadKeysStatus::kSuccess));
    EXPECT_THAT(processed_response.downloaded_keys,
                ElementsAre(kTrustedVaultKey1));
    EXPECT_THAT(processed_response.last_key_version,
                Eq(kKnownTrustedVaultKeyVersion + 1));
  }
}

// Tests handling presence of other security domain memberships.
TEST_F(DownloadKeysResponseHandlerTest, ShouldHandleMultipleSecurityDomains) {
  trusted_vault_pb::SecurityDomainMember member;
  AddSecurityDomainMembership(
      "other_domain", MakeTestKeyPair()->public_key(),
      /*trusted_vault_keys=*/{kTrustedVaultKey1},
      /*trusted_vault_keys_versions=*/{kKnownTrustedVaultKeyVersion + 1},
      /*signing_keys=*/{{}}, &member);

  // Note: sync security domain membership is different by having correct
  // rotation proof.
  AddSecurityDomainMembership(
      GetSecurityDomainPath(SecurityDomainId::kChromeSync),
      MakeTestKeyPair()->public_key(),
      /*trusted_vault_keys=*/{kTrustedVaultKey1},
      /*trusted_vault_keys_versions=*/{kKnownTrustedVaultKeyVersion + 1},
      /*signing_keys=*/{kKnownTrustedVaultKey}, &member);

  const DownloadKeysResponseHandler::ProcessedResponse processed_response =
      handler().ProcessResponse(
          /*http_status=*/TrustedVaultRequest::HttpStatus::kSuccess,
          /*response_body=*/member.SerializeAsString());

  EXPECT_THAT(processed_response.status,
              Eq(TrustedVaultDownloadKeysStatus::kSuccess));
  EXPECT_THAT(processed_response.downloaded_keys,
              ElementsAre(kTrustedVaultKey1));
  EXPECT_THAT(processed_response.last_key_version,
              Eq(kKnownTrustedVaultKeyVersion + 1));
}

}  // namespace

}  // namespace trusted_vault
