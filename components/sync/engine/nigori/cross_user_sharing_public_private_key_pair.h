// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NIGORI_CROSS_USER_SHARING_PUBLIC_PRIVATE_KEY_PAIR_H_
#define COMPONENTS_SYNC_ENGINE_NIGORI_CROSS_USER_SHARING_PUBLIC_PRIVATE_KEY_PAIR_H_

#include <array>
#include <memory>
#include <string>

#include "base/containers/span.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace syncer {

// A wrapper around a 32-byte X25519 public-private key-pair.
class CrossUserSharingPublicPrivateKeyPair {
 public:
  // Generate a X25519 key pair.
  static CrossUserSharingPublicPrivateKeyPair GenerateNewKeyPair();
  // Initialize the Public-private key-pair using |private_key|.
  static absl::optional<CrossUserSharingPublicPrivateKeyPair> CreateByImport(
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

 private:
  CrossUserSharingPublicPrivateKeyPair();
  explicit CrossUserSharingPublicPrivateKeyPair(
      base::span<const uint8_t> private_key);

  uint8_t private_key_[X25519_PRIVATE_KEY_LEN];
  uint8_t public_key_[X25519_PUBLIC_VALUE_LEN];
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NIGORI_CROSS_USER_SHARING_PUBLIC_PRIVATE_KEY_PAIR_H_
