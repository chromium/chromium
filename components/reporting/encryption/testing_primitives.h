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
void GenerateEncryptionKeyPair(uint8_t private_key[kKeySize],
                               uint8_t public_value[kKeySize]);

// Restore shared secret.
void RestoreSharedSecret(const uint8_t private_key[kKeySize],
                         const uint8_t peer_public_value[kKeySize],
                         uint8_t shared_secret[kKeySize]);

// Performs AEAD decryption with Chacha20Poly1305 key.
void PerformSymmetricDecryption(const uint8_t symmetric_key[kKeySize],
                                std::string_view input_data,
                                std::string* output_data);

// Generates new pair of signing private key and public value.
void GenerateSigningKeyPair(uint8_t private_key[kSignKeySize],
                            uint8_t public_value[kKeySize]);

// Signs the |message| producing ED25519 |signature|.
void SignMessage(const uint8_t signing_key[kSignKeySize],
                 std::string_view message,
                 uint8_t signature[kSignatureSize]);

}  // namespace test
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_ENCRYPTION_TESTING_PRIMITIVES_H_
