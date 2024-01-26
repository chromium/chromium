// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NIGORI_CROSS_USER_SHARING_PUBLIC_PRIVATE_KEY_PAIR_H_
#define COMPONENTS_SYNC_ENGINE_NIGORI_CROSS_USER_SHARING_PUBLIC_PRIVATE_KEY_PAIR_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace syncer {

// A wrapper around a 32-byte X25519 public-private key-pair.
class CrossUserSharingPublicPrivateKeyPair {
 public:
  // Generate a X25519 key pair.
  static CrossUserSharingPublicPrivateKeyPair GenerateNewKeyPair();
  // Initialize the Public-private key-pair using |private_key|.
  static std::optional<CrossUserSharingPublicPrivateKeyPair> CreateByImport(
      base::span<const uint8_t> private_key);

  CrossUserSharingPublicPrivateKeyPair(
      const CrossUserSharingPublicPrivateKeyPair& other) = delete;
  CrossUserSharingPublicPrivateKeyPair(
      CrossUserSharingPublicPrivateKeyPair&& other);
  CrossUserSharingPublicPrivateKeyPair& operator=(
      CrossUserSharingPublicPrivateKeyPair&& other);
  CrossUserSharingPublicPrivateKeyPair& operator=(
      const CrossUserSharingPublicPrivateKeyPair&) = delete;
  ~CrossUserSharingPublicPrivateKeyPair();

  // Returns the raw private key.
  std::array<uint8_t, X25519_PRIVATE_KEY_LEN> GetRawPrivateKey() const;

  // Returns the raw public key.
  std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> GetRawPublicKey() const;

  // Encrypts the `plaintext` with Auth HPKE using |sender_public_key|
  // authenticated with own private/public key-pair.
  // Returns decrypted bytes as a vector if the decryption succeeds.
  // |authenticated_info| is optional.
  std::optional<std::vector<uint8_t>> HpkeAuthDecrypt(
      base::span<const uint8_t> encrypted_data,
      base::span<const uint8_t> sender_public_key,
      base::span<const uint8_t> authenticated_info) const;

  // Decrypt |encrypted_data| with Auth HPKE using own public/private key-pair
  // and authenticated with |sender_public_key|.
  // Returns encrypted bytes as a vector if the encryption succeeds.
  // |authenticated_info| is optional and should match the one used during
  // encryption.
  std::optional<std::vector<uint8_t>> HpkeAuthEncrypt(
      base::span<const uint8_t> plaintext,
      base::span<const uint8_t> recipient_public_key,
      base::span<const uint8_t> authenticated_info) const;

 private:
  CrossUserSharingPublicPrivateKeyPair();
  explicit CrossUserSharingPublicPrivateKeyPair(
      base::span<const uint8_t> private_key);

  bssl::ScopedEVP_HPKE_KEY key_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NIGORI_CROSS_USER_SHARING_PUBLIC_PRIVATE_KEY_PAIR_H_
