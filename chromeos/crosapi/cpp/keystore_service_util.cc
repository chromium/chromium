// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/keystore_service_util.h"

#include "base/numerics/safe_math.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crosapi {
namespace keystore_service_util {

const char kWebCryptoEcdsa[] = "ECDSA";
const char kWebCryptoRsassaPkcs1v15[] = "RSASSA-PKCS1-v1_5";
const char kWebCryptoNamedCurveP256[] = "P-256";

// Converts a signing algorithm into a WebCrypto dictionary.
absl::optional<base::DictionaryValue> DictionaryFromSigningAlgorithm(
    const crosapi::mojom::KeystoreSigningAlgorithmPtr& algorithm) {
  base::DictionaryValue value;
  switch (algorithm->which()) {
    case crosapi::mojom::KeystoreSigningAlgorithm::Tag::kPkcs115:
      value.SetStringKey("name", kWebCryptoRsassaPkcs1v15);

      if (!base::IsValueInRangeForNumericType<int>(
              algorithm->get_pkcs115()->modulus_length)) {
        return absl::nullopt;
      }

      value.SetKey("modulusLength",
                   base::Value(base::checked_cast<int>(
                       algorithm->get_pkcs115()->modulus_length)));

      if (!algorithm->get_pkcs115()->public_exponent) {
        return absl::nullopt;
      }
      value.SetKey(
          "publicExponent",
          base::Value(algorithm->get_pkcs115()->public_exponent.value()));
      break;
    case crosapi::mojom::KeystoreSigningAlgorithm::Tag::kEcdsa:
      value.SetStringKey("name", kWebCryptoEcdsa);
      value.SetStringKey("namedCurve", algorithm->get_ecdsa()->named_curve);
      break;
    default:
      return absl::nullopt;
  }
  return value;
}

absl::optional<crosapi::mojom::KeystoreSigningAlgorithmPtr>
SigningAlgorithmFromDictionary(const base::DictionaryValue& dictionary) {
  crosapi::mojom::KeystoreSigningAlgorithmPtr algorithm =
      crosapi::mojom::KeystoreSigningAlgorithm::New();

  const std::string* name = dictionary.FindStringKey("name");
  if (!name)
    return absl::nullopt;

  if (*name == kWebCryptoRsassaPkcs1v15) {
    absl::optional<int> modulus_length = dictionary.FindIntKey("modulusLength");
    const std::vector<uint8_t>* public_exponent =
        dictionary.FindBlobKey("publicExponent");
    if (!modulus_length || !public_exponent)
      return absl::nullopt;
    if (!base::IsValueInRangeForNumericType<uint32_t>(modulus_length.value()))
      return absl::nullopt;
    crosapi::mojom::KeystorePKCS115ParamsPtr params =
        crosapi::mojom::KeystorePKCS115Params::New();
    params->modulus_length =
        base::checked_cast<uint32_t>(modulus_length.value());
    params->public_exponent = *public_exponent;
    algorithm->set_pkcs115(std::move(params));
    return algorithm;
  }

  if (*name == kWebCryptoEcdsa) {
    const std::string* named_curve = dictionary.FindStringKey("namedCurve");
    if (!named_curve)
      return absl::nullopt;
    crosapi::mojom::KeystoreECDSAParamsPtr params =
        crosapi::mojom::KeystoreECDSAParams::New();
    params->named_curve = *named_curve;
    algorithm->set_ecdsa(std::move(params));
    return algorithm;
  }

  return absl::nullopt;
}

mojom::KeystoreSigningAlgorithmPtr MakeRsaKeystoreSigningAlgorithm(
    unsigned int modulus_length,
    bool sw_backed) {
  mojom::KeystoreSigningAlgorithmPtr algorithm =
      mojom::KeystoreSigningAlgorithm::New();
  mojom::KeystorePKCS115ParamsPtr params = mojom::KeystorePKCS115Params::New();
  params->modulus_length = modulus_length;
  params->sw_backed = sw_backed;
  algorithm->set_pkcs115(std::move(params));
  return algorithm;
}

mojom::KeystoreSigningAlgorithmPtr MakeEcKeystoreSigningAlgorithm(
    const std::string& named_curve) {
  mojom::KeystoreSigningAlgorithmPtr algorithm =
      mojom::KeystoreSigningAlgorithm::New();
  mojom::KeystoreECDSAParamsPtr params = mojom::KeystoreECDSAParams::New();
  params->named_curve = named_curve;
  algorithm->set_ecdsa(std::move(params));
  return algorithm;
}

}  // namespace keystore_service_util
}  // namespace crosapi
