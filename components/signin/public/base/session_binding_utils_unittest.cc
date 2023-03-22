// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/session_binding_utils.h"

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/value_iterators.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace signin {

namespace {

base::Value Base64UrlEncodedJsonToValue(base::StringPiece input) {
  std::string json;
  EXPECT_TRUE(base::Base64UrlDecode(
      input, base::Base64UrlDecodePolicy::DISALLOW_PADDING, &json));
  absl::optional<base::Value> result = base::JSONReader::Read(json);
  EXPECT_TRUE(result.has_value());
  return std::move(*result);
}

}  // namespace

TEST(SessionBindingUtilsTest, CreateKeyRegistrationHeaderAndPayload) {
  absl::optional<std::string> result = CreateKeyRegistrationHeaderAndPayload(
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      std::vector<uint8_t>({1, 2, 3}), "test_client_id", "test_auth_code",
      GURL("https://accounts.google.com/RegisterKey"),
      base::Time::UnixEpoch() + base::Days(200) + base::Milliseconds(123));
  ASSERT_TRUE(result.has_value());

  std::vector<base::StringPiece> header_and_payload = base::SplitStringPiece(
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

TEST(SessionBindingUtilsTest, AppendSignatureToHeaderAndPayload) {
  std::string result = AppendSignatureToHeaderAndPayload(
      "abc.efg", std::vector<uint8_t>({1, 2, 3}));
  EXPECT_EQ(result, "abc.efg.AQID");
}

}  // namespace signin
