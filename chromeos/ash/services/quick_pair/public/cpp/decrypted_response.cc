// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"

namespace ash {
namespace quick_pair {

DecryptedResponse::DecryptedResponse() = default;

DecryptedResponse::DecryptedResponse(
    FastPairMessageType message_type,
    std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes,
    std::array<uint8_t, kDecryptedResponseSaltByteSize> salt)
    : message_type(message_type), address_bytes(address_bytes), salt(salt) {}

DecryptedResponse::DecryptedResponse(const DecryptedResponse&) = default;

DecryptedResponse::DecryptedResponse(DecryptedResponse&&) = default;

DecryptedResponse& DecryptedResponse::operator=(const DecryptedResponse&) =
    default;

DecryptedResponse& DecryptedResponse::operator=(DecryptedResponse&&) = default;

DecryptedResponse::~DecryptedResponse() = default;

}  // namespace quick_pair
}  // namespace ash
