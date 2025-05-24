// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_KEYSTORE_SERVICE_UTIL_H_
#define CHROMEOS_CROSAPI_CPP_KEYSTORE_SERVICE_UTIL_H_

#include <optional>

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"

namespace crosapi {
namespace keystore_service_util {

// The WebCrypto string for ECDSA.
COMPONENT_EXPORT(CROSAPI)
extern const char kWebCryptoEcdsa[];

// The WebCrypto string for RSASSA-PKCS1-v1_5.
COMPONENT_EXPORT(CROSAPI)
extern const char kWebCryptoRsassaPkcs1v15[];

// The WebCrypto string for RSA-OAEP.
COMPONENT_EXPORT(CROSAPI)
extern const char kWebCryptoRsaOaep[];

// The WebCrypto string for the P-256 named curve.
COMPONENT_EXPORT(CROSAPI)
extern const char kWebCryptoNamedCurveP256[];

// Converts a crosapi keystore algorithm into a WebCrypto dictionary. Returns
// std::nullopt on error.
COMPONENT_EXPORT(CROSAPI)
std::optional<base::Value::Dict> MakeDictionaryFromKeystoreAlgorithm(
    const mojom::KeystoreAlgorithmPtr& algorithm);

// Converts a WebCrypto dictionary into a crosapi keystore algorithm. Returns
// std::nullopt on error.
COMPONENT_EXPORT(CROSAPI)
std::optional<mojom::KeystoreAlgorithmPtr> MakeKeystoreAlgorithmFromDictionary(
    const base::Value::Dict& dictionary);

// Creates the RSASSA-PKCS1-v1_5 variant of the KeystoreAlgorithm union
// and populates the modulus_length and sw_backed fields with the provided
// values.
COMPONENT_EXPORT(CROSAPI)
mojom::KeystoreAlgorithmPtr MakeRsassaPkcs1v15KeystoreAlgorithm(
    unsigned int modulus_length,
    bool sw_backed);

// Creates the ECDSA variant of the KeystoreAlgorithm union and populates
// the named_curve field with the provided value.
COMPONENT_EXPORT(CROSAPI)
mojom::KeystoreAlgorithmPtr MakeEcdsaKeystoreAlgorithm(
    const std::string& named_curve);

// Creates the RSA-OAEP variant of the KeystoreAlgorithm union and
// populates the modulus_length and sw_backed fields with the provided values.
COMPONENT_EXPORT(CROSAPI)
mojom::KeystoreAlgorithmPtr MakeRsaOaepKeystoreAlgorithm(
    unsigned int modulus_length,
    bool sw_backed);

}  // namespace keystore_service_util
}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_KEYSTORE_SERVICE_UTIL_H_
