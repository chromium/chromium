// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/public/transport_handler.h"

#include <string_view>

#include "base/types/expected.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

class MockTransportSession : public TransportSession {
 public:
  MockTransportSession() = default;
  ~MockTransportSession() override = default;

  MOCK_METHOD(std::string_view, GetSessionId, (), (const, override));
  MOCK_METHOD((base::expected<void, SendUpstreamMessageError>),
              SendUpstreamMessage,
              (PayloadType payload_type,
               const google::protobuf::MessageLite& message),
              (override));
  MOCK_METHOD(void,
              OnMessage,
              (PayloadType payload_type,
               const google::protobuf::MessageLite& message),
              (override));
};

class TestTransportHandler : public TransportHandler {
 public:
  explicit TestTransportHandler(TransportSession* session = nullptr)
      : TransportHandler(session) {}
  ~TestTransportHandler() override = default;

  void OnMessage(PayloadType payload_type,
                 std::string_view serialized_payload) override {}

  using TransportHandler::SendUpstreamMessage;
};

TEST(TransportHandlerTest, SendUpstreamMessageDelegatesToSession) {
  MockTransportSession session;
  TestTransportHandler handler(&session);
  ControlCommand command;
  command.mutable_close_channel();

  EXPECT_CALL(session,
              SendUpstreamMessage(PayloadType::kControl, testing::Ref(command)))
      .WillOnce(testing::Return(base::ok()));

  auto result = handler.SendUpstreamMessage(PayloadType::kControl, command);
  EXPECT_TRUE(result.has_value());
}

TEST(TransportHandlerTest, SendUpstreamMessageFailsWhenSessionNull) {
  TestTransportHandler handler(nullptr);
  ControlCommand command;
  command.mutable_close_channel();

  auto result = handler.SendUpstreamMessage(PayloadType::kControl, command);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SendUpstreamMessageError::kChannelDisconnected);
}

}  // namespace
}  // namespace browser_actuator
