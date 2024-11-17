// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/keystore_service_util.h"

#include <optional>
#include <vector>

#include "base/numerics/safe_math.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"

namespace crosapi {
namespace keystore_service_util {

const char kWebCryptoEcdsa[] = "ECDSA";
const char kWebCryptoRsassaPkcs1v15[] = "RSASSA-PKCS1-v1_5";
const char kWebCryptoRsaOaep[] = "RSA-OAEP";
const char kWebCryptoNamedCurveP256[] = "P-256";

// Converts a keystore algorithm into a WebCrypto dictionary.
std::optional<base::Value::Dict> MakeDictionaryFromKeystoreAlgorithm(
    const crosapi::mojom::KeystoreAlgorithmPtr& algorithm) {
  base::Value::Dict value;
  switch (algorithm->which()) {
    case crosapi::mojom::KeystoreAlgorithm::Tag::kRsassaPkcs115:
      value.Set("name", kWebCryptoRsassaPkcs1v15);

      if (!base::IsValueInRangeForNumericType<int>(
              algorithm->get_rsassa_pkcs115()->modulus_length)) {
        return std::nullopt;
      }

      value.Set(
          "modulusLength",
          static_cast<int>(algorithm->get_rsassa_pkcs115()->modulus_length));

      if (!algorithm->get_rsassa_pkcs115()->public_exponent) {
        return std::nullopt;
      }
      value.Set("publicExponent",
                base::Value::BlobStorage(
                    algorithm->get_rsassa_pkcs115()->public_exponent.value()));
      return value;
    case crosapi::mojom::KeystoreAlgorithm::Tag::kEcdsa:
      value.Set("name", kWebCryptoEcdsa);
      value.Set("namedCurve", algorithm->get_ecdsa()->named_curve);
      return value;
    case crosapi::mojom::KeystoreAlgorithm::Tag::kRsaOaep:
      value.Set("name", kWebCryptoRsaOaep);

      if (!base::IsValueInRangeForNumericType<int>(
              algorithm->get_rsa_oaep()->modulus_length)) {
        return std::nullopt;
      }

      value.Set("modulusLength",
                static_cast<int>(algorithm->get_rsa_oaep()->modulus_length));

      if (!algorithm->get_rsa_oaep()->public_exponent) {
        return std::nullopt;
      }
      value.Set("publicExponent",
                base::Value::BlobStorage(
                    algorithm->get_rsa_oaep()->public_exponent.value()));
      return value;
    default:
      return std::nullopt;
  }
}

// Converts a WebCrypto dictionary into a keystore algorithm.
std::optional<crosapi::mojom::KeystoreAlgorithmPtr>
MakeKeystoreAlgorithmFromDictionary(const base::Value::Dict& dictionary) {
  const std::string* name = dictionary.FindString("name");
  if (!name) {
    return std::nullopt;
  }

  if (*name == kWebCryptoRsassaPkcs1v15) {
    std::optional<int> modulus_length = dictionary.FindInt("modulusLength");
    const std::vector<uint8_t>* public_exponent =
        dictionary.FindBlob("publicExponent");
    if (!modulus_length || !public_exponent) {
      return std::nullopt;
    }
    if (!base::IsValueInRangeForNumericType<uint32_t>(modulus_length.value())) {
      return std::nullopt;
    }
    crosapi::mojom::KeystoreRsaParamsPtr params =
        crosapi::mojom::KeystoreRsaParams::New();
    params->modulus_length =
        base::checked_cast<uint32_t>(modulus_length.value());
    params->public_exponent = *public_exponent;
    return crosapi::mojom::KeystoreAlgorithm::NewRsassaPkcs115(
        std::move(params));
  }

  if (*name == kWebCryptoEcdsa) {
    const std::string* named_curve = dictionary.FindString("namedCurve");
    if (!named_curve) {
      return std::nullopt;
    }
    crosapi::mojom::KeystoreEcdsaParamsPtr params =
        crosapi::mojom::KeystoreEcdsaParams::New();
    params->named_curve = *named_curve;
    return crosapi::mojom::KeystoreAlgorithm::NewEcdsa(std::move(params));
  }

  if (*name == kWebCryptoRsaOaep) {
    std::optional<int> modulus_length = dictionary.FindInt("modulusLength");
    const std::vector<uint8_t>* public_exponent =
        dictionary.FindBlob("publicExponent");
    if (!modulus_length || !public_exponent) {
      return std::nullopt;
    }
    if (!base::IsValueInRangeForNumericType<uint32_t>(modulus_length.value())) {
      return std::nullopt;
    }
    crosapi::mojom::KeystoreRsaParamsPtr params =
        crosapi::mojom::KeystoreRsaParams::New();
    params->modulus_length =
        base::checked_cast<uint32_t>(modulus_length.value());
    params->public_exponent = *public_exponent;
    return crosapi::mojom::KeystoreAlgorithm::NewRsaOaep(std::move(params));
  }

  return std::nullopt;
}

mojom::KeystoreAlgorithmPtr MakeRsassaPkcs1v15KeystoreAlgorithm(
    unsigned int modulus_length,
    bool sw_backed) {
  mojom::KeystoreRsaParamsPtr params = mojom::KeystoreRsaParams::New();
  params->modulus_length = modulus_length;
  params->sw_backed = sw_backed;
  return mojom::KeystoreAlgorithm::NewRsassaPkcs115(std::move(params));
}

mojom::KeystoreAlgorithmPtr MakeEcdsaKeystoreAlgorithm(
    const std::string& named_curve) {
  mojom::KeystoreEcdsaParamsPtr params = mojom::KeystoreEcdsaParams::New();
  params->named_curve = named_curve;
  return mojom::KeystoreAlgorithm::NewEcdsa(std::move(params));
}

mojom::KeystoreAlgorithmPtr MakeRsaOaepKeystoreAlgorithm(
    unsigned int modulus_length,
    bool sw_backed) {
  mojom::KeystoreRsaParamsPtr params = mojom::KeystoreRsaParams::New();
  params->modulus_length = modulus_length;
  params->sw_backed = sw_backed;
  return mojom::KeystoreAlgorithm::NewRsaOaep(std::move(params));
}

}  // namespace keystore_service_util
}  // namespace crosapi
