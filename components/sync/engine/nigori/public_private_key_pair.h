// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NIGORI_PUBLIC_PRIVATE_KEY_PAIR_H_
#define COMPONENTS_SYNC_ENGINE_NIGORI_PUBLIC_PRIVATE_KEY_PAIR_H_

#include <array>
#include <memory>
#include <string>

#include "base/containers/span.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace syncer {

// A wrapper around a 32-byte X25519 public-private key-pair.
class PublicPrivateKeyPair {
 public:
  // Generate a X25519 key pair.
  static PublicPrivateKeyPair GenerateNewKeyPair();
  // Initialize the Public-private key-pair using |private_key|.
  static absl::optional<PublicPrivateKeyPair> CreateByImport(
      base::span<uint8_t> private_key);

  PublicPrivateKeyPair(const PublicPrivateKeyPair& other) = delete;
  PublicPrivateKeyPair(PublicPrivateKeyPair&& other);
  PublicPrivateKeyPair& operator=(PublicPrivateKeyPair&& other);
  PublicPrivateKeyPair& operator=(const PublicPrivateKeyPair&) = delete;
  ~PublicPrivateKeyPair();

  // Returns the raw private key.
  std::array<uint8_t, X25519_PRIVATE_KEY_LEN> GetRawPrivateKey() const;

  // Returns the raw public key.
  std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> GetRawPublicKey() const;

 private:
  PublicPrivateKeyPair();
  explicit PublicPrivateKeyPair(base::span<uint8_t> private_key);

  uint8_t private_key_[X25519_PRIVATE_KEY_LEN];
  uint8_t public_key_[X25519_PUBLIC_VALUE_LEN];
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NIGORI_PUBLIC_PRIVATE_KEY_PAIR_H_
