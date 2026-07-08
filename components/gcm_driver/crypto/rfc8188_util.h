// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_RFC8188_UTIL_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_RFC8188_UTIL_H_

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/types/expected.h"

namespace crypto::keypair {
class PrivateKey;
}  // namespace crypto::keypair

namespace gcm {

BASE_DECLARE_FEATURE(kRfc8188StrictCompliance);

enum class Rfc8188EncryptionError {
  kKeyDerivationFailed,
  kEncryptionFailed,
};

// Stateless, synchronous RFC 8188 encryption helper.
// Encrypts the `message` using the `p256dh` recipient public key and the
// `auth_secret` with the `sender_private_key`.
//
// Returns the fully formatted RFC 8188 payload as a string or an error.
base::expected<std::string, Rfc8188EncryptionError> EncryptPayloadWithRfc8188(
    std::string_view message,
    std::string_view p256dh,
    std::string_view auth_secret,
    const crypto::keypair::PrivateKey& sender_private_key);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_RFC8188_UTIL_H_
