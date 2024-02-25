// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/keystore_service_util.h"

#include <optional>

#include "base/numerics/safe_math.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"

namespace crosapi {
namespace keystore_service_util {

const char kWebCryptoEcdsa[] = "ECDSA";
const char kWebCryptoRsassaPkcs1v15[] = "RSASSA-PKCS1-v1_5";
const char kWebCryptoNamedCurveP256[] = "P-256";

// Converts a signing algorithm into a WebCrypto dictionary.
std::optional<base::Value::Dict> DictionaryFromSigningAlgorithm(
    const crosapi::mojom::KeystoreSigningAlgorithmPtr& algorithm) {
  base::Value::Dict value;
  switch (algorithm->which()) {
    case crosapi::mojom::KeystoreSigningAlgorithm::Tag::kPkcs115:
      value.Set("name", kWebCryptoRsassaPkcs1v15);

      if (!base::IsValueInRangeForNumericType<int>(
              algorithm->get_pkcs115()->modulus_length)) {
        return std::nullopt;
      }

      value.Set("modulusLength",
                static_cast<int>(algorithm->get_pkcs115()->modulus_length));

      if (!algorithm->get_pkcs115()->public_exponent) {
        return std::nullopt;
      }
      value.Set("publicExponent",
                base::Value::BlobStorage(
                    algorithm->get_pkcs115()->public_exponent.value()));
      return value;
    case crosapi::mojom::KeystoreSigningAlgorithm::Tag::kEcdsa:
      value.Set("name", kWebCryptoEcdsa);
      value.Set("namedCurve", algorithm->get_ecdsa()->named_curve);
      return value;
    default:
      return std::nullopt;
  }
}

std::optional<crosapi::mojom::KeystoreSigningAlgorithmPtr>
SigningAlgorithmFromDictionary(const base::Value::Dict& dictionary) {
  const std::string* name = dictionary.FindString("name");
  if (!name)
    return std::nullopt;

  if (*name == kWebCryptoRsassaPkcs1v15) {
    std::optional<int> modulus_length = dictionary.FindInt("modulusLength");
    const std::vector<uint8_t>* public_exponent =
        dictionary.FindBlob("publicExponent");
    if (!modulus_length || !public_exponent)
      return std::nullopt;
    if (!base::IsValueInRangeForNumericType<uint32_t>(modulus_length.value()))
      return std::nullopt;
    crosapi::mojom::KeystorePKCS115ParamsPtr params =
        crosapi::mojom::KeystorePKCS115Params::New();
    params->modulus_length =
        base::checked_cast<uint32_t>(modulus_length.value());
    params->public_exponent = *public_exponent;
    return crosapi::mojom::KeystoreSigningAlgorithm::NewPkcs115(
        std::move(params));
  }

  if (*name == kWebCryptoEcdsa) {
    const std::string* named_curve = dictionary.FindString("namedCurve");
    if (!named_curve)
      return std::nullopt;
    crosapi::mojom::KeystoreECDSAParamsPtr params =
        crosapi::mojom::KeystoreECDSAParams::New();
    params->named_curve = *named_curve;
    return crosapi::mojom::KeystoreSigningAlgorithm::NewEcdsa(
        std::move(params));
  }

  return std::nullopt;
}

mojom::KeystoreSigningAlgorithmPtr MakeRsaKeystoreSigningAlgorithm(
    unsigned int modulus_length,
    bool sw_backed) {
  mojom::KeystorePKCS115ParamsPtr params = mojom::KeystorePKCS115Params::New();
  params->modulus_length = modulus_length;
  params->sw_backed = sw_backed;
  return mojom::KeystoreSigningAlgorithm::NewPkcs115(std::move(params));
}

mojom::KeystoreSigningAlgorithmPtr MakeEcKeystoreSigningAlgorithm(
    const std::string& named_curve) {
  mojom::KeystoreECDSAParamsPtr params = mojom::KeystoreECDSAParams::New();
  params->named_curve = named_curve;
  return mojom::KeystoreSigningAlgorithm::NewEcdsa(std::move(params));
}

}  // namespace keystore_service_util
}  // namespace crosapi
