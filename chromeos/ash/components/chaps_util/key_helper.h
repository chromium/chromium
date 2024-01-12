// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_KEY_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_KEY_HELPER_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace chromeos {

// Calculates and returns CKA_ID from public key bytes (`public_key_bytes`).
crypto::ScopedSECItem MakeIdFromPubKeyNss(
    const std::vector<uint8_t>& public_key_bytes);

// Converts ScopedSECItem `id` to vector<uint8_t>.
std::vector<uint8_t> SECItemToBytes(const crypto::ScopedSECItem& id);

// Creates PKCS11 id for the key (`key_data`). Returns a new id as vector.
std::vector<uint8_t> MakePkcs11IdForEcKey(base::span<const uint8_t> key_data);

// Extracts public `key` from EC_KEY object and returns it as X9.62
// uncompressed bytes.
COMPONENT_EXPORT(CHAPS_UTIL)
std::vector<uint8_t> GetEcPublicKeyBytes(const EC_KEY* ec_key);

// Extracts private `key` from EC_KEY object and returns it as bytes.
// Leading zeros will be padded.
COMPONENT_EXPORT(CHAPS_UTIL)
std::vector<uint8_t> GetEcPrivateKeyBytes(const EC_KEY* ec_key);

// Verify that `key` has type of EVP_PKEY_EC.
bool IsKeyEcType(const bssl::UniquePtr<EVP_PKEY>& key);

// Verify that `key` has type of EVP_PKEY_RSA.
bool IsKeyRsaType(const bssl::UniquePtr<EVP_PKEY>& key);

}  // namespace chromeos

#endif  // CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_KEY_HELPER_H_
