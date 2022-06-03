// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/keystore_service_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crosapi {
namespace keystore_service_util {

TEST(KeystoreServiceUtil, ECDSA) {
  base::DictionaryValue value;
  value.SetStringKey("name", kWebCryptoEcdsa);
  value.SetStringKey("namedCurve", kWebCryptoNamedCurveP256);

  absl::optional<crosapi::mojom::KeystoreSigningAlgorithmPtr> ptr =
      SigningAlgorithmFromDictionary(value);
  ASSERT_TRUE(ptr);

  absl::optional<base::DictionaryValue> value2 =
      DictionaryFromSigningAlgorithm(ptr.value());
  ASSERT_TRUE(value2);

  EXPECT_EQ(value, value2);
}

TEST(KeystoreServiceUtil, PKCS) {
  base::DictionaryValue value;
  value.SetStringKey("name", kWebCryptoRsassaPkcs1v15);
  value.SetKey("modulusLength", base::Value(5));

  // Equals 65537.
  static constexpr uint8_t kDefaultPublicExponent[] = {0x01, 0x00, 0x01};
  value.SetKey("publicExponent",
               base::Value(base::make_span(kDefaultPublicExponent)));

  absl::optional<crosapi::mojom::KeystoreSigningAlgorithmPtr> ptr =
      SigningAlgorithmFromDictionary(value);
  ASSERT_TRUE(ptr);

  absl::optional<base::DictionaryValue> value2 =
      DictionaryFromSigningAlgorithm(ptr.value());
  ASSERT_TRUE(value2);

  EXPECT_EQ(value, value2);
}

}  // namespace keystore_service_util
}  // namespace crosapi
