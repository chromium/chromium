// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_RFC8291_UTIL_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_RFC8291_UTIL_H_

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/types/expected.h"

namespace crypto::keypair {
class PrivateKey;
}  // namespace crypto::keypair

namespace gcm {

BASE_DECLARE_FEATURE(kRfc8291StrictCompliance);

enum class Rfc8291EncryptionError {
  kKeyDerivationFailed,
  kEncryptionFailed,
};

// Stateless, synchronous RFC 8291 encryption helper.
// Encrypts the `message` using the `p256dh` recipient public key and the
// `auth_secret` with the `sender_private_key`.
//
// Returns the fully formatted RFC 8291 payload (conforming to Web Push message
// encryption over RFC 8188) as a string or an error.
base::expected<std::string, Rfc8291EncryptionError> EncryptPayloadWithRfc8291(
    std::string_view message,
    std::string_view p256dh,
    std::string_view auth_secret,
    const crypto::keypair::PrivateKey& sender_private_key);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_RFC8291_UTIL_H_
