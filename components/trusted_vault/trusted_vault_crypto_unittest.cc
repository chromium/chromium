// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_crypto.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "components/trusted_vault/securebox.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::Eq;
using testing::Ne;

const char kEncodedPrivateKey[] =
    "49e052293c29b5a50b0013eec9d030ac2ad70a42fe093be084264647cb04e16f";

std::unique_ptr<SecureBoxKeyPair> MakeTestKeyPair() {
  std::vector<uint8_t> private_key_bytes;
  bool success = base::HexStringToBytes(kEncodedPrivateKey, &private_key_bytes);
  DCHECK(success);
  return SecureBoxKeyPair::CreateByPrivateKeyImport(private_key_bytes);
}

TEST(TrustedVaultCrypto, ShouldHandleDecryptionFailure) {
  EXPECT_THAT(DecryptTrustedVaultWrappedKey(
                  MakeTestKeyPair()->private_key(),
                  /*wrapped_key=*/std::vector<uint8_t>{1, 2, 3, 4}),
              Eq(std::nullopt));
}

TEST(TrustedVaultCrypto, ShouldEncryptAndDecryptWrappedKey) {
  const std::vector<uint8_t> trusted_vault_key = {1, 2, 3, 4};
  const std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  std::optional<std::vector<uint8_t>> decrypted_trusted_vault_key =
      DecryptTrustedVaultWrappedKey(
          key_pair->private_key(),
          /*wrapped_key=*/ComputeTrustedVaultWrappedKey(key_pair->public_key(),
                                                        trusted_vault_key));
  ASSERT_THAT(decrypted_trusted_vault_key, Ne(std::nullopt));
  EXPECT_THAT(*decrypted_trusted_vault_key, Eq(trusted_vault_key));
}

TEST(TrustedVaultCrypto, ShouldComputeAndVerifyMemberProof) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  const std::vector<uint8_t> trusted_vault_key = {1, 2, 3, 4};
  EXPECT_TRUE(VerifyMemberProof(
      key_pair->public_key(), trusted_vault_key, /*member_proof=*/
      ComputeMemberProof(key_pair->public_key(), trusted_vault_key)));
}

TEST(TrustedVaultCrypto, ShouldDetectIncorrectMemberProof) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  const std::vector<uint8_t> correct_trusted_vault_key = {1, 2, 3, 4};
  const std::vector<uint8_t> incorrect_trusted_vault_key = {1, 2, 3, 5};
  EXPECT_FALSE(VerifyMemberProof(
      key_pair->public_key(), correct_trusted_vault_key, /*member_proof=*/
      ComputeMemberProof(key_pair->public_key(), incorrect_trusted_vault_key)));
}

TEST(TrustedVaultCrypto, ShouldComputeAndVerifyRotationProof) {
  const std::vector<uint8_t> trusted_vault_key = {1, 2, 3, 4};
  const std::vector<uint8_t> prev_trusted_vault_key = {1, 2, 3, 5};
  EXPECT_TRUE(VerifyRotationProof(
      trusted_vault_key, prev_trusted_vault_key, /*rotation_proof=*/
      ComputeRotationProofForTesting(trusted_vault_key,
                                     prev_trusted_vault_key)));
}

TEST(TrustedVaultCrypto, ShouldDetectIncorrectRotationProof) {
  const std::vector<uint8_t> trusted_vault_key = {1, 2, 3, 4};
  const std::vector<uint8_t> prev_trusted_vault_key = {1, 2, 3, 5};
  const std::vector<uint8_t> incorrect_trusted_vault_key = {1, 2, 3, 6};
  EXPECT_FALSE(VerifyRotationProof(
      trusted_vault_key, prev_trusted_vault_key, /*rotation_proof=*/
      ComputeRotationProofForTesting(trusted_vault_key,
                                     incorrect_trusted_vault_key)));
}

}  // namespace

}  // namespace trusted_vault
