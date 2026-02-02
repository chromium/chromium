// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/platform_keys/keystore_service_util.h"

#include <optional>
#include <variant>

#include "base/numerics/safe_math.h"
#include "base/values.h"
#include "chromeos/ash/components/platform_keys/keystore_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace keystore_service_util {

static constexpr unsigned int kModulusLength = 1024;
static constexpr bool kSoftwareBacked = true;

// Equals to 65537.
static constexpr uint8_t kDefaultPublicExponent[] = {0x01, 0x00, 0x01};

TEST(KeystoreServiceUtil, EcdsaDictionary) {
  base::DictValue value;
  value.Set("name", kWebCryptoEcdsa);
  value.Set("namedCurve", kWebCryptoNamedCurveP256);

  std::optional<chromeos::KeystoreAlgorithm> ptr =
      MakeKeystoreAlgorithmFromDictionary(value);
  ASSERT_TRUE(ptr);

  std::optional<base::DictValue> value2 =
      MakeDictionaryFromKeystoreAlgorithm(ptr.value());
  ASSERT_TRUE(value2);

  EXPECT_EQ(value, value2);
}

TEST(KeystoreServiceUtil, RsassaPkcs1v15Dictionary) {
  base::DictValue value;
  value.Set("name", kWebCryptoRsassaPkcs1v15);
  value.Set("modulusLength", base::checked_cast<int>(kModulusLength));

  value.Set("publicExponent", base::Value(base::Value::BlobStorage(
                                  std::begin(kDefaultPublicExponent),
                                  std::end(kDefaultPublicExponent))));

  std::optional<chromeos::KeystoreAlgorithm> ptr =
      MakeKeystoreAlgorithmFromDictionary(value);
  ASSERT_TRUE(ptr);

  std::optional<base::DictValue> value2 =
      MakeDictionaryFromKeystoreAlgorithm(ptr.value());
  ASSERT_TRUE(value2);

  EXPECT_EQ(value, value2);
}

TEST(KeystoreServiceUtil, RsaOaepDictionary) {
  base::DictValue value;
  value.Set("name", kWebCryptoRsaOaep);
  value.Set("modulusLength", base::checked_cast<int>(kModulusLength));

  value.Set("publicExponent", base::Value(base::Value::BlobStorage(
                                  std::begin(kDefaultPublicExponent),
                                  std::end(kDefaultPublicExponent))));

  std::optional<chromeos::KeystoreAlgorithm> ptr =
      MakeKeystoreAlgorithmFromDictionary(value);
  ASSERT_TRUE(ptr);

  std::optional<base::DictValue> value2 =
      MakeDictionaryFromKeystoreAlgorithm(ptr.value());
  ASSERT_TRUE(value2);

  EXPECT_EQ(value, value2);
}

TEST(KeystoreServiceUtil, MakeRsassaPkcs1v15KeystoreAlgorithm) {
  chromeos::KeystoreAlgorithm algorithm =
      MakeRsassaPkcs1v15KeystoreAlgorithm(kModulusLength, kSoftwareBacked);

  ASSERT_TRUE(std::holds_alternative<chromeos::RsassaPkcs115Params>(algorithm));
  const auto& params = std::get<chromeos::RsassaPkcs115Params>(algorithm);
  EXPECT_EQ(params.rsa_params.modulus_length, kModulusLength);
  EXPECT_TRUE(params.rsa_params.sw_backed);
}

TEST(KeystoreServiceUtil, MakeRsaOaepKeystoreAlgorithm) {
  chromeos::KeystoreAlgorithm algorithm =
      MakeRsaOaepKeystoreAlgorithm(kModulusLength, kSoftwareBacked);

  ASSERT_TRUE(std::holds_alternative<chromeos::RsaOaepParams>(algorithm));
  const auto& params = std::get<chromeos::RsaOaepParams>(algorithm);
  EXPECT_EQ(params.rsa_params.modulus_length, kModulusLength);
  EXPECT_TRUE(params.rsa_params.sw_backed);
}

TEST(KeystoreServiceUtil, MakeEcdsaKeystoreAlgorithm) {
  chromeos::KeystoreAlgorithm algorithm =
      MakeEcdsaKeystoreAlgorithm(kWebCryptoNamedCurveP256);

  ASSERT_TRUE(std::holds_alternative<chromeos::KeystoreEcdsaParams>(algorithm));
  const auto& params = std::get<chromeos::KeystoreEcdsaParams>(algorithm);
  EXPECT_EQ(params.named_curve, kWebCryptoNamedCurveP256);
}

}  // namespace keystore_service_util
}  // namespace chromeos
