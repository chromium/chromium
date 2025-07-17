// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_ENCRYPTION_TESTING_PRIMITIVES_H_
#define COMPONENTS_REPORTING_ENCRYPTION_TESTING_PRIMITIVES_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "components/reporting/encryption/primitives.h"

namespace reporting {
namespace test {

// Generates new pair of encryption private key and public value.
void GenerateEncryptionKeyPair(base::span<uint8_t, kKeySize> private_key,
                               base::span<uint8_t, kKeySize> public_value);

// Restore shared secret.
void RestoreSharedSecret(base::span<const uint8_t, kKeySize> private_key,
                         base::span<const uint8_t, kKeySize> peer_public_value,
                         base::span<uint8_t, kKeySize> shared_secret);

// Performs AEAD decryption with Chacha20Poly1305 key.
void PerformSymmetricDecryption(base::span<const uint8_t, kKeySize> key,
                                std::string_view input_data,
                                std::string* output_data);

// Generates new pair of signing private key and public value.
void GenerateSigningKeyPair(base::span<uint8_t, kSignKeySize> private_key,
                            base::span<uint8_t, kKeySize> public_value);

// Signs the |message| producing ED25519 |signature|.
void SignMessage(base::span<const uint8_t, kSignKeySize> signing_key,
                 std::string_view message,
                 base::span<uint8_t, kSignatureSize> signature);

}  // namespace test
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_ENCRYPTION_TESTING_PRIMITIVES_H_
