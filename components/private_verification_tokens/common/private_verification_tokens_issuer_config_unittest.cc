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
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config_internal.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace private_verification_tokens {

namespace internal {

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

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetDecodedPublicKeyProof_Valid) {
  base::DictValue dict;
  const std::vector<uint8_t> expected_bytes = {4, 5, 6};
  dict.Set(kPublicKeyProofKey, base::Value(base::Base64Encode(expected_bytes)));
  std::optional<std::vector<uint8_t>> bytes = GetDecodedPublicKeyProof(dict);
  ASSERT_TRUE(bytes.has_value());
  EXPECT_EQ(*bytes, expected_bytes);
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetDecodedPublicKeyProof_Missing) {
  base::DictValue dict;
  std::optional<std::vector<uint8_t>> bytes = GetDecodedPublicKeyProof(dict);
  EXPECT_FALSE(bytes.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidRedeemers_Valid) {
  PrivateVerificationTokensParameters params{.max_number_of_redeemers = 2};
  base::DictValue dict;
  base::ListValue list;
  list.Append("https://s1.example.com");
  list.Append("https://s2.example.com");
  dict.Set(kRedeemersKey, std::move(list));
  std::optional<std::vector<url::Origin>> redeemers =
      GetValidRedeemers(dict, "example.com", params);
  ASSERT_TRUE(redeemers.has_value());
  EXPECT_THAT(*redeemers,
              testing::ElementsAre(
                  url::Origin::Create(GURL("https://s1.example.com")),
                  url::Origin::Create(GURL("https://s2.example.com"))));
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidRedeemers_TooManyOrigins) {
  PrivateVerificationTokensParameters params{.max_number_of_redeemers = 2};
  base::DictValue dict;
  base::ListValue list;
  for (int i = 0; i < 3; ++i) {
    list.Append(base::StringPrintf("https://s%d.example.com", i));
  }
  dict.Set(kRedeemersKey, std::move(list));
  std::optional<std::vector<url::Origin>> redeemers =
      GetValidRedeemers(dict, "example.com", params);
  EXPECT_FALSE(redeemers.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidRedeemers_NonHttpsScheme) {
  PrivateVerificationTokensParameters params{.max_number_of_redeemers = 2};
  base::DictValue dict;
  base::ListValue list;
  list.Append("http://s1.example.com");
  dict.Set(kRedeemersKey, std::move(list));
  std::optional<std::vector<url::Origin>> redeemers =
      GetValidRedeemers(dict, "example.com", params);
  EXPECT_FALSE(redeemers.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidRedeemers_DifferentEtldPlusOne) {
  PrivateVerificationTokensParameters params{.max_number_of_redeemers = 2};
  base::DictValue dict;
  base::ListValue list;
  list.Append("https://s1.other.com");
  dict.Set(kRedeemersKey, std::move(list));
  std::optional<std::vector<url::Origin>> redeemers =
      GetValidRedeemers(dict, "example.com", params);
  EXPECT_FALSE(redeemers.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest,
     GetValidRedeemers_Missing) {
  PrivateVerificationTokensParameters params{.max_number_of_redeemers = 2};
  base::DictValue dict;
  std::optional<std::vector<url::Origin>> redeemers =
      GetValidRedeemers(dict, "example.com", params);
  EXPECT_FALSE(redeemers.has_value());
}

TEST(PrivateVerificationTokensIssuerConfigInternalTest, ParseEntry_Valid) {
  base::DictValue entry;
  entry.Set(kIssuerRequestUrlKey, base::Value("https://example.com/pvt/issue"));
  entry.Set(kVersionKey, base::Value(1));
  entry.Set(kPublicKeyKey, base::Value(base::Base64Encode("some-pvt-key")));
  entry.Set(kPublicKeyProofKey,
            base::Value(base::Base64Encode("some-pvt-proof")));
  entry.Set(kBatchSizeKey, base::Value(3));
  entry.Set(kExpirationKey, base::Value("12"));
  base::ListValue redeemers_list;
  redeemers_list.Append("https://s1.example.com");
  entry.Set(kRedeemersKey, std::move(redeemers_list));
  entry.Set(kDeploymentIdKey, base::Value("test-deployment-id"));

  auto result = ParseEntry(entry);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->issuer_request_url, GURL("https://example.com/pvt/issue"));
  EXPECT_EQ(result->batch_size, 3);
  EXPECT_EQ(result->public_key.issuer(),
            url::Origin::Create(GURL("https://example.com")));
  EXPECT_EQ(result->public_key.key_id(), 27u);
  EXPECT_THAT(result->public_key.public_key_proof(),
              testing::ElementsAre('s', 'o', 'm', 'e', '-', 'p', 'v', 't', '-',
                                   'p', 'r', 'o', 'o', 'f'));
  EXPECT_EQ(result->redeemers.size(), 1u);
  EXPECT_EQ(result->redeemers[0],
            url::Origin::Create(GURL("https://s1.example.com")));
  EXPECT_EQ(result->deployment_id, "test-deployment-id");
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
  entry.Set(kIssuerRequestUrlKey, base::Value("https://example.com/pvt/issue"));
  entry.Set(kVersionKey, base::Value(1));
  entry.Set(kPublicKeyKey, base::Value(base::Base64Encode("some-pvt-key")));
  entry.Set(kPublicKeyProofKey,
            base::Value(base::Base64Encode("some-pvt-proof")));
  entry.Set(kBatchSizeKey, base::Value(3));
  entry.Set(kExpirationKey, base::Value("12"));
  base::ListValue redeemers_list;
  redeemers_list.Append("https://s1.example.com");
  entry.Set(kRedeemersKey, std::move(redeemers_list));
  entry.Set(kDeploymentIdKey, base::Value("test-deployment-id"));
  entry.Remove(test_case.field_to_remove);
  auto result = ParseEntry(entry);
  EXPECT_FALSE(result.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PrivateVerificationTokensIssuerConfigInternalMissingFieldTest,
    testing::Values(MissingFieldTestCase{kIssuerRequestUrlKey},
                    MissingFieldTestCase{kVersionKey},
                    MissingFieldTestCase{kPublicKeyKey},
                    MissingFieldTestCase{kPublicKeyProofKey},
                    MissingFieldTestCase{kBatchSizeKey},
                    MissingFieldTestCase{kExpirationKey},
                    MissingFieldTestCase{kRedeemersKey},
                    MissingFieldTestCase{kDeploymentIdKey}));

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
  const url::Origin issuer = url::Origin::Create(GURL("https://example.com"));
  const std::vector<uint8_t> serialized_public_key = {3, 6, 8, 12, 14};
  const std::string encoded_public_key =
      base::Base64Encode(serialized_public_key);
  const std::vector<uint8_t> serialized_public_key_proof = {1, 2, 4, 8};
  const std::string encoded_public_key_proof =
      base::Base64Encode(serialized_public_key_proof);
  const std::string expiration_str = "12";
  const uint64_t version = 1;
  const std::string json_str = base::StringPrintf(
      R"({
    "issuers": [
      {
        "issuerRequestUrl": "https://example.com/pvt/issue",
        "version": 1,
        "publicKey": "%s",
        "publicKeyProof": "%s",
        "batchSize": 3,
        "expiration": "%s",
        "redeemers": ["https://s1.example.com", "https://s2.example.com"],
        "deploymentId": "test-deployment-id"
      }
    ]
  })",
      encoded_public_key.c_str(), encoded_public_key_proof.c_str(),
      expiration_str.c_str());
  GetDictFromJSON(json_str);
  scoped_refptr<PrivateVerificationTokensIssuerConfig> config =
      PrivateVerificationTokensIssuerConfig::Create(std::move(config_dict_));
  EXPECT_THAT(config, testing::NotNull());
  EXPECT_THAT(config->config(), testing::SizeIs(1));

  PrivateVerificationTokensPublicKey expected_public_key{
      issuer, serialized_public_key, serialized_public_key_proof,
      base::Time::UnixEpoch() + base::Seconds(12), version};
  const auto& parsed_issuer_config = config->config().at(issuer);
  EXPECT_EQ(parsed_issuer_config.issuer_request_url,
            GURL("https://example.com/pvt/issue"));
  EXPECT_EQ(parsed_issuer_config.batch_size, 3);
  EXPECT_EQ(parsed_issuer_config.public_key, expected_public_key);
  EXPECT_THAT(parsed_issuer_config.redeemers,
              testing::ElementsAre(
                  url::Origin::Create(GURL("https://s1.example.com")),
                  url::Origin::Create(GURL("https://s2.example.com"))));
  EXPECT_EQ(parsed_issuer_config.deployment_id, "test-deployment-id");
}

TEST_F(PrivateVerificationTokensIssuerConfigTest,
       Create_ValidArgument_SuccessMultipleIssuers) {
  const std::vector<uint8_t> serialized_public_key1 = {3, 6, 8, 12, 14};
  const std::string encoded_public_key1 =
      base::Base64Encode(serialized_public_key1);
  const std::vector<uint8_t> serialized_public_key2 = {22, 11, 37, 43, 54, 65};
  const std::string encoded_public_key2 =
      base::Base64Encode(serialized_public_key2);
  const std::vector<uint8_t> serialized_proof = {1, 2, 3};
  const std::string encoded_proof = base::Base64Encode(serialized_proof);
  const std::string json_str = base::StringPrintf(
      R"({
    "issuers": [
      {
        "issuerRequestUrl": "https://a.com/pvt/issue",
        "version": 1,
        "publicKey": "%s",
        "publicKeyProof": "%s",
        "batchSize": 3,
        "expiration": "49",
        "redeemers": ["https://sub1.a.com"],
        "deploymentId": "dep-a"
      },
      {
        "issuerRequestUrl": "https://b.com/pvt/issue",
        "version": 1,
        "publicKey": "%s",
        "publicKeyProof": "%s",
        "batchSize": 5,
        "expiration": "53",
        "redeemers": ["https://sub1.b.com"],
        "deploymentId": "dep-b"
      }
    ]
  })",
      encoded_public_key1.c_str(), encoded_proof.c_str(),
      encoded_public_key2.c_str(), encoded_proof.c_str());
  GetDictFromJSON(json_str);
  scoped_refptr<PrivateVerificationTokensIssuerConfig> config =
      PrivateVerificationTokensIssuerConfig::Create(std::move(config_dict_));
  EXPECT_THAT(config, testing::NotNull());
  EXPECT_THAT(config->config(), testing::SizeIs(2));

  const url::Origin a_origin = url::Origin::Create(GURL("https://a.com"));
  const url::Origin b_origin = url::Origin::Create(GURL("https://b.com"));

  PrivateVerificationTokensPublicKey expected_pk1{
      a_origin, serialized_public_key1, serialized_proof,
      base::Time::UnixEpoch() + base::Seconds(49), 1};
  const auto& config1 = config->config().at(a_origin);
  EXPECT_EQ(config1.issuer_request_url, GURL("https://a.com/pvt/issue"));
  EXPECT_EQ(config1.batch_size, 3);
  EXPECT_EQ(config1.public_key, expected_pk1);
  EXPECT_THAT(
      config1.redeemers,
      testing::ElementsAre(url::Origin::Create(GURL("https://sub1.a.com"))));
  EXPECT_EQ(config1.deployment_id, "dep-a");

  PrivateVerificationTokensPublicKey expected_pk2{
      b_origin, serialized_public_key2, serialized_proof,
      base::Time::UnixEpoch() + base::Seconds(53), 1};
  const auto& config2 = config->config().at(b_origin);
  EXPECT_EQ(config2.issuer_request_url, GURL("https://b.com/pvt/issue"));
  EXPECT_EQ(config2.batch_size, 5);
  EXPECT_EQ(config2.public_key, expected_pk2);
  EXPECT_THAT(
      config2.redeemers,
      testing::ElementsAre(url::Origin::Create(GURL("https://sub1.b.com"))));
  EXPECT_EQ(config2.deployment_id, "dep-b");
}

TEST_F(PrivateVerificationTokensIssuerConfigTest,
       Create_MixedArguments_SkipsInvalidEntry) {
  const std::vector<uint8_t> serialized_public_key1 = {3, 6, 8, 12, 14};
  const std::string encoded_public_key1 =
      base::Base64Encode(serialized_public_key1);
  const std::vector<uint8_t> serialized_proof = {1, 2, 3};
  const std::string encoded_proof = base::Base64Encode(serialized_proof);
  const std::string json_str = base::StringPrintf(
      R"({
    "issuers": [
      {
        "issuerRequestUrl": "https://valid.com/pvt/issue",
        "version": 1,
        "publicKey": "%s",
        "publicKeyProof": "%s",
        "batchSize": 3,
        "expiration": "49",
        "redeemers": ["https://sub.valid.com"],
        "deploymentId": "athm-dep"
      },
      {
        "issuerRequestUrl": "https://invalid.com/pvt/issue",
        "version": 2,
        "publicKey": "Cg==",
        "publicKeyProof": "Cg==",
        "batchSize": 5,
        "expiration": "53",
        "redeemers": ["https://sub.invalid.com"],
        "deploymentId": "test-dep"
      }
    ]
  })",
      encoded_public_key1.c_str(), encoded_proof.c_str());
  GetDictFromJSON(json_str);
  scoped_refptr<PrivateVerificationTokensIssuerConfig> config =
      PrivateVerificationTokensIssuerConfig::Create(std::move(config_dict_));
  EXPECT_THAT(config, testing::NotNull());
  EXPECT_THAT(config->config(), testing::SizeIs(1));

  const url::Origin valid_origin =
      url::Origin::Create(GURL("https://valid.com"));
  const url::Origin invalid_origin =
      url::Origin::Create(GURL("https://invalid.com"));

  PrivateVerificationTokensPublicKey expected_pk1{
      valid_origin, serialized_public_key1, serialized_proof,
      base::Time::UnixEpoch() + base::Seconds(49), 1};
  const auto& config1 = config->config().at(valid_origin);
  EXPECT_EQ(config1.batch_size, 3);
  EXPECT_EQ(config1.public_key, expected_pk1);

  EXPECT_FALSE(config->config().contains(invalid_origin));
}

TEST_F(PrivateVerificationTokensIssuerConfigTest,
       Create_ValidArgument_DuplicateIssuers) {
  const std::vector<uint8_t> serialized_public_key = {3, 6, 8, 12, 14};
  const std::string encoded_public_key =
      base::Base64Encode(serialized_public_key);
  const std::vector<uint8_t> serialized_proof = {1, 2, 3};
  const std::string encoded_proof = base::Base64Encode(serialized_proof);
  // Contains duplicate entry for a.com
  const std::string json_str = base::StringPrintf(
      R"({
    "issuers": [
      {
        "issuerRequestUrl": "https://a.com/pvt/issue",
        "version": 1,
        "publicKey": "%s",
        "publicKeyProof": "%s",
        "batchSize": 3,
        "expiration": "49",
        "redeemers": ["https://sub.a.com"],
        "deploymentId": "dep-a"
      },
      {
        "issuerRequestUrl": "https://b.com/pvt/issue",
        "version": 1,
        "publicKey": "%s",
        "publicKeyProof": "%s",
        "batchSize": 5,
        "expiration": "53",
        "redeemers": ["https://sub.b.com"],
        "deploymentId": "dep-b"
      },
      {
        "issuerRequestUrl": "https://a.com/pvt/issue",
        "version": 2,
        "publicKey": "%s",
        "publicKeyProof": "%s",
        "batchSize": 7,
        "expiration": "62",
        "redeemers": ["https://sub.a.com"],
        "deploymentId": "dep-a-v2"
      }
    ]
  })",
      encoded_public_key.c_str(), encoded_proof.c_str(),
      encoded_public_key.c_str(), encoded_proof.c_str(),
      encoded_public_key.c_str(), encoded_proof.c_str());
  GetDictFromJSON(json_str);
  scoped_refptr<PrivateVerificationTokensIssuerConfig> config =
      PrivateVerificationTokensIssuerConfig::Create(std::move(config_dict_));
  EXPECT_THAT(config, testing::NotNull());
  EXPECT_THAT(config->config(), testing::SizeIs(2));

  const url::Origin a_origin = url::Origin::Create(GURL("https://a.com"));
  const url::Origin b_origin = url::Origin::Create(GURL("https://b.com"));

  // Verify first a.com entry is picked
  PrivateVerificationTokensPublicKey expected_pk1{
      a_origin, serialized_public_key, serialized_proof,
      base::Time::UnixEpoch() + base::Seconds(49), 1};
  const auto& config1 = config->config().at(a_origin);
  EXPECT_EQ(config1.batch_size, 3);
  EXPECT_EQ(config1.public_key, expected_pk1);

  PrivateVerificationTokensPublicKey expected_pk2{
      b_origin, serialized_public_key, serialized_proof,
      base::Time::UnixEpoch() + base::Seconds(53), 1};
  const auto& config2 = config->config().at(b_origin);
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
  const std::vector<uint8_t> serialized_proof = {1, 2, 3};
  const std::string encoded_proof = base::Base64Encode(serialized_proof);
  const std::string json_str = base::StringPrintf(
      R"({
    "1": {
      "issuers": [
        {
          "issuerRequestUrl": "https://example.com/pvt/issue",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 3,
          "expiration": "12",
          "redeemers": ["https://s1.example.com", "https://s2.example.com"],
          "deploymentId": "test-deployment-id"
        }
      ]
    }
  })",
      encoded_public_key.c_str(), encoded_proof.c_str());
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("test_config.json"));
  ASSERT_TRUE(base::WriteFile(path, json_str));
  auto result = PrivateVerificationTokensIssuerConfig::LoadFromFile(path);
  EXPECT_TRUE(result);
  EXPECT_THAT(result->config(), testing::SizeIs(1));
  const url::Origin expected_origin =
      url::Origin::Create(GURL("https://example.com"));
  EXPECT_TRUE(result->config().contains(expected_origin));

  EXPECT_EQ(result->config().at(expected_origin).issuer_request_url,
            GURL("https://example.com/pvt/issue"));
  EXPECT_EQ(result->config().at(expected_origin).batch_size, 3);

  const PrivateVerificationTokensPublicKey expected_public_key{
      expected_origin, serialized_public_key, serialized_proof,
      base::Time::UnixEpoch() + base::Seconds(12), 1};
  EXPECT_EQ(result->config().at(expected_origin).public_key,
            expected_public_key);
  EXPECT_THAT(result->config().at(expected_origin).redeemers,
              testing::ElementsAre(
                  url::Origin::Create(GURL("https://s1.example.com")),
                  url::Origin::Create(GURL("https://s2.example.com"))));
}

TEST_F(PrivateVerificationTokensIssuerConfigTest,
       CreateWithCustomIssuer_Valid) {
  const std::vector<uint8_t> serialized_public_key = {1, 2, 3, 4};
  const std::string encoded_public_key =
      base::Base64Encode(serialized_public_key);
  const std::vector<uint8_t> serialized_proof = {5, 6, 7};
  const std::string encoded_proof = base::Base64Encode(serialized_proof);
  const std::string json_entry = base::StringPrintf(
      R"({
        "issuerRequestUrl": "https://custom.example.com/issue",
        "version": 1,
        "publicKey": "%s",
        "publicKeyProof": "%s",
        "batchSize": 4,
        "expiration": "2147483647",
        "redeemers": ["https://s1.custom.example.com"],
        "deploymentId": "test-deployment-id"
      })",
      encoded_public_key.c_str(), encoded_proof.c_str());

  std::optional<base::Value> value = base::JSONReader::Read(json_entry, 0);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());

  scoped_refptr<const PrivateVerificationTokensIssuerConfig> config =
      PrivateVerificationTokensIssuerConfig::CreateWithCustomIssuer(
          nullptr, value->GetDict().Clone());
  ASSERT_THAT(config, testing::NotNull());
  EXPECT_THAT(config->config(), testing::SizeIs(1));

  const url::Origin custom_origin =
      url::Origin::Create(GURL("https://custom.example.com"));
  EXPECT_TRUE(config->config().contains(custom_origin));
  const auto& issuer_config = config->config().at(custom_origin);
  EXPECT_EQ(issuer_config.issuer_request_url,
            GURL("https://custom.example.com/issue"));
  EXPECT_EQ(issuer_config.batch_size, 4);
  EXPECT_EQ(issuer_config.deployment_id, "test-deployment-id");
  EXPECT_THAT(issuer_config.redeemers,
              testing::ElementsAre(
                  url::Origin::Create(GURL("https://s1.custom.example.com"))));

  const PrivateVerificationTokensPublicKey expected_public_key{
      custom_origin, serialized_public_key, serialized_proof,
      base::Time::UnixEpoch() + base::Seconds(2147483647), 1};
  EXPECT_EQ(issuer_config.public_key, expected_public_key);
}

TEST_F(PrivateVerificationTokensIssuerConfigTest,
       CreateWithCustomIssuer_MergeWithBaseConfig) {
  const std::vector<uint8_t> pk1 = {1, 2, 3};
  const std::string encoded_pk1 = base::Base64Encode(pk1);
  const std::vector<uint8_t> proof1 = {4, 5, 6};
  const std::string encoded_proof1 = base::Base64Encode(proof1);
  const std::string base_json = base::StringPrintf(
      R"({
    "1": {
      "issuers": [
        {
          "issuerRequestUrl": "https://base.com/issue",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 3,
          "expiration": "2147483647",
          "redeemers": ["https://sub.base.com"],
          "deploymentId": "dep1"
        }
      ]
    }
  })",
      encoded_pk1.c_str(), encoded_proof1.c_str());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("base_config.json"));
  ASSERT_TRUE(base::WriteFile(path, base_json));
  scoped_refptr<PrivateVerificationTokensIssuerConfig> base_config =
      PrivateVerificationTokensIssuerConfig::LoadFromFile(path);
  ASSERT_THAT(base_config, testing::NotNull());

  const std::vector<uint8_t> pk2 = {7, 8, 9};
  const std::string encoded_pk2 = base::Base64Encode(pk2);
  const std::vector<uint8_t> proof2 = {10, 11, 12};
  const std::string encoded_proof2 = base::Base64Encode(proof2);
  const std::string custom_entry = base::StringPrintf(
      R"({
        "issuerRequestUrl": "https://custom.com/issue",
        "version": 1,
        "publicKey": "%s",
        "publicKeyProof": "%s",
        "batchSize": 5,
        "expiration": "2147483647",
        "redeemers": ["https://sub.custom.com"],
        "deploymentId": "dep2"
      })",
      encoded_pk2.c_str(), encoded_proof2.c_str());

  std::optional<base::Value> value = base::JSONReader::Read(custom_entry, 0);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());

  scoped_refptr<const PrivateVerificationTokensIssuerConfig> merged_config =
      PrivateVerificationTokensIssuerConfig::CreateWithCustomIssuer(
          base_config, value->GetDict().Clone());
  ASSERT_THAT(merged_config, testing::NotNull());
  EXPECT_THAT(merged_config->config(), testing::SizeIs(2));

  const url::Origin base_origin = url::Origin::Create(GURL("https://base.com"));
  const url::Origin custom_origin =
      url::Origin::Create(GURL("https://custom.com"));
  EXPECT_TRUE(merged_config->config().contains(base_origin));
  EXPECT_TRUE(merged_config->config().contains(custom_origin));

  // Verify base config entry fields
  const auto& base_entry = merged_config->config().at(base_origin);
  EXPECT_EQ(base_entry.issuer_request_url, GURL("https://base.com/issue"));
  EXPECT_EQ(base_entry.batch_size, 3);
  EXPECT_EQ(base_entry.deployment_id, "dep1");
  EXPECT_THAT(
      base_entry.redeemers,
      testing::ElementsAre(url::Origin::Create(GURL("https://sub.base.com"))));
  const PrivateVerificationTokensPublicKey expected_base_pk{
      base_origin, pk1, proof1,
      base::Time::UnixEpoch() + base::Seconds(2147483647), 1};
  EXPECT_EQ(base_entry.public_key, expected_base_pk);

  // Verify custom config entry fields
  const auto& custom_entry_cfg = merged_config->config().at(custom_origin);
  EXPECT_EQ(custom_entry_cfg.issuer_request_url,
            GURL("https://custom.com/issue"));
  EXPECT_EQ(custom_entry_cfg.batch_size, 5);
  EXPECT_EQ(custom_entry_cfg.deployment_id, "dep2");
  EXPECT_THAT(custom_entry_cfg.redeemers,
              testing::ElementsAre(
                  url::Origin::Create(GURL("https://sub.custom.com"))));
  const PrivateVerificationTokensPublicKey expected_custom_pk{
      custom_origin, pk2, proof2,
      base::Time::UnixEpoch() + base::Seconds(2147483647), 1};
  EXPECT_EQ(custom_entry_cfg.public_key, expected_custom_pk);
}

TEST_F(PrivateVerificationTokensIssuerConfigTest,
       CreateWithCustomIssuer_InvalidDict) {
  const std::string invalid_json_entry = R"({
    "origin": "https://invalid.example.com"
  })";

  std::optional<base::Value> value =
      base::JSONReader::Read(invalid_json_entry, 0);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());

  scoped_refptr<const PrivateVerificationTokensIssuerConfig> config =
      PrivateVerificationTokensIssuerConfig::CreateWithCustomIssuer(
          nullptr, value->GetDict().Clone());
  EXPECT_THAT(config, testing::IsNull());
}

}  // namespace private_verification_tokens
