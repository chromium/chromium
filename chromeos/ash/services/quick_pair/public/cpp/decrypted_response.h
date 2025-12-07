// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_DECRYPTED_RESPONSE_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_DECRYPTED_RESPONSE_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <optional>

#include "chromeos/ash/services/quick_pair/public/cpp/fast_pair_message_type.h"

inline constexpr int kDecryptedResponseAddressByteSize = 6;
inline constexpr int kDecryptedResponseSaltByteSize = 9;

namespace ash {
namespace quick_pair {

// Thin class which is used by the higher level components of the Quick Pair
// system to represent a decrypted response.
struct DecryptedResponse {
  DecryptedResponse();
  DecryptedResponse(
      FastPairMessageType message_type,
      std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes,
      std::array<uint8_t, kDecryptedResponseSaltByteSize> salt);
  DecryptedResponse(
      FastPairMessageType message_type,
      std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes,
      std::array<uint8_t, kDecryptedResponseSaltByteSize> salt,
      std::optional<uint8_t> flags,
      std::optional<uint8_t> num_addresses,
      std::optional<std::array<uint8_t, kDecryptedResponseAddressByteSize>>
          secondary_address_bytes);
  DecryptedResponse(const DecryptedResponse&);
  DecryptedResponse(DecryptedResponse&&);
  DecryptedResponse& operator=(const DecryptedResponse&);
  DecryptedResponse& operator=(DecryptedResponse&&);
  ~DecryptedResponse();

  FastPairMessageType message_type;
  std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes;
  std::array<uint8_t, kDecryptedResponseSaltByteSize> salt;
  // Key-based Pairing Extended Response
  std::optional<uint8_t> flags;
  std::optional<uint8_t> num_addresses;
  std::optional<std::array<uint8_t, kDecryptedResponseAddressByteSize>>
      secondary_address_bytes;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_DECRYPTED_RESPONSE_H_
