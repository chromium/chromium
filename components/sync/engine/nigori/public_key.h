// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NIGORI_PUBLIC_KEY_H_
#define COMPONENTS_SYNC_ENGINE_NIGORI_PUBLIC_KEY_H_

#include <array>

#include "base/containers/span.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace syncer {

// A wrapper around a 32-byte X25519 public key.
class PublicKey {
 public:
  PublicKey(const PublicKey& other) = delete;
  PublicKey(PublicKey&& other);
  PublicKey& operator=(PublicKey&& other);
  PublicKey& operator=(const PublicKey& other) = delete;
  ~PublicKey();

  // Initialize the key using |public_key|.
  static absl::optional<PublicKey> CreateByImport(
      base::span<const uint8_t> public_key);

  // Returns the raw public key.
  std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> GetRawPublicKey() const;

  // Creates an exact clone.
  PublicKey Clone() const;

 private:
  explicit PublicKey(base::span<const uint8_t> public_key);

  uint8_t public_key_[X25519_PUBLIC_VALUE_LEN];
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NIGORI_PUBLIC_KEY_H_
