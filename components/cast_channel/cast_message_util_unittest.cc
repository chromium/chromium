// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_message_util.h"

#include "base/strings/strcat.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

using base::test::IsJson;
using base::test::ParseJson;

namespace cast_channel {

TEST(CastMessageUtilTest, IsCastReservedNamespace) {
  EXPECT_TRUE(
      IsCastReservedNamespace("urn:x-cast:com.google.cast.receiver.xyzzy"));
  EXPECT_TRUE(IsCastReservedNamespace("urn:x-cast:com.google.cast.receiver"));
  EXPECT_FALSE(IsCastReservedNamespace("urn:x-cast:com.google.cast"));
  EXPECT_FALSE(IsCastReservedNamespace("urn:x-cast:com.google.cast."));
  EXPECT_FALSE(
      IsCastReservedNamespace("urn:x-cast:com.google.cast.foo.receiver"));
  EXPECT_FALSE(
      IsCastReservedNamespace("urn:x-cast:com.google.cast.receiverfoo"));
  EXPECT_FALSE(IsCastReservedNamespace("urn:x-cast:com.google.cast.xyzzy"));
  EXPECT_FALSE(IsCastReservedNamespace("urn:x-cast:com.google.youtube"));
  EXPECT_FALSE(IsCastReservedNamespace("urn:x-cast:com.foo"));
  EXPECT_FALSE(IsCastReservedNamespace("foo"));
  EXPECT_FALSE(IsCastReservedNamespace(""));
}

TEST(CastMessageUtilTest, CastMessageType) {
  for (int i = 0; i < static_cast<int>(CastMessageType::kOther); ++i) {
    CastMessageType type = static_cast<CastMessageType>(i);
    EXPECT_EQ(type, CastMessageTypeFromString(ToString(type)));
  }
}

TEST(CastMessageUtilTest, GetLaunchSessionResponseOk) {
  std::string status = R"(
    {
      "applications": [
        {
          "appId": "2FE23A98",
          "universalAppId": "AD9AF8E0",
          "appType": "ANDROID_TV"
        }
      ]
    }
  )";
  std::string payload = base::StrCat({R"(
    {
      "type": "RECEIVER_STATUS",
      "requestId": 123,
      "status": )",
                                      status, "}"});

  LaunchSessionResponse response = GetLaunchSessionResponse(ParseJson(payload));
  EXPECT_EQ(LaunchSessionResponse::Result::kOk, response.result);
  EXPECT_EQ(ParseJson(status), response.receiver_status);
}

TEST(CastMessageUtilTest, GetLaunchSessionResponseError) {
  std::string payload = R"(
    {
      "type": "LAUNCH_ERROR",
      "requestId": 123
    }
  )";

  LaunchSessionResponse response = GetLaunchSessionResponse(ParseJson(payload));
  EXPECT_EQ(LaunchSessionResponse::Result::kError, response.result);
  EXPECT_FALSE(response.receiver_status);
}

TEST(CastMessageUtilTest, GetLaunchSessionResponseUnknown) {
  // Unrelated type.
  std::string payload = R"(
    {
      "type": "APPLICATION_BROADCAST",
      "requestId": 123,
      "status": {}
    }
  )";

  LaunchSessionResponse response = GetLaunchSessionResponse(ParseJson(payload));
  EXPECT_EQ(LaunchSessionResponse::Result::kUnknown, response.result);
  EXPECT_FALSE(response.receiver_status);
}

TEST(CastMessageUtilTest, CreateStopRequest) {
  std::string expected_message = R"(
    {
      "type": "STOP",
      "requestId": 123,
      "sessionId": "sessionId"
    }
  )";

  CastMessage message = CreateStopRequest("sourceId", 123, "sessionId");
  ASSERT_TRUE(IsCastMessageValid(message));
  EXPECT_THAT(message.payload_utf8(), IsJson(expected_message));
}

TEST(CastMessageUtilTest, CreateCastMessageWithObject) {
  constexpr char payload[] = R"({"foo": "bar"})";
  const auto message = CreateCastMessage("theNamespace", ParseJson(payload),
                                         "theSourceId", "theDestinationId");
  ASSERT_TRUE(IsCastMessageValid(message));
  EXPECT_EQ("theNamespace", message.namespace_());
  EXPECT_EQ("theSourceId", message.source_id());
  EXPECT_EQ("theDestinationId", message.destination_id());
  EXPECT_THAT(message.payload_utf8(), IsJson(payload));
}

TEST(CastMessageUtilTest, CreateCastMessageWithString) {
  constexpr char payload[] = "foo";
  const auto message = CreateCastMessage("theNamespace", base::Value(payload),
                                         "theSourceId", "theDestinationId");
  ASSERT_TRUE(IsCastMessageValid(message));
  EXPECT_EQ("theNamespace", message.namespace_());
  EXPECT_EQ("theSourceId", message.source_id());
  EXPECT_EQ("theDestinationId", message.destination_id());
  EXPECT_EQ(message.payload_utf8(), payload);
}

TEST(CastMessageUtilTest, CreateVirtualConnectionClose) {
  std::string expected_message = R"(
    {
       "type": "CLOSE",
       "reasonCode": 5
    }
  )";

  CastMessage message =
      CreateVirtualConnectionClose("sourceId", "destinationId");
  ASSERT_TRUE(IsCastMessageValid(message));
  EXPECT_EQ(message.source_id(), "sourceId");
  EXPECT_EQ(message.destination_id(), "destinationId");
  EXPECT_EQ(message.namespace_(), kConnectionNamespace);
  EXPECT_THAT(message.payload_utf8(), IsJson(expected_message));
}

TEST(CastMessageUtilTest, CreateReceiverStatusRequest) {
  std::string expected_message = R"(
    {
       "type": "GET_STATUS",
       "requestId": 123
    }
  )";

  CastMessage message = CreateReceiverStatusRequest("sourceId", 123);
  ASSERT_TRUE(IsCastMessageValid(message));
  EXPECT_THAT(message.payload_utf8(), IsJson(expected_message));
}

TEST(CastMessageUtilTest, CreateMediaRequest) {
  std::string body = R"({
       "type": "STOP_MEDIA",
    })";
  std::string expected_message = R"({
       "type": "STOP",
       "requestId": 123,
    })";

  CastMessage message = CreateMediaRequest(ParseJson(body), 123, "theSourceId",
                                           "theDestinationId");
  ASSERT_TRUE(IsCastMessageValid(message));
  EXPECT_EQ(kMediaNamespace, message.namespace_());
  EXPECT_EQ("theSourceId", message.source_id());
  EXPECT_EQ("theDestinationId", message.destination_id());
  EXPECT_THAT(message.payload_utf8(), IsJson(expected_message));
}

TEST(CastMessageUtilTest, CreateVolumeRequest) {
  std::string body = R"({
       "type": "SET_VOLUME",
       "sessionId": "theSessionId",
    })";
  std::string expected_message = R"({
       "type": "SET_VOLUME",
       "requestId": 123,
    })";

  CastMessage message =
      CreateSetVolumeRequest(ParseJson(body), 123, "theSourceId");
  ASSERT_TRUE(IsCastMessageValid(message));
  EXPECT_EQ(kReceiverNamespace, message.namespace_());
  EXPECT_EQ("theSourceId", message.source_id());
  EXPECT_EQ(kPlatformReceiverId, message.destination_id());
  EXPECT_THAT(message.payload_utf8(), IsJson(expected_message));
}

TEST(CastMessageUtilTest, GetConnectionType) {
  EXPECT_EQ(VirtualConnectionType::kStrong, GetConnectionType("receiver-0"));
  EXPECT_EQ(VirtualConnectionType::kInvisible, GetConnectionType("sender-123"));
}

}  // namespace cast_channel
