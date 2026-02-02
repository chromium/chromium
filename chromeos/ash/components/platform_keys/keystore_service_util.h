// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS_KEYSTORE_SERVICE_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS_KEYSTORE_SERVICE_UTIL_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/ash/components/platform_keys/keystore_types.h"

namespace chromeos::keystore_service_util {

// The WebCrypto string for ECDSA.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS)
extern const char kWebCryptoEcdsa[];

// The WebCrypto string for RSASSA-PKCS1-v1_5.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS)
extern const char kWebCryptoRsassaPkcs1v15[];

// The WebCrypto string for RSA-OAEP.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS)
extern const char kWebCryptoRsaOaep[];

// The WebCrypto string for the P-256 named curve.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS)
extern const char kWebCryptoNamedCurveP256[];

// Converts a keystore algorithm into a WebCrypto dictionary. Returns
// std::nullopt on error.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS)
std::optional<base::DictValue> MakeDictionaryFromKeystoreAlgorithm(
    const chromeos::KeystoreAlgorithm& algorithm);

// Converts a WebCrypto dictionary into a keystore algorithm. Returns
// std::nullopt on error.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS)
std::optional<chromeos::KeystoreAlgorithm> MakeKeystoreAlgorithmFromDictionary(
    const base::DictValue& dictionary);

// Creates the RSASSA-PKCS1-v1_5 variant of the KeystoreAlgorithm variant
// and populates the modulus_length and sw_backed fields with the provided
// values.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS)
chromeos::KeystoreAlgorithm MakeRsassaPkcs1v15KeystoreAlgorithm(
    unsigned int modulus_length,
    bool sw_backed);

// Creates the ECDSA variant of the KeystoreAlgorithm variant and populates
// the named_curve field with the provided value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS)
chromeos::KeystoreAlgorithm MakeEcdsaKeystoreAlgorithm(
    const std::string& named_curve);

// Creates the RSA-OAEP variant of the KeystoreAlgorithm variant and
// populates the modulus_length and sw_backed fields with the provided values.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS)
chromeos::KeystoreAlgorithm MakeRsaOaepKeystoreAlgorithm(
    unsigned int modulus_length,
    bool sw_backed);

}  // namespace chromeos::keystore_service_util

#endif  // CHROMEOS_ASH_COMPONENTS_PLATFORM_KEYS_KEYSTORE_SERVICE_UTIL_H_
