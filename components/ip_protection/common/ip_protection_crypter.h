// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CRYPTER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CRYPTER_H_

#include <string>

#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/private-join-and-compute/src/crypto/elgamal.h"

namespace ip_protection {

// Deserialize an elgamal public key from given string.
absl::StatusOr<::private_join_and_compute::elgamal::PublicKey>
DeserializePublicKey(const std::string& serialized_public_key);

// Serialize an elgamal public key to a string.
absl::StatusOr<std::string> SerializePublicKey(
    const ::private_join_and_compute::elgamal::PublicKey& key);

// Randomize the given elgamal ciphertext using the given public key.
absl::StatusOr<::private_join_and_compute::elgamal::Ciphertext> Randomize(
    const std::string& serialized_public_key_y,
    const ::private_join_and_compute::elgamal::Ciphertext& ciphertext);

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CRYPTER_H_
