// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_passkey.h"

namespace ash {
namespace quick_pair {

DecryptedPasskey::DecryptedPasskey() = default;

DecryptedPasskey::DecryptedPasskey(
    FastPairMessageType message_type,
    uint32_t passkey,
    std::array<uint8_t, kDecryptedPasskeySaltByteSize> salt)
    : message_type(message_type), passkey(passkey), salt(salt) {}

DecryptedPasskey::DecryptedPasskey(const DecryptedPasskey&) = default;

DecryptedPasskey::DecryptedPasskey(DecryptedPasskey&&) = default;

DecryptedPasskey& DecryptedPasskey::operator=(const DecryptedPasskey&) =
    default;

DecryptedPasskey& DecryptedPasskey::operator=(DecryptedPasskey&&) = default;

DecryptedPasskey::~DecryptedPasskey() = default;

}  // namespace quick_pair
}  // namespace ash
