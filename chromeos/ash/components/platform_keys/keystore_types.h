// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS_KEYSTORE_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS_KEYSTORE_TYPES_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/types/expected.h"

namespace chromeos {

// The name of a WebCrypto key algorithm.
enum class KeystoreAlgorithmName {
  kUnknown,
  kRsassaPkcs115,
  kRsaOaep,
  kEcdsa,
};

// The type of custom key attribute.
enum class KeystoreKeyAttributeType {
  kUnknown,
  kPlatformKeysTag,
};

// Recognized WebCrypto signing schemes.
enum class KeystoreSigningScheme {
  kUnknown,
  kRsassaPkcs1V15None,  // The data is PKCS#1 v1.5 padded but not hashed.
  kRsassaPkcs1V15Sha1,
  kRsassaPkcs1V15Sha256,
  kRsassaPkcs1V15Sha384,
  kRsassaPkcs1V15Sha512,
  kEcdsaSha1,
  kEcdsaSha256,
  kEcdsaSha384,
  kEcdsaSha512,
};

// Returned by ChallengeAttestationOnlyKeystore().
// On success, contains the challenge response.
// On failure, contains the error message.
using ChallengeAttestationOnlyKeystoreResult =
    base::expected<std::vector<uint8_t>, std::string>;

}  // namespace chromeos

#endif  // CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS_KEYSTORE_TYPES_H_
