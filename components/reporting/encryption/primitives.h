// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_ENCRYPTION_PRIMITIVES_H_
#define COMPONENTS_REPORTING_ENCRYPTION_PRIMITIVES_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace reporting {

static constexpr size_t kKeySize = 32u;
static constexpr size_t kNonceSize = 12u;
static constexpr size_t kAeadTagSize = 16u;
static constexpr size_t kSignatureSize = 64u;
static constexpr size_t kSignKeySize = 64u;

// Computes shared secret. Generates new pair of keys, computes shared secret
// from its private key and peer public value. Returns true, shared secret and
// public value from the generated pair in case of success. Otherwise returns
// false.
bool ComputeSharedSecret(const uint8_t peer_public_value[kKeySize],
                         uint8_t shared_secret[kKeySize],
                         uint8_t generated_public_value[kKeySize]);

// Produces symmetric key from shared secret using HKDF.
bool ProduceSymmetricKey(const uint8_t shared_secret[kKeySize],
                         uint8_t symmetric_key[kKeySize]);

// Performs AEAD encryption with Chacha20Poly1305 key.
// Returns true and filled in encrypted |output_data|, if successful.
// Otherwise returns false.
bool PerformSymmetricEncryption(const uint8_t symmetric_key[kKeySize],
                                std::string_view input_data,
                                std::string* output_data);

// Verifies ED25519 |signature| of the |message|. Returns true if successful,
// false otherwise.
bool VerifySignature(const uint8_t verification_key[kKeySize],
                     std::string_view message,
                     const uint8_t signature[kSignatureSize]);

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_ENCRYPTION_PRIMITIVES_H_
