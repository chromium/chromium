// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_HYBRID_ENCRYPTION_KEY_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_HYBRID_ENCRYPTION_KEY_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

class HybridEncryptionKey {
 public:
  // Generates a new random Hybrid key.
  HybridEncryptionKey();
  ~HybridEncryptionKey();

  // Move-able but not copy-able.
  HybridEncryptionKey(const HybridEncryptionKey&) = delete;
  HybridEncryptionKey& operator=(const HybridEncryptionKey&) = delete;
  HybridEncryptionKey(HybridEncryptionKey&& other) noexcept;
  HybridEncryptionKey& operator=(HybridEncryptionKey&& other) noexcept;

  std::optional<std::vector<uint8_t>> Decrypt(
      base::span<const uint8_t> ciphertext) const;

  // Returns the public key in a keyset serialized as Tink's ProtoKeysetFormat.
  std::string ExportPublicKey() const;

  // Returns concatenated encapsulated key & ciphertext, or empty vector if
  // encryption fails.
  std::vector<uint8_t> EncryptForTesting(
      base::span<const uint8_t> plaintext) const;

 private:
  friend std::vector<uint8_t> GetHybridEncryptionPublicKeyForTesting(
      const HybridEncryptionKey& key);
  friend HybridEncryptionKey CreateHybridEncryptionKeyForTesting();

  // Initializes a new Hybrid key with a provided `private_key`.
  // Used for testing.
  explicit HybridEncryptionKey(base::span<const uint8_t> private_key);

  std::vector<uint8_t> GetPublicKey() const;

  bssl::ScopedEVP_HPKE_KEY key_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_HYBRID_ENCRYPTION_KEY_H_
