// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_FAST_PAIR_MESSAGE_TYPE_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_FAST_PAIR_MESSAGE_TYPE_H_

namespace ash {
namespace quick_pair {

// Type values for Fast Pair messages.
enum class FastPairMessageType {
  // Key-based Pairing Request.
  kKeyBasedPairingRequest,
  // Key-based Pairing Response.
  kKeyBasedPairingResponse,
  // Key-based Pairing Extended Response.
  kKeyBasedPairingExtendedResponse,
  // Seeker's passkey.
  // Note: This and Key-based Pairing Extended Response are both equal to 2, but
  // they are messages for different characteristics.
  kSeekersPasskey = 2,
  // Provider's passkey.
  kProvidersPasskey,
};

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_FAST_PAIR_MESSAGE_TYPE_H_
