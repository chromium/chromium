// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_crypto.h"

#include <array>

#include "base/check_op.h"
#include "components/trusted_vault/securebox.h"
#include "crypto/hash.h"
#include "crypto/hmac.h"

namespace trusted_vault {

namespace {

const size_t kHMACDigestLength = 32;
const uint8_t kWrappedKeyHeader[] = {'V', '1', ' ', 's', 'h', 'a', 'r',
                                     'e', 'd', '_', 'k', 'e', 'y'};

}  // namespace

std::optional<std::vector<uint8_t>> DecryptTrustedVaultWrappedKey(
    const SecureBoxPrivateKey& private_key,
    base::span<const uint8_t> wrapped_key) {
  return private_key.Decrypt(
      /*shared_secret=*/base::span<const uint8_t>(), kWrappedKeyHeader,
      /*encrypted_payload=*/wrapped_key);
}

std::vector<uint8_t> ComputeTrustedVaultWrappedKey(
    const SecureBoxPublicKey& public_key,
    base::span<const uint8_t> trusted_vault_key) {
  return public_key.Encrypt(
      /*shared_secret=*/base::span<const uint8_t>(), kWrappedKeyHeader,
      /*payload=*/trusted_vault_key);
}

std::array<uint8_t, crypto::hash::kSha256Size> ComputeMemberProof(
    const SecureBoxPublicKey& key,
    base::span<const uint8_t> trusted_vault_key) {
  return crypto::hmac::SignSha256(trusted_vault_key, key.ExportToBytes());
}

bool VerifyMemberProof(const SecureBoxPublicKey& key,
                       base::span<const uint8_t> trusted_vault_key,
                       base::span<const uint8_t> member_proof) {
  auto proof = member_proof.to_fixed_extent<kHMACDigestLength>();
  if (!proof) {
    return false;
  }
  return crypto::hmac::VerifySha256(trusted_vault_key, key.ExportToBytes(),
                                    *proof);
}

std::vector<uint8_t> ComputeRotationProofForTesting(  // IN-TEST
    const std::vector<uint8_t>& trusted_vault_key,
    const std::vector<uint8_t>& prev_trusted_vault_key) {
  return SecureBoxSymmetricEncrypt(
      /*shared_secret=*/prev_trusted_vault_key,
      /*header=*/trusted_vault_key,
      /*payload=*/base::span<uint8_t>());
}

bool VerifyRotationProof(const std::vector<uint8_t>& trusted_vault_key,
                         const std::vector<uint8_t>& prev_trusted_vault_key,
                         const std::vector<uint8_t>& rotation_proof) {
  return SecureBoxSymmetricDecrypt(
             /*shared_secret=*/prev_trusted_vault_key,
             /*header=*/trusted_vault_key, /*encrypted_payload=*/rotation_proof)
      .has_value();
}

}  // namespace trusted_vault
