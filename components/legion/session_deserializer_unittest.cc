// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/session_deserializer.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"
#include "third_party/oak/chromium/proto/session/session.test.h"
#include "third_party/oak/chromium/proto/session/session.to_value.h"

namespace legion {

namespace {

using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(SessionDeserializerTest, DeserializeHandshakeResponseSuccess) {
  const char kValidJson[] = R"(
    {
      "handshake_response": {
        "noise_handshake_message": {
          "ephemeral_public_key": "ZV9rZXk=",
          "static_public_key": "c19rZXk=",
          "ciphertext": "c2VjcmV0"
        },
        "attestation_bindings": {
          "key1": {
            "binding": "YmluZGluZzE="
          }
        },
        "assertion_bindings": {
          "key2": {
            "binding": "YmluZGluZzI="
          }
        }
      }
    }
  )";

  std::optional<base::Value> value =
      base::JSONReader::Read(kValidJson, base::JSON_PARSE_RFC);
  ASSERT_TRUE(value.has_value());

  oak::session::v1::SessionResponse session_response;
  EXPECT_TRUE(DeserializeSessionResponse(*value, &session_response));
  EXPECT_TRUE(session_response.has_handshake_response());

  auto response = session_response.handshake_response();
  EXPECT_EQ(response.noise_handshake_message().ephemeral_public_key(), "e_key");
  EXPECT_EQ(response.noise_handshake_message().static_public_key(), "s_key");
  EXPECT_EQ(response.noise_handshake_message().ciphertext(), "secret");

  ASSERT_THAT(response.attestation_bindings(), SizeIs(1));
  EXPECT_EQ(response.attestation_bindings().at("key1").binding(), "binding1");

  ASSERT_THAT(response.assertion_bindings(), SizeIs(1));
  EXPECT_EQ(response.assertion_bindings().at("key2").binding(), "binding2");
}

TEST(SessionDeserializerTest, DeserializeHandshakeResponseEmptyMaps) {
  const char kJson[] = R"({
    "handshake_response": {
      "noise_handshake_message": {
        "ephemeral_public_key": "ZV9rZXk="
      }
    }
  })";

  std::optional<base::Value> value =
      base::JSONReader::Read(kJson, base::JSON_PARSE_RFC);
  ASSERT_TRUE(value.has_value());

  oak::session::v1::SessionResponse session_response;
  EXPECT_TRUE(DeserializeSessionResponse(*value, &session_response));
  EXPECT_TRUE(session_response.has_handshake_response());

  auto response = session_response.handshake_response();

  EXPECT_EQ(response.noise_handshake_message().ephemeral_public_key(), "e_key");
  EXPECT_THAT(response.attestation_bindings(), IsEmpty());
  EXPECT_THAT(response.assertion_bindings(), IsEmpty());
}

TEST(SessionDeserializerTest, DeserializeNotADictionary) {
  base::Value value(base::Value::Type::LIST);
  oak::session::v1::SessionResponse response;
  EXPECT_FALSE(DeserializeSessionResponse(value, &response));
}

TEST(SessionDeserializerTest, DeserializeInvalidNestedType) {
  const char kJson[] = R"({
        "handshake_response": "not_a_dict"
  })";

  std::optional<base::Value> value =
      base::JSONReader::Read(kJson, base::JSON_PARSE_RFC);
  ASSERT_TRUE(value.has_value());

  oak::session::v1::SessionResponse response;
  EXPECT_FALSE(DeserializeSessionResponse(*value, &response));
}

TEST(SessionDeserializerTest, HandshakeResponseToJsonAndBack) {
  oak::session::v1::SessionResponse session_proto;
  auto* handshake_proto = session_proto.mutable_handshake_response();
  handshake_proto->mutable_noise_handshake_message()->set_ephemeral_public_key(
      "test_ephemeral_key");
  handshake_proto->mutable_noise_handshake_message()->set_static_public_key(
      "test_static_key");
  handshake_proto->mutable_noise_handshake_message()->set_ciphertext(
      "test_ciphertext");

  base::Value value = oak::session::v1::Serialize(session_proto);

  oak::session::v1::SessionResponse deserialized_proto;
  ASSERT_TRUE(DeserializeSessionResponse(value, &deserialized_proto));

  EXPECT_THAT(session_proto,
              oak::session::v1::EqualsSessionResponse(deserialized_proto));
}

}  // namespace

}  // namespace legion
