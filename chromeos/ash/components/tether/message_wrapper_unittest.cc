// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/message_wrapper.h"

#include <sstream>

#include "base/base64url.h"
#include "chromeos/ash/components/tether/proto/tether.pb.h"
#include "chromeos/ash/components/tether/proto_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

TetherAvailabilityResponse CreateTetherAvailabilityResponse() {
  TetherAvailabilityResponse response;
  response.set_response_code(
      TetherAvailabilityResponse_ResponseCode::
          TetherAvailabilityResponse_ResponseCode_TETHER_AVAILABLE);
  response.mutable_device_status()->CopyFrom(
      CreateDeviceStatusWithFakeFields());
  return response;
}

void VerifyProtoConversion(const google::protobuf::MessageLite* proto,
                           const MessageWrapper& wrapper,
                           const MessageType& expected_message_type) {
  std::string raw_message = wrapper.ToRawMessage();
  EXPECT_TRUE(raw_message.length() > 0);

  std::unique_ptr<MessageWrapper> wrapper_from_raw_message =
      MessageWrapper::FromRawMessage(raw_message);
  EXPECT_TRUE(wrapper_from_raw_message);
  EXPECT_EQ(expected_message_type, wrapper_from_raw_message->GetMessageType());
  EXPECT_EQ(proto->SerializeAsString(),
            wrapper_from_raw_message->GetProto()->SerializeAsString());
}

}  // namespace

class MessageWrapperTest : public testing::Test {
 public:
  MessageWrapperTest(const MessageWrapperTest&) = delete;
  MessageWrapperTest& operator=(const MessageWrapperTest&) = delete;

 protected:
  MessageWrapperTest() = default;
};

TEST_F(MessageWrapperTest, TestToAndFromRawMessage_ConnectTetheringRequest) {
  ConnectTetheringRequest request;

  MessageWrapper wrapper(request);
  VerifyProtoConversion(&request, wrapper,
                        MessageType::CONNECT_TETHERING_REQUEST);
}

TEST_F(MessageWrapperTest, TestToAndFromRawMessage_ConnectTetheringResponse) {
  ConnectTetheringResponse response;
  response.set_ssid("Instant Tethering 123456");
  response.set_password("password");
  response.set_response_code(ConnectTetheringResponse_ResponseCode::
                                 ConnectTetheringResponse_ResponseCode_SUCCESS);
  response.mutable_device_status()->CopyFrom(
      CreateDeviceStatusWithFakeFields());

  MessageWrapper wrapper(response);
  VerifyProtoConversion(&response, wrapper,
                        MessageType::CONNECT_TETHERING_RESPONSE);
}

TEST_F(MessageWrapperTest, TestToAndFromRawMessage_DisconnectTetheringRequest) {
  DisconnectTetheringRequest request;

  MessageWrapper wrapper(request);
  VerifyProtoConversion(&request, wrapper,
                        MessageType::DISCONNECT_TETHERING_REQUEST);
}

TEST_F(MessageWrapperTest, TestToAndFromRawMessage_KeepAliveTickle) {
  KeepAliveTickle tickle;

  MessageWrapper wrapper(tickle);
  VerifyProtoConversion(&tickle, wrapper, MessageType::KEEP_ALIVE_TICKLE);
}

TEST_F(MessageWrapperTest, TestToAndFromRawMessage_KeepAliveTickleResponse) {
  KeepAliveTickleResponse response;

  MessageWrapper wrapper(response);
  VerifyProtoConversion(&response, wrapper,
                        MessageType::KEEP_ALIVE_TICKLE_RESPONSE);
}

TEST_F(MessageWrapperTest, TestToAndFromRawMessage_TetherAvailabilityRequest) {
  TetherAvailabilityRequest request;

  MessageWrapper wrapper(request);
  VerifyProtoConversion(&request, wrapper,
                        MessageType::TETHER_AVAILABILITY_REQUEST);
}

TEST_F(MessageWrapperTest, TestToAndFromRawMessage_TetherAvailabilityResponse) {
  TetherAvailabilityResponse response = CreateTetherAvailabilityResponse();

  MessageWrapper wrapper(response);
  VerifyProtoConversion(&response, wrapper,
                        MessageType::TETHER_AVAILABILITY_RESPONSE);
}

TEST_F(MessageWrapperTest, TestHandleInvalidJson) {
  EXPECT_FALSE(MessageWrapper::FromRawMessage("not JSON"));
}

TEST_F(MessageWrapperTest, TestHandleJsonWithoutType) {
  ConnectTetheringRequest request;
  std::string encoded_message;
  base::Base64UrlEncode(request.SerializeAsString(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_message);

  std::stringstream ss;
  ss << "{\"data\": \"" << encoded_message << "\"}";

  EXPECT_FALSE(MessageWrapper::FromRawMessage(ss.str()));
}

TEST_F(MessageWrapperTest, TestHandleJsonWithoutData) {
  EXPECT_FALSE(MessageWrapper::FromRawMessage("{\"type\":1}"));
}

TEST_F(MessageWrapperTest, TestHandleJsonWithoutTypeOrData) {
  EXPECT_FALSE(MessageWrapper::FromRawMessage("{}"));
}

TEST_F(MessageWrapperTest, TestHandleDataNotEncodedWithBase64) {
  TetherAvailabilityResponse response = CreateTetherAvailabilityResponse();

  std::stringstream ss;
  ss << "{\"type\":" << static_cast<int>(MessageType::CONNECT_TETHERING_REQUEST)
     << ",\"data\":\""
     << response.SerializeAsString()  // Do not convert to base-64.
     << "\"}";

  EXPECT_FALSE(MessageWrapper::FromRawMessage(ss.str()));
}

TEST_F(MessageWrapperTest, TestFromRawMessage_StringLiteral) {
  // Type 2 is TETHER_AVAILABILITY_RESPONSE, and the data supplied is
  // CreateTetherAvailabilityResponse() encoded in base-64.
  std::string raw_message =
      "{\"type\":2,\"data\":\"CAESHQhLEglHb29nbGUgRmkYBCIMCAESCFdpZmlTc2lk\"}";

  std::unique_ptr<MessageWrapper> wrapper =
      MessageWrapper::FromRawMessage(raw_message);
  EXPECT_TRUE(wrapper);
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            wrapper->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            wrapper->GetProto()->SerializeAsString());
}

TEST_F(MessageWrapperTest, TestFromRawMessage_ExtraJsonKeyValuePair) {
  TetherAvailabilityResponse response = CreateTetherAvailabilityResponse();
  std::string response_string_base64;
  base::Base64UrlEncode(response.SerializeAsString(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &response_string_base64);

  std::stringstream ss;
  ss << "{\"type\":"
     << static_cast<int>(MessageType::TETHER_AVAILABILITY_RESPONSE)
     << ",\"data\":\"" << response_string_base64 << "\""
     << ",\"extraKey\":\"extraValue\"}";

  std::unique_ptr<MessageWrapper> wrapper =
      MessageWrapper::FromRawMessage(ss.str());
  EXPECT_TRUE(wrapper);
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            wrapper->GetMessageType());
  EXPECT_EQ(response.SerializeAsString(),
            wrapper->GetProto()->SerializeAsString());
}

}  // namespace tether

}  // namespace ash
