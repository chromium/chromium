// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_DECRYPTED_PASSKEY_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_DECRYPTED_PASSKEY_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "chromeos/ash/services/quick_pair/public/cpp/fast_pair_message_type.h"

inline constexpr int kDecryptedPasskeySaltByteSize = 12;

namespace ash {
namespace quick_pair {

// Thin class which is used by the higher level components of the Quick Pair
// system to represent a decrypted account passkey.
struct DecryptedPasskey {
  DecryptedPasskey();
  DecryptedPasskey(FastPairMessageType message_type,
                   uint32_t passkey,
                   std::array<uint8_t, kDecryptedPasskeySaltByteSize> salt);
  DecryptedPasskey(const DecryptedPasskey&);
  DecryptedPasskey(DecryptedPasskey&&);
  DecryptedPasskey& operator=(const DecryptedPasskey&);
  DecryptedPasskey& operator=(DecryptedPasskey&&);
  ~DecryptedPasskey();

  FastPairMessageType message_type;
  uint32_t passkey;
  std::array<uint8_t, kDecryptedPasskeySaltByteSize> salt;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_DECRYPTED_PASSKEY_H_
