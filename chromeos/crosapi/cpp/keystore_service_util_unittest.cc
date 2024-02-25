// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/keystore_service_util.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace keystore_service_util {

TEST(KeystoreServiceUtil, ECDSA) {
  base::Value::Dict value;
  value.Set("name", kWebCryptoEcdsa);
  value.Set("namedCurve", kWebCryptoNamedCurveP256);

  std::optional<crosapi::mojom::KeystoreSigningAlgorithmPtr> ptr =
      SigningAlgorithmFromDictionary(value);
  ASSERT_TRUE(ptr);

  std::optional<base::Value::Dict> value2 =
      DictionaryFromSigningAlgorithm(ptr.value());
  ASSERT_TRUE(value2);

  EXPECT_EQ(value, value2);
}

TEST(KeystoreServiceUtil, PKCS) {
  base::Value::Dict value;
  value.Set("name", kWebCryptoRsassaPkcs1v15);
  value.Set("modulusLength", 5);

  // Equals 65537.
  static constexpr uint8_t kDefaultPublicExponent[] = {0x01, 0x00, 0x01};
  value.Set("publicExponent",
            base::Value::BlobStorage(std::begin(kDefaultPublicExponent),
                                     std::end(kDefaultPublicExponent)));

  std::optional<crosapi::mojom::KeystoreSigningAlgorithmPtr> ptr =
      SigningAlgorithmFromDictionary(value);
  ASSERT_TRUE(ptr);

  std::optional<base::Value::Dict> value2 =
      DictionaryFromSigningAlgorithm(ptr.value());
  ASSERT_TRUE(value2);

  EXPECT_EQ(value, value2);
}

}  // namespace keystore_service_util
}  // namespace crosapi
