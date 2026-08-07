// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/control_transport_handler.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

class TestTransportSession : public TransportSession {
 public:
  explicit TestTransportSession(std::string_view session_id)
      : session_id_(session_id) {}
  ~TestTransportSession() override = default;

  std::string_view GetSessionId() const override { return session_id_; }
  base::expected<void, SendMessageError> SendMessage(
      PayloadType payload_type,
      const google::protobuf::MessageLite& message) override {
    return {};
  }

 private:
  std::string session_id_;
};

TEST(ControlTransportHandlerTest, OnMessageCloseChannel) {
  bool close_channel_called = false;
  bool close_session_called = false;

  ControlTransportHandler handler(
      "session_1",
      base::BindLambdaForTesting([&]() { close_channel_called = true; }),
      base::BindLambdaForTesting(
          [&](std::string_view) { close_session_called = true; }));

  ControlCommand command;
  command.mutable_close_channel();

  handler.OnMessage(command.SerializeAsString());

  EXPECT_TRUE(close_channel_called);
  EXPECT_FALSE(close_session_called);
}

TEST(ControlTransportHandlerTest, OnMessageCloseSession) {
  bool close_channel_called = false;
  std::string closed_session_id;

  ControlTransportHandler handler(
      "session_1",
      base::BindLambdaForTesting([&]() { close_channel_called = true; }),
      base::BindLambdaForTesting([&](std::string_view session_id) {
        closed_session_id = session_id;
      }));

  ControlCommand command;
  command.mutable_close_session();

  handler.OnMessage(command.SerializeAsString());

  EXPECT_FALSE(close_channel_called);
  EXPECT_EQ(closed_session_id, "session_1");
}

TEST(ControlTransportHandlerTest, OnMessageInvalidPayload) {
  bool close_channel_called = false;
  bool close_session_called = false;

  ControlTransportHandler handler(
      "session_1",
      base::BindLambdaForTesting([&]() { close_channel_called = true; }),
      base::BindLambdaForTesting(
          [&](std::string_view) { close_session_called = true; }));

  handler.OnMessage("invalid_corrupted_protobuf_payload_bytes");

  EXPECT_FALSE(close_channel_called);
  EXPECT_FALSE(close_session_called);
}

TEST(ControlTransportHandlerTest, OnMessageUnsetCommand) {
  bool close_channel_called = false;
  bool close_session_called = false;

  ControlTransportHandler handler(
      "session_1",
      base::BindLambdaForTesting([&]() { close_channel_called = true; }),
      base::BindLambdaForTesting(
          [&](std::string_view) { close_session_called = true; }));

  ControlCommand command;  // empty, command_case() is COMMAND_NOT_SET

  handler.OnMessage(command.SerializeAsString());

  EXPECT_FALSE(close_channel_called);
  EXPECT_FALSE(close_session_called);
}

TEST(ControlTransportHandlerTest, FactoryGetFactoryId) {
  ControlTransportHandlerFactory factory(base::DoNothing(), base::DoNothing());

  EXPECT_EQ(factory.GetFactoryId(), FactoryId::kControl);
}

TEST(ControlTransportHandlerTest, FactoryGetSupportedPayloadTypes) {
  ControlTransportHandlerFactory factory(base::DoNothing(), base::DoNothing());

  EXPECT_THAT(factory.GetSupportedPayloadTypes(),
              ::testing::ElementsAre(PayloadType::kControl));
}

TEST(ControlTransportHandlerTest, FactoryUnsupportedPayloadType) {
  ControlTransportHandlerFactory factory(base::DoNothing(), base::DoNothing());

  EXPECT_THAT(factory.GetSupportedPayloadTypes(),
              ::testing::Not(::testing::Contains(PayloadType::kUnspecified)));
}

TEST(ControlTransportHandlerTest, FactoryOnNewSession) {
  std::string closed_session_id;

  ControlTransportHandlerFactory factory(
      base::DoNothing(),
      base::BindLambdaForTesting([&](std::string_view session_id) {
        closed_session_id = session_id;
      }));

  TestTransportSession test_session("session_abc");
  std::unique_ptr<TransportHandler> handler =
      factory.OnNewSession(&test_session);
  ASSERT_NE(handler, nullptr);

  ControlCommand command;
  command.mutable_close_session();

  handler->OnMessage(command.SerializeAsString());

  EXPECT_EQ(closed_session_id, "session_abc");
}

}  // namespace
}  // namespace browser_actuator
