// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_FAST_PAIR_DECRYPTION_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_FAST_PAIR_DECRYPTION_H_

#include <array>
#include <optional>
#include <string>

#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"

namespace ash {
namespace quick_pair {
namespace fast_pair_decryption {

inline constexpr int kBlockByteSize = 16;

std::optional<DecryptedResponse> ParseDecryptedResponse(
    const std::array<uint8_t, 16>& aes_key_bytes,
    const std::array<uint8_t, 16>& encrypted_response_bytes);

std::optional<DecryptedPasskey> ParseDecryptedPasskey(
    const std::array<uint8_t, 16>& aes_key_bytes,
    const std::array<uint8_t, 16>& encrypted_passkey_bytes);

}  // namespace fast_pair_decryption
}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_FAST_PAIR_DECRYPTION_H_
