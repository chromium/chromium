// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/platform_keys/keystore_service_util.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "base/values.h"
#include "chromeos/ash/components/platform_keys/keystore_types.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace chromeos::keystore_service_util {

const char kWebCryptoEcdsa[] = "ECDSA";
const char kWebCryptoRsassaPkcs1v15[] = "RSASSA-PKCS1-v1_5";
const char kWebCryptoRsaOaep[] = "RSA-OAEP";
const char kWebCryptoNamedCurveP256[] = "P-256";

std::optional<base::DictValue> MakeDictionaryFromKeystoreAlgorithm(
    const chromeos::KeystoreAlgorithm& algorithm) {
  return std::visit(
      absl::Overload{
          [](const chromeos::RsassaPkcs115Params& params)
              -> std::optional<base::DictValue> {
            base::DictValue value;
            value.Set("name", kWebCryptoRsassaPkcs1v15);
            if (!base::IsValueInRangeForNumericType<int>(
                    params.rsa_params.modulus_length)) {
              return std::nullopt;
            }
            value.Set("modulusLength",
                      static_cast<int>(params.rsa_params.modulus_length));
            if (!params.rsa_params.public_exponent) {
              return std::nullopt;
            }
            value.Set("publicExponent",
                      base::Value::BlobStorage(
                          params.rsa_params.public_exponent.value()));
            return value;
          },
          [](const chromeos::KeystoreEcdsaParams& params)
              -> std::optional<base::DictValue> {
            base::DictValue value;
            value.Set("name", kWebCryptoEcdsa);
            value.Set("namedCurve", params.named_curve);
            return value;
          },
          [](const chromeos::RsaOaepParams& params)
              -> std::optional<base::DictValue> {
            base::DictValue value;
            value.Set("name", kWebCryptoRsaOaep);
            if (!base::IsValueInRangeForNumericType<int>(
                    params.rsa_params.modulus_length)) {
              return std::nullopt;
            }
            value.Set("modulusLength",
                      static_cast<int>(params.rsa_params.modulus_length));
            if (!params.rsa_params.public_exponent) {
              return std::nullopt;
            }
            value.Set("publicExponent",
                      base::Value::BlobStorage(
                          params.rsa_params.public_exponent.value()));
            return value;
          },
      },
      algorithm);
}

// Converts a WebCrypto dictionary into a keystore algorithm.
std::optional<chromeos::KeystoreAlgorithm> MakeKeystoreAlgorithmFromDictionary(
    const base::DictValue& dictionary) {
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
    chromeos::RsassaPkcs115Params params;
    params.rsa_params.modulus_length =
        base::checked_cast<uint32_t>(modulus_length.value());
    params.rsa_params.public_exponent = *public_exponent;
    return params;
  }

  if (*name == kWebCryptoEcdsa) {
    const std::string* named_curve = dictionary.FindString("namedCurve");
    if (!named_curve) {
      return std::nullopt;
    }
    chromeos::KeystoreEcdsaParams params;
    params.named_curve = *named_curve;
    return params;
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
    chromeos::RsaOaepParams params;
    params.rsa_params.modulus_length =
        base::checked_cast<uint32_t>(modulus_length.value());
    params.rsa_params.public_exponent = *public_exponent;
    return params;
  }

  return std::nullopt;
}

chromeos::KeystoreAlgorithm MakeRsassaPkcs1v15KeystoreAlgorithm(
    unsigned int modulus_length,
    bool sw_backed) {
  chromeos::RsassaPkcs115Params params;
  params.rsa_params.modulus_length = modulus_length;
  params.rsa_params.sw_backed = sw_backed;
  return params;
}

chromeos::KeystoreAlgorithm MakeEcdsaKeystoreAlgorithm(
    const std::string& named_curve) {
  chromeos::KeystoreEcdsaParams params;
  params.named_curve = named_curve;
  return params;
}

chromeos::KeystoreAlgorithm MakeRsaOaepKeystoreAlgorithm(
    unsigned int modulus_length,
    bool sw_backed) {
  chromeos::RsaOaepParams params;
  params.rsa_params.modulus_length = modulus_length;
  params.rsa_params.sw_backed = sw_backed;
  return params;
}

}  // namespace chromeos::keystore_service_util
