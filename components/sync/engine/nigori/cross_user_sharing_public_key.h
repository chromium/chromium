// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NIGORI_CROSS_USER_SHARING_PUBLIC_KEY_H_
#define COMPONENTS_SYNC_ENGINE_NIGORI_CROSS_USER_SHARING_PUBLIC_KEY_H_

#include <array>
#include <optional>

#include "base/containers/span.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace syncer {

// A wrapper around a 32-byte X25519 public key.
class CrossUserSharingPublicKey {
 public:
  CrossUserSharingPublicKey(const CrossUserSharingPublicKey& other) = delete;
  CrossUserSharingPublicKey(CrossUserSharingPublicKey&& other);
  CrossUserSharingPublicKey& operator=(CrossUserSharingPublicKey&& other);
  CrossUserSharingPublicKey& operator=(const CrossUserSharingPublicKey& other) =
      delete;
  ~CrossUserSharingPublicKey();

  // Initialize the key using |public_key|.
  static std::optional<CrossUserSharingPublicKey> CreateByImport(
      base::span<const uint8_t> public_key);

  // Returns the raw public key.
  std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> GetRawPublicKey() const;

  // Creates an exact clone.
  CrossUserSharingPublicKey Clone() const;

 private:
  explicit CrossUserSharingPublicKey(
      base::span<const uint8_t, X25519_PUBLIC_VALUE_LEN> public_key);

  std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> public_key_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NIGORI_CROSS_USER_SHARING_PUBLIC_KEY_H_
