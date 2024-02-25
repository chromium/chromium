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

// The WebCrypto string for PKCS1.
COMPONENT_EXPORT(CROSAPI)
extern const char kWebCryptoRsassaPkcs1v15[];

// The WebCrypto string for the P-256 named curve.
COMPONENT_EXPORT(CROSAPI)
extern const char kWebCryptoNamedCurveP256[];

// Converts a crosapi signing algorithm into a WebCrypto dictionary. Returns
// std::nullopt on error.
COMPONENT_EXPORT(CROSAPI)
std::optional<base::Value::Dict> DictionaryFromSigningAlgorithm(
    const mojom::KeystoreSigningAlgorithmPtr& algorithm);

// Converts a WebCrypto dictionary into a crosapi signing algorithm. Returns
// std::nullopt on error.
COMPONENT_EXPORT(CROSAPI)
std::optional<mojom::KeystoreSigningAlgorithmPtr>
SigningAlgorithmFromDictionary(const base::Value::Dict& dictionary);

// Creates the KeystorePKCS115Params variant of the KeystoreSigningAlgorithm
// union and populates the modulus_length field with |modulus_length|.
COMPONENT_EXPORT(CROSAPI)
mojom::KeystoreSigningAlgorithmPtr MakeRsaKeystoreSigningAlgorithm(
    unsigned int modulus_length,
    bool sw_backed);

// Creates the KeystoreECDSAParams variant of the KeystoreSigningAlgorithm
// union and populates the named_curve field with |modulus_length|.
COMPONENT_EXPORT(CROSAPI)
mojom::KeystoreSigningAlgorithmPtr MakeEcKeystoreSigningAlgorithm(
    const std::string& named_curve);

}  // namespace keystore_service_util
}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_KEYSTORE_SERVICE_UTIL_H_
