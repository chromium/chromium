// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/receiver_response.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/test/mock_callback.h"
#include "components/mirroring/service/value_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using ::testing::_;

namespace mirroring {

class ReceiverResponseTest : public ::testing::Test {
 public:
  ReceiverResponseTest() {}

  ReceiverResponseTest(const ReceiverResponseTest&) = delete;
  ReceiverResponseTest& operator=(const ReceiverResponseTest&) = delete;

  ~ReceiverResponseTest() override {}
};

TEST_F(ReceiverResponseTest, ParseValidJson) {
  const std::string response_string = R"({"type": "ANSWER", "result": "ok"})";
  auto response = ReceiverResponse::Parse(response_string);
  ASSERT_TRUE(response);
  EXPECT_EQ(-1, response->session_id());
  EXPECT_EQ(-1, response->sequence_number());
  EXPECT_EQ(ResponseType::ANSWER, response->type());
  // Should be an error: there is no message body so this ANSWER is
  // completely useless.
  EXPECT_FALSE(response->valid());
}

TEST_F(ReceiverResponseTest, ParseInvalidValueType) {
  const std::string response_string =
      R"({
        "sessionId": 123, "seqNum": "one - two - three"
      })";
  EXPECT_EQ(nullptr, ReceiverResponse::Parse(response_string));
}

TEST_F(ReceiverResponseTest, ParseNonJsonString) {
  const std::string response_string = "This is not JSON.";
  EXPECT_EQ(nullptr, ReceiverResponse::Parse(response_string));
}

TEST_F(ReceiverResponseTest, ParseRealWorldAnswerMessage) {
  const std::string response_string =
      R"({"answer":{
          "receiverRtcpEventLog":[0, 1],
          "rtpExtensions": [
            ["adaptive_playout_delay"],
            ["adaptive_playout_delay"]
          ],
          "sendIndexes": [0, 1],
          "ssrcs": [40863, 759293],
          "udpPort": 50691,
          "castMode": "mirroring"
          },
          "result": "ok",
          "seqNum": 989031000,
          "type": "ANSWER"
        })";
  auto response = ReceiverResponse::Parse(response_string);
  ASSERT_TRUE(response);
  EXPECT_EQ(-1, response->session_id());
  EXPECT_EQ(989031000, response->sequence_number());
  EXPECT_EQ(ResponseType::ANSWER, response->type());
  ASSERT_TRUE(response->valid());
  EXPECT_EQ(50691, response->answer().udp_port);
  EXPECT_EQ(std::vector<int32_t>({0, 1}), response->answer().send_indexes);
  EXPECT_EQ(std::vector<uint32_t>({40863u, 759293u}), response->answer().ssrcs);
}

TEST_F(ReceiverResponseTest, ParseErrorMessage) {
  const std::string response_string =
      R"({"sessionId": 123,
          "seqNum": 999,
          "type": "ANSWER",
          "result": "error",
          "error": {
            "code": 42,
            "description": "it is broken",
            "details": {"foo": -1, "bar": 88}
          }
        })";
  auto response = ReceiverResponse::Parse(response_string);
  EXPECT_EQ(123, response->session_id());
  EXPECT_EQ(999, response->sequence_number());
  EXPECT_EQ(ResponseType::ANSWER, response->type());
  ASSERT_FALSE(response->valid());
  ASSERT_TRUE(response->error());
  EXPECT_EQ(42, response->error()->code);
  EXPECT_EQ("it is broken", response->error()->description);
  absl::optional<base::Value> parsed_details =
      base::JSONReader::Read(response->error()->details);
  ASSERT_TRUE(parsed_details && parsed_details->is_dict());
  EXPECT_EQ(2u, parsed_details->GetDict().size());
  int fool_value = 0;
  EXPECT_TRUE(GetInt(*parsed_details, "foo", &fool_value));
  EXPECT_EQ(-1, fool_value);
  int bar_value = 0;
  EXPECT_TRUE(GetInt(*parsed_details, "bar", &bar_value));
  EXPECT_EQ(88, bar_value);
}

TEST_F(ReceiverResponseTest, ParseCapabilityMessage) {
  const std::string response_string =
      R"({"sessionId": 999888777,
          "seqNum": 5551212,
          "type": "CAPABILITIES_RESPONSE",
          "result": "ok",
          "capabilities": {
            "remoting": 2,
            "mediaCaps": ["audio", "video", "vp9"],
            "keySystems": [
              {
                "keySystemName": "com.w3c.clearkey"
              },
              {
                "keySystemName": "com.widevine.alpha",
                "initDataTypes": ["a", "b", "c", "1", "2", "3"],
                "codecs": ["vp8", "h264"],
                "secureCodecs": ["h264", "vp8"],
                "audioRobustness": ["nope"],
                "videoRobustness": ["yep"],
                "persistentLicenseSessionSupport": "SUPPORTED",
                "persistentReleaseMessageSessionSupport": "SUPPORTED_WITH_ID",
                "persistentStateSupport": "REQUESTABLE",
                "distinctiveIdentifierSupport": "ALWAYS_ENABLED"
              }
            ]
        }})";
  auto response = ReceiverResponse::Parse(response_string);
  ASSERT_TRUE(response);
  EXPECT_EQ(999888777, response->session_id());
  EXPECT_EQ(5551212, response->sequence_number());
  EXPECT_EQ(ResponseType::CAPABILITIES_RESPONSE, response->type());
  // Key systems are now ignored, but should not be considered an error.
  ASSERT_TRUE(response->valid());
  EXPECT_EQ(std::vector<std::string>({"audio", "video", "vp9"}),
            response->capabilities().media_caps);
  EXPECT_EQ(2, response->capabilities().remoting);
}

TEST_F(ReceiverResponseTest, ParseRpcMessage) {
  const std::string message = "Hello from the Cast Receiver!";
  std::string rpc_base64;
  base::Base64Encode(message, &rpc_base64);
  const std::string response_string =
      R"({"sessionId": 735189,
          "seqNum": 6789,
          "type": "RPC",
          "result": "ok",
          "rpc": ")" +
      rpc_base64 + R"("})";

  auto response = ReceiverResponse::Parse(response_string);
  ASSERT_TRUE(response);
  EXPECT_EQ(735189, response->session_id());
  EXPECT_EQ(6789, response->sequence_number());
  ASSERT_TRUE(response->valid());
  EXPECT_EQ(ResponseType::RPC, response->type());
  EXPECT_EQ(message, response->rpc());
}

TEST_F(ReceiverResponseTest, ParseRpcMessageWithoutResultField) {
  const std::string message = "Hello from the Cast Receiver!";
  std::string rpc_base64;
  base::Base64Encode(message, &rpc_base64);
  const std::string response_string =
      R"({"sessionId": 3345678,
          "seqNum": 5566,
          "type": "RPC",
          "rpc": ")" +
      rpc_base64 + R"("})";

  auto response = ReceiverResponse::Parse(response_string);
  ASSERT_TRUE(response);
  EXPECT_EQ(3345678, response->session_id());
  EXPECT_EQ(5566, response->sequence_number());
  ASSERT_TRUE(response->valid());
  EXPECT_EQ(ResponseType::RPC, response->type());
  EXPECT_EQ(message, response->rpc());
}

TEST_F(ReceiverResponseTest, ParseResponseWithNullField) {
  const std::string response_string =
      R"({"sessionId": null,
          "seqNum": 808907000,
          "type": "ANSWER",
          "result": "ok",
          "rpc": null,
          "error": null,
          "answer": {
            "udpPort": 51706,
            "sendIndexes": [0,1],
            "ssrcs": [152818,556029],
            "IV": null,
            "castMode": "mirroring"
          },
          "status": null,
          "capabilities": null
        }
      )";
  auto response = ReceiverResponse::Parse(response_string);
  ASSERT_TRUE(response);
  EXPECT_EQ(808907000, response->sequence_number());
  ASSERT_TRUE(response->valid());
  EXPECT_EQ(ResponseType::ANSWER, response->type());
  EXPECT_EQ(51706, response->answer().udp_port);
  EXPECT_EQ(std::vector<int32_t>({0, 1}), response->answer().send_indexes);
  EXPECT_EQ(std::vector<uint32_t>({152818u, 556029u}),
            response->answer().ssrcs);
}

}  // namespace mirroring
