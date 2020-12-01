// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_CRYPTO_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_CRYPTO_H_

#include <vector>

#include "base/containers/span.h"
#include "base/optional.h"

namespace syncer {

class SecureBoxPrivateKey;
class SecureBoxPublicKey;

// Decrypts |wrapped_key| using securebox. Returns decrypted key if successful
// and base::nullopt otherwise.
base::Optional<std::vector<uint8_t>> DecryptTrustedVaultWrappedKey(
    const SecureBoxPrivateKey& private_key,
    base::span<const uint8_t> wrapped_key);

// Encrypts |trusted_vault_key| using securebox.
std::vector<uint8_t> ComputeTrustedVaultWrappedKey(
    const SecureBoxPublicKey& public_key,
    base::span<const uint8_t> trusted_vault_key);

// Computes HMAC digest using SHA-256.
std::vector<uint8_t> ComputeTrustedVaultHMAC(base::span<const uint8_t> key,
                                             base::span<const uint8_t> data);

// Returns true if |digest| is a valid HMAC SHA-256 digest of |data| and |key|.
bool VerifyTrustedVaultHMAC(base::span<const uint8_t> key,
                            base::span<const uint8_t> data,
                            base::span<const uint8_t> digest);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_CRYPTO_H_
