// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_CRYPTO_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_CRYPTO_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "components/trusted_vault/securebox.h"

namespace trusted_vault {

class SecureBoxPrivateKey;
class SecureBoxPublicKey;

// Decrypts |wrapped_key| using securebox. Returns decrypted key if successful
// and std::nullopt otherwise.
std::optional<std::vector<uint8_t>> DecryptTrustedVaultWrappedKey(
    const SecureBoxPrivateKey& private_key,
    base::span<const uint8_t> wrapped_key);

// Encrypts |trusted_vault_key| using securebox.
std::vector<uint8_t> ComputeTrustedVaultWrappedKey(
    const SecureBoxPublicKey& public_key,
    base::span<const uint8_t> trusted_vault_key);

// Signs |key| with |trusted_vault_key| using HMAC-SHA-256.
std::vector<uint8_t> ComputeMemberProof(
    const SecureBoxPublicKey& key,
    const std::vector<uint8_t>& trusted_vault_key);

// Returns whether |member_proof| is |key| signed with |trusted_vault_key|.
bool VerifyMemberProof(const SecureBoxPublicKey& key,
                       const std::vector<uint8_t>& trusted_vault_key,
                       const std::vector<uint8_t>& member_proof);

// Signs |trusted_vault_key| with |prev_trusted_vault_key| using SecureBox
// symmetric encryption.
std::vector<uint8_t> ComputeRotationProofForTesting(
    const std::vector<uint8_t>& trusted_vault_key,
    const std::vector<uint8_t>& prev_trusted_vault_key);

// Returns whether |rotation_proof| is |trusted_vault_key| signed with
// |prev_trusted_vault_key|.
bool VerifyRotationProof(const std::vector<uint8_t>& trusted_vault_key,
                         const std::vector<uint8_t>& prev_trusted_vault_key,
                         const std::vector<uint8_t>& rotation_proof);

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_CRYPTO_H_
