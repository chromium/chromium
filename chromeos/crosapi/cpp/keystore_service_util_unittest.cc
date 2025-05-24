// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/keystore_service_util.h"

#include <optional>

#include "base/numerics/safe_math.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace keystore_service_util {

static constexpr unsigned int kModulusLength = 1024;
static constexpr bool kSoftwareBacked = true;

// Equals to 65537.
static constexpr uint8_t kDefaultPublicExponent[] = {0x01, 0x00, 0x01};

TEST(KeystoreServiceUtil, EcdsaDictionary) {
  base::Value::Dict value;
  value.Set("name", kWebCryptoEcdsa);
  value.Set("namedCurve", kWebCryptoNamedCurveP256);

  std::optional<crosapi::mojom::KeystoreAlgorithmPtr> ptr =
      MakeKeystoreAlgorithmFromDictionary(value);
  ASSERT_TRUE(ptr);

  std::optional<base::Value::Dict> value2 =
      MakeDictionaryFromKeystoreAlgorithm(ptr.value());
  ASSERT_TRUE(value2);

  EXPECT_EQ(value, value2);
}

TEST(KeystoreServiceUtil, RsassaPkcs1v15Dictionary) {
  base::Value::Dict value;
  value.Set("name", kWebCryptoRsassaPkcs1v15);
  value.Set("modulusLength", base::checked_cast<int>(kModulusLength));

  value.Set("publicExponent",
            base::Value::BlobStorage(std::begin(kDefaultPublicExponent),
                                     std::end(kDefaultPublicExponent)));

  std::optional<crosapi::mojom::KeystoreAlgorithmPtr> ptr =
      MakeKeystoreAlgorithmFromDictionary(value);
  ASSERT_TRUE(ptr);

  std::optional<base::Value::Dict> value2 =
      MakeDictionaryFromKeystoreAlgorithm(ptr.value());
  ASSERT_TRUE(value2);

  EXPECT_EQ(value, value2);
}

TEST(KeystoreServiceUtil, RsaOaepDictionary) {
  base::Value::Dict value;
  value.Set("name", kWebCryptoRsaOaep);
  value.Set("modulusLength", base::checked_cast<int>(kModulusLength));

  value.Set("publicExponent",
            base::Value::BlobStorage(std::begin(kDefaultPublicExponent),
                                     std::end(kDefaultPublicExponent)));

  std::optional<crosapi::mojom::KeystoreAlgorithmPtr> ptr =
      MakeKeystoreAlgorithmFromDictionary(value);
  ASSERT_TRUE(ptr);

  std::optional<base::Value::Dict> value2 =
      MakeDictionaryFromKeystoreAlgorithm(ptr.value());
  ASSERT_TRUE(value2);

  EXPECT_EQ(value, value2);
}

TEST(KeystoreServiceUtil, MakeRsassaPkcs1v15KeystoreAlgorithm) {
  crosapi::mojom::KeystoreAlgorithmPtr algorithm_ptr =
      MakeRsassaPkcs1v15KeystoreAlgorithm(kModulusLength, kSoftwareBacked);

  EXPECT_EQ(algorithm_ptr->which(),
            crosapi::mojom::KeystoreAlgorithm::Tag::kRsassaPkcs115);
  EXPECT_EQ(algorithm_ptr->get_rsassa_pkcs115()->modulus_length,
            kModulusLength);
  EXPECT_TRUE(algorithm_ptr->get_rsassa_pkcs115()->sw_backed);
}

TEST(KeystoreServiceUtil, MakeRsaOaepKeystoreAlgorithm) {
  crosapi::mojom::KeystoreAlgorithmPtr algorithm_ptr =
      MakeRsaOaepKeystoreAlgorithm(kModulusLength, kSoftwareBacked);

  EXPECT_EQ(algorithm_ptr->which(),
            crosapi::mojom::KeystoreAlgorithm::Tag::kRsaOaep);
  EXPECT_EQ(algorithm_ptr->get_rsa_oaep()->modulus_length, kModulusLength);
  EXPECT_TRUE(algorithm_ptr->get_rsa_oaep()->sw_backed);
}

TEST(KeystoreServiceUtil, MakeEcdsaKeystoreAlgorithm) {
  crosapi::mojom::KeystoreAlgorithmPtr algorithm_ptr =
      MakeEcdsaKeystoreAlgorithm(kWebCryptoNamedCurveP256);

  EXPECT_EQ(algorithm_ptr->which(),
            crosapi::mojom::KeystoreAlgorithm::Tag::kEcdsa);
  EXPECT_EQ(algorithm_ptr->get_ecdsa()->named_curve, kWebCryptoNamedCurveP256);
}

}  // namespace keystore_service_util
}  // namespace crosapi
