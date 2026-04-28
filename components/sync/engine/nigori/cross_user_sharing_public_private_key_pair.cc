// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "crypto/hpke.h"

namespace syncer {

namespace {

constexpr crypto::hpke::HpkeParams kHpkeParams{
    .kem = crypto::hpke::KemType::kX25519HkdfSha256,
    .kdf = crypto::hpke::KdfType::kHkdfSha256,
    .aead = crypto::hpke::AeadType::kChaCha20Poly1305};
}  // namespace

CrossUserSharingPublicPrivateKeyPair::CrossUserSharingPublicPrivateKeyPair(
    CrossUserSharingPublicPrivateKeyPair&& other) = default;

CrossUserSharingPublicPrivateKeyPair&
CrossUserSharingPublicPrivateKeyPair::operator=(
    CrossUserSharingPublicPrivateKeyPair&& other) = default;

CrossUserSharingPublicPrivateKeyPair::~CrossUserSharingPublicPrivateKeyPair() =
    default;

// static
CrossUserSharingPublicPrivateKeyPair
CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair() {
  return CrossUserSharingPublicPrivateKeyPair();
}

CrossUserSharingPublicPrivateKeyPair::CrossUserSharingPublicPrivateKeyPair(
    base::span<const uint8_t, X25519_PRIVATE_KEY_LEN> private_key)
    : key_(crypto::keypair::PrivateKey::FromX25519PrivateKey(private_key)) {}

CrossUserSharingPublicPrivateKeyPair::CrossUserSharingPublicPrivateKeyPair()
    : key_(crypto::keypair::PrivateKey::GenerateX25519()) {}

std::array<uint8_t, X25519_PRIVATE_KEY_LEN>
CrossUserSharingPublicPrivateKeyPair::GetRawPrivateKey() const {
  return key_.ToX25519PrivateKey();
}

std::array<uint8_t, X25519_PUBLIC_VALUE_LEN>
CrossUserSharingPublicPrivateKeyPair::GetRawPublicKey() const {
  return key_.ToX25519PublicKey();
}

std::optional<std::vector<uint8_t>>
CrossUserSharingPublicPrivateKeyPair::HpkeAuthEncrypt(
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> recipient_public_key,
    base::span<const uint8_t> authenticated_info) const {
  if (recipient_public_key.size() != X25519_PUBLIC_VALUE_LEN) {
    return std::nullopt;
  }

  auto receiver_pub = crypto::keypair::PublicKey::FromX25519PublicKey(
      recipient_public_key.first<X25519_PUBLIC_VALUE_LEN>());

  return crypto::hpke::AuthSeal(kHpkeParams, key_, receiver_pub, plaintext,
                                authenticated_info, {});
}

std::optional<std::vector<uint8_t>>
CrossUserSharingPublicPrivateKeyPair::HpkeAuthDecrypt(
    base::span<const uint8_t> encrypted_data,
    base::span<const uint8_t> sender_public_key,
    base::span<const uint8_t> authenticated_info) const {
  if (sender_public_key.size() != X25519_PUBLIC_VALUE_LEN) {
    return std::nullopt;
  }

  auto sender_pub = crypto::keypair::PublicKey::FromX25519PublicKey(
      sender_public_key.first<X25519_PUBLIC_VALUE_LEN>());

  return crypto::hpke::AuthOpen(kHpkeParams, key_, sender_pub, encrypted_data,
                                authenticated_info, {});

}  // namespace

}  // namespace syncer
