// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CRYPTO_CRYPTER_H_
#define COMPONENTS_LEGION_CRYPTO_CRYPTER_H_

#include <stdint.h>

#include <array>
#include <optional>
#include <vector>

#include "base/containers/span.h"

namespace legion {

// Crypter handles the post-handshake encryption of messages.
class Crypter {
 public:
  Crypter(base::span<const uint8_t, 32> read_key,
          base::span<const uint8_t, 32> write_key);
  ~Crypter();

  // Encrypt encrypts |plaintext|. It returns the ciphertext on success and
  // std::nullopt on error.
  std::optional<std::vector<uint8_t>> Encrypt(
      base::span<const uint8_t> plaintext);

  // Decrypt decrypts |ciphertext|. It returns the plaintext on success and
  // std::nullopt on error.
  std::optional<std::vector<uint8_t>> Decrypt(
      base::span<const uint8_t> ciphertext);

 private:
  const std::array<uint8_t, 32> read_key_, write_key_;
  uint32_t read_sequence_num_ = 0;
  uint32_t write_sequence_num_ = 0;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_CRYPTO_CRYPTER_H_
