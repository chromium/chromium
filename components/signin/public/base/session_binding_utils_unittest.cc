// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/session_binding_utils.h"

#include <optional>
#include <string_view>

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "base/value_iterators.h"
#include "base/values.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/hybrid_encryption_key_test_utils.h"
#include "crypto/signature_verifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;

namespace signin {

namespace {

base::Value Base64UrlEncodedJsonToValue(std::string_view input) {
  std::string json;
  EXPECT_TRUE(base::Base64UrlDecode(
      input, base::Base64UrlDecodePolicy::DISALLOW_PADDING, &json));
  std::optional<base::Value> result = base::JSONReader::Read(json);
  EXPECT_TRUE(result.has_value());
  return std::move(*result);
}

std::string Base64UrlEncode(std::string_view input) {
  std::string result;
  base::Base64UrlEncode(input, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &result);
  return result;
}

}  // namespace

TEST(SessionBindingUtilsTest, SignatureAlgorithmFromString) {
  using enum crypto::SignatureVerifier::SignatureAlgorithm;
  EXPECT_EQ(SignatureAlgorithmFromString("ES256"), ECDSA_SHA256);
  EXPECT_EQ(SignatureAlgorithmFromString("es256"), ECDSA_SHA256);

  EXPECT_EQ(SignatureAlgorithmFromString("RS256"), RSA_PKCS1_SHA256);
  EXPECT_EQ(SignatureAlgorithmFromString("rs256"), RSA_PKCS1_SHA256);

  EXPECT_EQ(SignatureAlgorithmFromString("ES256 blah"), std::nullopt);
  EXPECT_EQ(SignatureAlgorithmFromString("AB512"), std::nullopt);
  EXPECT_EQ(SignatureAlgorithmFromString(""), std::nullopt);
}

TEST(SessionBindingUtilsTest, ParseSignatureAlgorithmList) {
  using enum crypto::SignatureVerifier::SignatureAlgorithm;
  EXPECT_THAT(ParseSignatureAlgorithmList("ES256 RS256"),
              ElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  // Extra whitespace is ignored.
  EXPECT_THAT(ParseSignatureAlgorithmList("   ES256      RS256   "),
              ElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  // Order is preserved.
  EXPECT_THAT(ParseSignatureAlgorithmList("RS256 ES256"),
              ElementsAre(RSA_PKCS1_SHA256, ECDSA_SHA256));
  // Unknown algorithms are skipped.
  EXPECT_THAT(ParseSignatureAlgorithmList("WAT1 ES256 WAT2 RS256 WAT3"),
              ElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_THAT(ParseSignatureAlgorithmList(""), ElementsAre());
  // All unknown -- empty result.
  EXPECT_THAT(ParseSignatureAlgorithmList("WAT1 WAT2 WAT3"), ElementsAre());
}

TEST(SessionBindingUtilsTest,
     CreateKeyRegistrationHeaderAndPayloadForTokenBinding) {
  std::optional<std::string> result =
      CreateKeyRegistrationHeaderAndPayloadForTokenBinding(
          "test_client_id", "test_auth_code",
          GURL("https://accounts.google.com/RegisterKey"),
          crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
          std::vector<uint8_t>({1, 2, 3}),
          base::Time::UnixEpoch() + base::Days(200) + base::Milliseconds(123));
  ASSERT_TRUE(result.has_value());

  std::vector<std::string_view> header_and_payload = base::SplitStringPiece(
      *result, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(header_and_payload.size(), 2U);
  base::Value actual_header =
      Base64UrlEncodedJsonToValue(header_and_payload[0]);
  base::Value actual_payload =
      Base64UrlEncodedJsonToValue(header_and_payload[1]);

  base::Value::Dict expected_header =
      base::Value::Dict().Set("alg", "ES256").Set("typ", "jwt");
  base::Value::Dict expected_payload =
      base::Value::Dict()
          .Set("sub", "test_client_id")
          .Set("aud", "https://accounts.google.com/RegisterKey")
          // Base64UrlEncode(SHA256("test_auth_code"));
          .Set("jti", "TQurqawiFBU95_obuobFjt-aOhaU14_YdtMTCEjyTkM")
          .Set("iat", 17280000)
          .Set("key", base::Value::Dict()
                          .Set("kty",
                               "accounts.google.com/.well-known/kty/"
                               "SubjectPublicKeyInfo")
                          .Set("SubjectPublicKeyInfo", "AQID"));

  EXPECT_EQ(actual_header, expected_header);
  EXPECT_EQ(actual_payload, expected_payload);
}

TEST(SessionBindingUtilsTest,
     CreateKeyRegistrationHeaderAndPayloadForSessionBinding) {
  std::optional<std::string> result =
      CreateKeyRegistrationHeaderAndPayloadForSessionBinding(
          "test_challenge", GURL("https://accounts.google.com/RegisterKey"),
          crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
          std::vector<uint8_t>({1, 2, 3}),
          base::Time::UnixEpoch() + base::Days(200) + base::Milliseconds(123));
  ASSERT_TRUE(result.has_value());

  std::vector<std::string_view> header_and_payload = base::SplitStringPiece(
      *result, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(header_and_payload.size(), 2U);
  base::Value actual_header =
      Base64UrlEncodedJsonToValue(header_and_payload[0]);
  base::Value actual_payload =
      Base64UrlEncodedJsonToValue(header_and_payload[1]);

  base::Value::Dict expected_header =
      base::Value::Dict().Set("alg", "RS256").Set("typ", "jwt");
  base::Value::Dict expected_payload =
      base::Value::Dict()
          .Set("aud", "https://accounts.google.com/RegisterKey")
          .Set("jti", "test_challenge")
          .Set("iat", 17280000)
          .Set("key", base::Value::Dict()
                          .Set("kty",
                               "accounts.google.com/.well-known/kty/"
                               "SubjectPublicKeyInfo")
                          .Set("SubjectPublicKeyInfo", "AQID"));

  EXPECT_EQ(actual_header, expected_header);
  EXPECT_EQ(actual_payload, expected_payload);
}

class SessionBindingUtilsEphemeralKeyParamTest
    : public testing::TestWithParam<bool> {
 public:
  bool UseEphemeralKey() { return GetParam(); }
};

TEST_P(SessionBindingUtilsEphemeralKeyParamTest,
       CreateKeyAssertionHeaderAndPayload) {
  std::optional<HybridEncryptionKey> ephemeral_key;
  if (UseEphemeralKey()) {
    ephemeral_key = CreateHybridEncryptionKeyForTesting();
  }
  std::optional<std::string> result = CreateKeyAssertionHeaderAndPayload(
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      std::vector<uint8_t>({1, 2, 3}), "test_client_id", "test_challenge",
      GURL("https://accounts.google.com/VerifyKey"), "test_namespace",
      base::OptionalToPtr(ephemeral_key));
  ASSERT_TRUE(result.has_value());

  std::vector<std::string_view> header_and_payload = base::SplitStringPiece(
      *result, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(header_and_payload.size(), 2U);
  base::Value actual_header =
      Base64UrlEncodedJsonToValue(header_and_payload[0]);
  base::Value actual_payload =
      Base64UrlEncodedJsonToValue(header_and_payload[1]);

  base::Value::Dict expected_header =
      base::Value::Dict()
          .Set("alg", "ES256")
          .Set("typ", "jwt")
          .Set("schema", "DEVICE_BOUND_SESSION_CREDENTIALS_ASSERTION");
  base::Value::Dict expected_payload =
      base::Value::Dict()
          .Set("sub", "test_client_id")
          .Set("aud", "https://accounts.google.com/VerifyKey")
          .Set("jti", "test_challenge")
          // Base64UrlEncode(SHA256(public_key));
          .Set("iss", "A5BYxvLAy0ksUzsKTRTvd8wPeKvMztUofYShogEc-4E")
          .Set("namespace", "test_namespace");
  if (UseEphemeralKey()) {
    expected_payload.Set(
        "ephemeral_key",
        base::Value::Dict()
            .Set(
                "kty",
                "type.googleapis.com/google.crypto.tink.EciesAeadHkdfPublicKey")
            .Set("TinkKeysetPublicKeyInfo",
                 Base64UrlEncode(ephemeral_key->ExportPublicKey())));
  }

  EXPECT_EQ(actual_header, expected_header);
  EXPECT_EQ(actual_payload, expected_payload);
}

INSTANTIATE_TEST_SUITE_P(,
                         SessionBindingUtilsEphemeralKeyParamTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithEphemeralKey"
                                             : "WithoutEphemeralKey";
                         });

TEST(SessionBindingUtilsTest, AppendSignatureToHeaderAndPayload) {
  std::optional<std::string> result = AppendSignatureToHeaderAndPayload(
      "abc.efg",
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
      std::vector<uint8_t>({1, 2, 3}));
  EXPECT_EQ(result, "abc.efg.AQID");
}

TEST(SessionBindingUtilsTest,
     AppendSignatureToHeaderAndPayloadValidECDSASignature) {
  const std::vector<uint8_t> kDerSignature = {
      0x30, 0x45, 0x02, 0x20, 0x74, 0xa0, 0x6f, 0x6b, 0x2b, 0x0e, 0x82, 0x0e,
      0x03, 0x3b, 0x6e, 0x98, 0xfc, 0x89, 0x9c, 0xf3, 0x30, 0xb5, 0x56, 0xd3,
      0x29, 0x89, 0xb5, 0x82, 0x33, 0x5f, 0x9d, 0x97, 0xfb, 0x65, 0x64, 0x90,
      0x02, 0x21, 0x00, 0xbc, 0xb5, 0xee, 0x42, 0xe2, 0x5a, 0x87, 0xae, 0x21,
      0x18, 0xda, 0x7e, 0x68, 0x65, 0x30, 0xbe, 0xe5, 0x69, 0x3d, 0xc5, 0x5f,
      0xd5, 0x62, 0x45, 0x3e, 0x8d, 0x0b, 0x05, 0x1a, 0x33, 0x79, 0x8d};
  constexpr std::string_view kRawSignatureBase64UrlEncoded =
      "dKBvaysOgg4DO26Y_Imc8zC1VtMpibWCM1-dl_tlZJC8te5C4lqHriEY2n5oZTC-5Wk9xV_"
      "VYkU-jQsFGjN5jQ";

  std::optional<std::string> result = AppendSignatureToHeaderAndPayload(
      "abc.efg", crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      kDerSignature);
  EXPECT_EQ(result, base::StrCat({"abc.efg.", kRawSignatureBase64UrlEncoded}));
}

TEST(SessionBindingUtilsTest,
     AppendSignatureToHeaderAndPayloadInvalidECDSASignature) {
  std::optional<std::string> result = AppendSignatureToHeaderAndPayload(
      "abc.efg", crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      std::vector<uint8_t>({1, 2, 3}));
  EXPECT_EQ(result, std::nullopt);
}

}  // namespace signin
