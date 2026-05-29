// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"

#include <memory>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config_internal.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_verification_tokens {

namespace internal {

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     IsRegistrableDomain_Valid) {
  EXPECT_TRUE(IsRegistrableDomain("example.com"));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     IsRegistrableDomain_Subdomain) {
  EXPECT_FALSE(IsRegistrableDomain("sub.example.com"));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     IsRegistrableDomain_TldOnly) {
  EXPECT_FALSE(IsRegistrableDomain("com"));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     IsRegistrableDomain_Empty) {
  EXPECT_FALSE(IsRegistrableDomain(""));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest, GetValidVersion_Valid) {
  base::DictValue dict;
  dict.Set(kVersionKey, base::Value(1));
  std::optional<int> version = GetValidVersion(dict);
  EXPECT_THAT(version, testing::Optional(1));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidVersion_Invalid) {
  base::DictValue dict;
  dict.Set(kVersionKey, base::Value(2));
  std::optional<int> version = GetValidVersion(dict);
  EXPECT_FALSE(version.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidVersion_InvalidType) {
  base::DictValue dict;
  dict.Set(kVersionKey, base::Value("1"));  // String, not integer
  std::optional<int> version = GetValidVersion(dict);
  EXPECT_FALSE(version.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidVersion_Missing) {
  base::DictValue dict;
  std::optional<int> version = GetValidVersion(dict);
  EXPECT_FALSE(version.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetDecodedPublicKey_Valid) {
  base::DictValue dict;
  const std::vector<uint8_t> expected_bytes = {1, 2, 3};
  dict.Set(kPublicKeyKey, base::Value(base::Base64Encode(expected_bytes)));
  std::optional<std::vector<uint8_t>> bytes = GetDecodedPublicKey(dict);
  ASSERT_TRUE(bytes.has_value());
  EXPECT_EQ(*bytes, expected_bytes);
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetDecodedPublicKey_InvalidBase64) {
  base::DictValue dict;
  dict.Set(kPublicKeyKey, base::Value("invalid-base64-!@#$"));
  std::optional<std::vector<uint8_t>> bytes = GetDecodedPublicKey(dict);
  EXPECT_FALSE(bytes.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetDecodedPublicKey_InvalidType) {
  base::DictValue dict;
  dict.Set(kPublicKeyKey, base::Value(123));  // Number, not string
  std::optional<std::vector<uint8_t>> bytes = GetDecodedPublicKey(dict);
  EXPECT_FALSE(bytes.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetDecodedPublicKey_Missing) {
  base::DictValue dict;
  std::optional<std::vector<uint8_t>> bytes = GetDecodedPublicKey(dict);
  EXPECT_FALSE(bytes.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest, GetValidKeyId_Valid) {
  base::DictValue dict;
  dict.Set(kKeyIdKey, base::Value(3));
  std::optional<uint32_t> key_id = GetValidKeyId(dict);
  EXPECT_THAT(key_id, testing::Optional(3u));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidKeyId_Negative) {
  base::DictValue dict;
  dict.Set(kKeyIdKey, base::Value(-1));
  std::optional<uint32_t> key_id = GetValidKeyId(dict);
  EXPECT_FALSE(key_id.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidKeyId_InvalidType) {
  base::DictValue dict;
  dict.Set(kKeyIdKey, base::Value("3"));
  std::optional<uint32_t> key_id = GetValidKeyId(dict);
  EXPECT_FALSE(key_id.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest, GetValidKeyId_Missing) {
  base::DictValue dict;
  std::optional<uint32_t> key_id = GetValidKeyId(dict);
  EXPECT_FALSE(key_id.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidBatchSize_Valid) {
  base::DictValue dict;
  dict.Set(kBatchSizeKey, base::Value(3));
  PrivateVerificationTokensParameters params{.min_batch_size = 2,
                                             .max_batch_size = 5};
  std::optional<int> batch_size = GetValidBatchSize(dict, params);
  EXPECT_THAT(batch_size, testing::Optional(3));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidBatchSize_TooSmall) {
  base::DictValue dict;
  dict.Set(kBatchSizeKey, base::Value(1));
  PrivateVerificationTokensParameters params{.min_batch_size = 2,
                                             .max_batch_size = 5};
  std::optional<int> batch_size = GetValidBatchSize(dict, params);
  EXPECT_FALSE(batch_size.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidBatchSize_TooLarge) {
  base::DictValue dict;
  dict.Set(kBatchSizeKey, base::Value(6));
  PrivateVerificationTokensParameters params{.min_batch_size = 2,
                                             .max_batch_size = 5};
  std::optional<int> batch_size = GetValidBatchSize(dict, params);
  EXPECT_FALSE(batch_size.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidBatchSize_InvalidType) {
  base::DictValue dict;
  dict.Set(kBatchSizeKey, base::Value("3"));  // String, not integer
  PrivateVerificationTokensParameters params{.min_batch_size = 2,
                                             .max_batch_size = 5};
  std::optional<int> batch_size = GetValidBatchSize(dict, params);
  EXPECT_FALSE(batch_size.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidBatchSize_Missing) {
  base::DictValue dict;
  PrivateVerificationTokensParameters params{.min_batch_size = 2,
                                             .max_batch_size = 5};
  std::optional<int> batch_size = GetValidBatchSize(dict, params);
  EXPECT_FALSE(batch_size.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidExpiration_Valid) {
  base::DictValue dict;
  dict.Set(kExpirationKey, base::Value("1234567890123456"));
  std::optional<int64_t> expiration = GetValidExpiration(dict);
  EXPECT_THAT(expiration, testing::Optional(1234567890123456LL));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidExpiration_InvalidString) {
  base::DictValue dict;
  dict.Set(kExpirationKey, base::Value("abc"));
  std::optional<int64_t> expiration = GetValidExpiration(dict);
  EXPECT_FALSE(expiration.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidExpiration_Negative) {
  base::DictValue dict;
  dict.Set(kExpirationKey, base::Value("-123"));
  std::optional<int64_t> expiration = GetValidExpiration(dict);
  EXPECT_FALSE(expiration.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidExpiration_NotAString) {
  base::DictValue dict;
  dict.Set(kExpirationKey, base::Value(123456));  // Number, not string
  std::optional<int64_t> expiration = GetValidExpiration(dict);
  EXPECT_FALSE(expiration.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidExpiration_Missing) {
  base::DictValue dict;
  std::optional<int64_t> expiration = GetValidExpiration(dict);
  EXPECT_FALSE(expiration.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest, ParseEntry_Valid) {
  base::DictValue entry;
  entry.Set(kDomainKey, base::Value("example.com"));
  entry.Set(kVersionKey, base::Value(1));
  entry.Set(kKeyIdKey, base::Value(3));
  entry.Set(kPublicKeyKey, base::Value(base::Base64Encode("some-pvt-key")));
  entry.Set(kBatchSizeKey, base::Value(3));
  entry.Set(kExpirationKey, base::Value("12"));

  auto result = ParseEntry(entry);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->batch_size, 3);
  EXPECT_EQ(result->public_key.etld_plus_one(), "example.com");
}

struct MissingFieldTestCase {
  std::string field_to_remove;
};

class PrivateVerificationTokensIssuerConfigInternalMissingFieldTest
    : public testing::TestWithParam<MissingFieldTestCase> {};

TEST_P(PrivateVerificationTokensIssuerConfigInternalMissingFieldTest,
       MissingField) {
  const auto& test_case = GetParam();

  base::DictValue entry;
  entry.Set(kDomainKey, base::Value("example.com"));
  entry.Set(kVersionKey, base::Value(1));
  entry.Set(kKeyIdKey, base::Value(3));
  entry.Set(kPublicKeyKey, base::Value(base::Base64Encode("some-pvt-key")));
  entry.Set(kBatchSizeKey, base::Value(3));
  entry.Set(kExpirationKey, base::Value("12"));
  entry.Remove(test_case.field_to_remove);
  auto result = ParseEntry(entry);
  EXPECT_FALSE(result.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PrivateVerificationTokensIssuerConfigInternalMissingFieldTest,
    testing::Values(MissingFieldTestCase{kDomainKey},
                    MissingFieldTestCase{kVersionKey},
                    MissingFieldTestCase{kPublicKeyKey},
                    MissingFieldTestCase{kKeyIdKey},
                    MissingFieldTestCase{kBatchSizeKey},
                    MissingFieldTestCase{kExpirationKey}));

}  // namespace internal

class PrivateVerificationTokensIssuerConfigTest : public testing::Test {
 public:
  void GetDictFromJSON(const std::string& json) {
    std::optional<base::Value> value = base::JSONReader::Read(json, 0);
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(value->is_dict());
    config_dict_ = value->GetDict().Clone();
  }

  base::DictValue config_dict_;
};

TEST_F(PrivateVerificationTokensIssuerConfigTest,
       Create_EmptyIssuersList_ReturnsEmptyConfig) {
  std::string json_str = R"({
    "issuers": []
  })";
  GetDictFromJSON(json_str);
  auto config =
      PrivateVerificationTokensIssuerConfig::Create(std::move(config_dict_));
  EXPECT_THAT(config, testing::NotNull());
  EXPECT_THAT(config->config(), testing::IsEmpty());
}

TEST_F(PrivateVerificationTokensIssuerConfigTest,
       Create_MissingIssuersKey_ReturnsNull) {
  std::string json_str = R"({
    "version": 1
  })";  // Missing "issuers" key
  GetDictFromJSON(json_str);
  auto config =
      PrivateVerificationTokensIssuerConfig::Create(std::move(config_dict_));
  EXPECT_THAT(config, testing::IsNull());
}

TEST_F(PrivateVerificationTokensIssuerConfigTest,
       Create_ValidArgument_SuccessSingleIssuer) {
  const std::string domain = "example.com";
  const uint32_t key_id = 3;
  const std::vector<uint8_t> serialized_public_key = {3, 6, 8, 12, 14};
  const std::string encoded_public_key =
      base::Base64Encode(serialized_public_key);
  const std::string expiration_str = "12";
  const uint64_t version = 1;
  const std::string json_str = base::StringPrintf(
      R"({
    "issuers": [
      {
        "domain": "%s",
        "version": 1,
        "key_id": %u,
        "public_key": "%s",
        "batch_size": 3,
        "expiration": "%s"
      }
    ]
  })",
      domain.c_str(), key_id, encoded_public_key.c_str(),
      expiration_str.c_str());
  GetDictFromJSON(json_str);
  std::unique_ptr<PrivateVerificationTokensIssuerConfig> config =
      PrivateVerificationTokensIssuerConfig::Create(std::move(config_dict_));
  EXPECT_THAT(config, testing::NotNull());
  EXPECT_THAT(config->config(), testing::SizeIs(1));

  PrivateVerificationTokensPublicKey expected_public_key{
      domain, serialized_public_key, key_id,
      base::Time::UnixEpoch() + base::Seconds(12), version};
  const auto& parsed_issuer_config = config->config().at(domain);
  EXPECT_EQ(parsed_issuer_config.batch_size, 3);
  EXPECT_EQ(parsed_issuer_config.public_key, expected_public_key);
}

TEST_F(PrivateVerificationTokensIssuerConfigTest,
       Create_ValidArgument_SuccessMultipleIssuers) {
  const std::vector<uint8_t> serialized_public_key1 = {3, 6, 8, 12, 14};
  const std::string encoded_public_key1 =
      base::Base64Encode(serialized_public_key1);
  const std::vector<uint8_t> serialized_public_key2 = {22, 11, 37, 43, 54, 65};
  const std::string encoded_public_key2 =
      base::Base64Encode(serialized_public_key2);
  const std::string json_str = base::StringPrintf(
      R"({
    "issuers": [
      {
        "domain": "a.com",
        "version": 1,
        "key_id": 2,
        "public_key": "%s",
        "batch_size": 3,
        "expiration": "49"
      },
      {
        "domain": "b.com",
        "version": 1,
        "key_id": 5,
        "public_key": "%s",
        "batch_size": 5,
        "expiration": "53"
      }
    ]
  })",
      encoded_public_key1.c_str(), encoded_public_key2.c_str());
  GetDictFromJSON(json_str);
  std::unique_ptr<PrivateVerificationTokensIssuerConfig> config =
      PrivateVerificationTokensIssuerConfig::Create(std::move(config_dict_));
  EXPECT_THAT(config, testing::NotNull());
  EXPECT_THAT(config->config(), testing::SizeIs(2));

  PrivateVerificationTokensPublicKey expected_pk1{
      "a.com", serialized_public_key1, 2,
      base::Time::UnixEpoch() + base::Seconds(49), 1};
  const auto& config1 = config->config().at("a.com");
  EXPECT_EQ(config1.batch_size, 3);
  EXPECT_EQ(config1.public_key, expected_pk1);

  PrivateVerificationTokensPublicKey expected_pk2{
      "b.com", serialized_public_key2, 5,
      base::Time::UnixEpoch() + base::Seconds(53), 1};
  const auto& config2 = config->config().at("b.com");
  EXPECT_EQ(config2.batch_size, 5);
  EXPECT_EQ(config2.public_key, expected_pk2);
}

TEST_F(PrivateVerificationTokensIssuerConfigTest,
       Create_MixedArguments_SkipsInvalidEntry) {
  const std::vector<uint8_t> serialized_public_key1 = {3, 6, 8, 12, 14};
  const std::string encoded_public_key1 =
      base::Base64Encode(serialized_public_key1);
  const std::string json_str = base::StringPrintf(
      R"({
    "issuers": [
      {
        "domain": "valid.com",
        "version": 1,
        "key_id": 2,
        "public_key": "%s",
        "batch_size": 3,
        "expiration": "49"
      },
      {
        "domain": "invalid.com",
        "version": 2,
        "key_id": 5,
        "public_key": "Cg==",
        "batch_size": 5,
        "expiration": "53"
      }
    ]
  })",
      encoded_public_key1.c_str());
  GetDictFromJSON(json_str);
  std::unique_ptr<PrivateVerificationTokensIssuerConfig> config =
      PrivateVerificationTokensIssuerConfig::Create(std::move(config_dict_));
  EXPECT_THAT(config, testing::NotNull());
  EXPECT_THAT(config->config(), testing::SizeIs(1));

  PrivateVerificationTokensPublicKey expected_pk1{
      "valid.com", serialized_public_key1, 2,
      base::Time::UnixEpoch() + base::Seconds(49), 1};
  const auto& config1 = config->config().at("valid.com");
  EXPECT_EQ(config1.batch_size, 3);
  EXPECT_EQ(config1.public_key, expected_pk1);

  EXPECT_FALSE(config->config().contains("invalid.com"));
}

TEST_F(PrivateVerificationTokensIssuerConfigTest,
       Create_ValidArgument_DuplicateIssuers) {
  const std::vector<uint8_t> serialized_public_key = {3, 6, 8, 12, 14};
  const std::string encoded_public_key =
      base::Base64Encode(serialized_public_key);
  // Contains duplicate entry for a.com
  const std::string json_str = base::StringPrintf(
      R"({
    "issuers": [
      {
        "domain": "a.com",
        "version": 1,
        "key_id": 2,
        "public_key": "%s",
        "batch_size": 3,
        "expiration": "49"
      },
      {
        "domain": "b.com",
        "version": 1,
        "key_id": 5,
        "public_key": "%s",
        "batch_size": 5,
        "expiration": "53"
      },
      {
        "domain": "a.com",
        "version": 2,
        "key_id": 3,
        "public_key": "%s",
        "batch_size": 7,
        "expiration": "62"
      }
    ]
  })",
      encoded_public_key.c_str(), encoded_public_key.c_str(),
      encoded_public_key.c_str());
  GetDictFromJSON(json_str);
  std::unique_ptr<PrivateVerificationTokensIssuerConfig> config =
      PrivateVerificationTokensIssuerConfig::Create(std::move(config_dict_));
  EXPECT_THAT(config, testing::NotNull());
  EXPECT_THAT(config->config(), testing::SizeIs(2));

  // Verify first a.com entry is picked
  PrivateVerificationTokensPublicKey expected_pk1{
      "a.com", serialized_public_key, 2,
      base::Time::UnixEpoch() + base::Seconds(49), 1};
  const auto& config1 = config->config().at("a.com");
  EXPECT_EQ(config1.batch_size, 3);
  EXPECT_EQ(config1.public_key, expected_pk1);

  PrivateVerificationTokensPublicKey expected_pk2{
      "b.com", serialized_public_key, 5,
      base::Time::UnixEpoch() + base::Seconds(53), 1};
  const auto& config2 = config->config().at("b.com");
  EXPECT_EQ(config2.batch_size, 5);
  EXPECT_EQ(config2.public_key, expected_pk2);
}

TEST_F(PrivateVerificationTokensIssuerConfigTest, LoadFromFile_EmptyPath) {
  auto result =
      PrivateVerificationTokensIssuerConfig::LoadFromFile(base::FilePath());
  EXPECT_FALSE(result);
}

TEST_F(PrivateVerificationTokensIssuerConfigTest, LoadFromFile_FileNotFound) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("test_config.json"));
  auto result = PrivateVerificationTokensIssuerConfig::LoadFromFile(path);
  EXPECT_FALSE(result);
}

TEST_F(PrivateVerificationTokensIssuerConfigTest, LoadFromFile_InvalidJson) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("test_config.json"));
  ASSERT_TRUE(base::WriteFile(path, "invalid json"));
  auto result = PrivateVerificationTokensIssuerConfig::LoadFromFile(path);
  EXPECT_FALSE(result);
}

TEST_F(PrivateVerificationTokensIssuerConfigTest, LoadFromFile_EmptyFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("test_config.json"));
  ASSERT_TRUE(base::WriteFile(path, ""));
  auto result = PrivateVerificationTokensIssuerConfig::LoadFromFile(path);
  EXPECT_FALSE(result);
}

TEST_F(PrivateVerificationTokensIssuerConfigTest, LoadFromFile_ValidJson) {
  const std::vector<uint8_t> serialized_public_key = {3, 6, 8, 12, 14};
  const std::string encoded_public_key =
      base::Base64Encode(serialized_public_key);
  const std::string json_str = base::StringPrintf(
      R"({
    "issuers": [
      {
        "domain": "example.com",
        "version": 1,
        "key_id": 3,
        "public_key": "%s",
        "batch_size": 3,
        "expiration": "12"
      }
    ]
  })",
      encoded_public_key.c_str());
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("test_config.json"));
  ASSERT_TRUE(base::WriteFile(path, json_str));
  auto result = PrivateVerificationTokensIssuerConfig::LoadFromFile(path);
  EXPECT_TRUE(result);
  EXPECT_THAT(result->config(), testing::SizeIs(1));
  EXPECT_TRUE(result->config().contains("example.com"));

  EXPECT_EQ(result->config().at("example.com").batch_size, 3);

  const PrivateVerificationTokensPublicKey expected_public_key{
      "example.com", serialized_public_key, 3,
      base::Time::UnixEpoch() + base::Seconds(12), 1};
  EXPECT_EQ(result->config().at("example.com").public_key, expected_public_key);
}

}  // namespace private_verification_tokens
