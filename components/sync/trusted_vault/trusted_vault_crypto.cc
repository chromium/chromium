// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_crypto.h"

#include "components/sync/trusted_vault/securebox.h"
#include "crypto/hmac.h"

namespace syncer {

namespace {

const size_t kHMACDigestLength = 32;
const uint8_t kWrappedKeyHeader[] = {'V', '1', ' ', 's', 'h', 'a', 'r',
                                     'e', 'd', '_', 'k', 'e', 'y'};

}  // namespace

base::Optional<std::vector<uint8_t>> DecryptTrustedVaultWrappedKey(
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

std::vector<uint8_t> ComputeTrustedVaultHMAC(base::span<const uint8_t> key,
                                             base::span<const uint8_t> data) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  CHECK(hmac.Init(key));

  std::vector<uint8_t> digest(kHMACDigestLength);
  CHECK(hmac.Sign(data, digest));
  return digest;
}

bool VerifyTrustedVaultHMAC(base::span<const uint8_t> key,
                            base::span<const uint8_t> data,
                            base::span<const uint8_t> digest) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  CHECK(hmac.Init(key));
  return hmac.Verify(data, digest);
}

}  // namespace syncer
