// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"

#include <optional>
#include <string>

#include "base/base64.h"
#include "base/values.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config_internal.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_verification_tokens::internal {

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     IsRegistrableDomain_Valid) {
  EXPECT_TRUE(internal::IsRegistrableDomain("example.com"));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     IsRegistrableDomain_Subdomain) {
  EXPECT_FALSE(internal::IsRegistrableDomain("sub.example.com"));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     IsRegistrableDomain_TldOnly) {
  EXPECT_FALSE(internal::IsRegistrableDomain("com"));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     IsRegistrableDomain_Empty) {
  EXPECT_FALSE(internal::IsRegistrableDomain(""));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest, GetValidVersion_Valid) {
  base::DictValue dict;
  dict.Set(kVersionKey, base::Value(1));
  std::optional<int> version = internal::GetValidVersion(dict);
  EXPECT_THAT(version, testing::Optional(1));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidVersion_Invalid) {
  base::DictValue dict;
  dict.Set(kVersionKey, base::Value(2));
  std::optional<int> version = internal::GetValidVersion(dict);
  EXPECT_FALSE(version.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidVersion_InvalidType) {
  base::DictValue dict;
  dict.Set(kVersionKey, base::Value("1"));  // String, not integer
  std::optional<int> version = internal::GetValidVersion(dict);
  EXPECT_FALSE(version.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidVersion_Missing) {
  base::DictValue dict;
  std::optional<int> version = internal::GetValidVersion(dict);
  EXPECT_FALSE(version.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetDecodedPublicKey_Valid) {
  base::DictValue dict;
  const std::vector<uint8_t> expected_bytes = {1, 2, 3};
  dict.Set(kPublicKeyKey, base::Value(base::Base64Encode(expected_bytes)));
  std::optional<std::vector<uint8_t>> bytes =
      internal::GetDecodedPublicKey(dict);
  ASSERT_TRUE(bytes.has_value());
  EXPECT_EQ(*bytes, expected_bytes);
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetDecodedPublicKey_InvalidBase64) {
  base::DictValue dict;
  dict.Set(kPublicKeyKey, base::Value("invalid-base64-!@#$"));
  std::optional<std::vector<uint8_t>> bytes =
      internal::GetDecodedPublicKey(dict);
  EXPECT_FALSE(bytes.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetDecodedPublicKey_InvalidType) {
  base::DictValue dict;
  dict.Set(kPublicKeyKey, base::Value(123));  // Number, not string
  std::optional<std::vector<uint8_t>> bytes =
      internal::GetDecodedPublicKey(dict);
  EXPECT_FALSE(bytes.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetDecodedPublicKey_Missing) {
  base::DictValue dict;
  std::optional<std::vector<uint8_t>> bytes =
      internal::GetDecodedPublicKey(dict);
  EXPECT_FALSE(bytes.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest, GetValidKeyId_Valid) {
  base::DictValue dict;
  dict.Set(kKeyIdKey, base::Value(3));
  std::optional<uint32_t> key_id = internal::GetValidKeyId(dict);
  EXPECT_THAT(key_id, testing::Optional(3u));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidKeyId_Negative) {
  base::DictValue dict;
  dict.Set(kKeyIdKey, base::Value(-1));
  std::optional<uint32_t> key_id = internal::GetValidKeyId(dict);
  EXPECT_FALSE(key_id.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidKeyId_InvalidType) {
  base::DictValue dict;
  dict.Set(kKeyIdKey, base::Value("3"));
  std::optional<uint32_t> key_id = internal::GetValidKeyId(dict);
  EXPECT_FALSE(key_id.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest, GetValidKeyId_Missing) {
  base::DictValue dict;
  std::optional<uint32_t> key_id = internal::GetValidKeyId(dict);
  EXPECT_FALSE(key_id.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidBatchSize_Valid) {
  base::DictValue dict;
  dict.Set(kBatchSizeKey, base::Value(3));
  PrivateVerificationTokensParameters params{.min_batch_size = 2,
                                             .max_batch_size = 5};
  std::optional<int> batch_size = internal::GetValidBatchSize(dict, params);
  EXPECT_THAT(batch_size, testing::Optional(3));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidBatchSize_TooSmall) {
  base::DictValue dict;
  dict.Set(kBatchSizeKey, base::Value(1));
  PrivateVerificationTokensParameters params{.min_batch_size = 2,
                                             .max_batch_size = 5};
  std::optional<int> batch_size = internal::GetValidBatchSize(dict, params);
  EXPECT_FALSE(batch_size.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidBatchSize_TooLarge) {
  base::DictValue dict;
  dict.Set(kBatchSizeKey, base::Value(6));
  PrivateVerificationTokensParameters params{.min_batch_size = 2,
                                             .max_batch_size = 5};
  std::optional<int> batch_size = internal::GetValidBatchSize(dict, params);
  EXPECT_FALSE(batch_size.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidBatchSize_InvalidType) {
  base::DictValue dict;
  dict.Set(kBatchSizeKey, base::Value("3"));  // String, not integer
  PrivateVerificationTokensParameters params{.min_batch_size = 2,
                                             .max_batch_size = 5};
  std::optional<int> batch_size = internal::GetValidBatchSize(dict, params);
  EXPECT_FALSE(batch_size.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidBatchSize_Missing) {
  base::DictValue dict;
  PrivateVerificationTokensParameters params{.min_batch_size = 2,
                                             .max_batch_size = 5};
  std::optional<int> batch_size = internal::GetValidBatchSize(dict, params);
  EXPECT_FALSE(batch_size.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidExpiration_Valid) {
  base::DictValue dict;
  dict.Set(kExpirationKey, base::Value("1234567890123456"));
  std::optional<int64_t> expiration = internal::GetValidExpiration(dict);
  EXPECT_THAT(expiration, testing::Optional(1234567890123456LL));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidExpiration_InvalidString) {
  base::DictValue dict;
  dict.Set(kExpirationKey, base::Value("abc"));
  std::optional<int64_t> expiration = internal::GetValidExpiration(dict);
  EXPECT_FALSE(expiration.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidExpiration_Negative) {
  base::DictValue dict;
  dict.Set(kExpirationKey, base::Value("-123"));
  std::optional<int64_t> expiration = internal::GetValidExpiration(dict);
  EXPECT_FALSE(expiration.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidExpiration_NotAString) {
  base::DictValue dict;
  dict.Set(kExpirationKey, base::Value(123456));  // Number, not string
  std::optional<int64_t> expiration = internal::GetValidExpiration(dict);
  EXPECT_FALSE(expiration.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidExpiration_Missing) {
  base::DictValue dict;
  std::optional<int64_t> expiration = internal::GetValidExpiration(dict);
  EXPECT_FALSE(expiration.has_value());
}

}  // namespace private_verification_tokens::internal
