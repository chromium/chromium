// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/internal/control_transport_handler.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/stream_transport/internal/metrics_utils.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/payload_type_mapping.h"
#include "components/stream_transport/public/transport_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace stream_transport {
namespace {

class TestTransportSession : public TransportSession {
 public:
  explicit TestTransportSession(std::string_view session_id)
      : session_id_(session_id) {}
  ~TestTransportSession() override = default;

  std::string_view GetSessionId() const override { return session_id_; }
  base::expected<void, SendUpstreamMessageError> SendUpstreamMessage(
      PayloadType payload_type,
      const google::protobuf::MessageLite& message) override {
    return {};
  }
  void OnMessage(PayloadType payload_type,
                 std::string_view serialized_payload) override {}
  void Close() override {}

 private:
  std::string session_id_;
};

class MockTransportSession : public TransportSession {
 public:
  explicit MockTransportSession(std::string_view session_id)
      : session_id_(session_id) {}
  ~MockTransportSession() override = default;

  std::string_view GetSessionId() const override { return session_id_; }
  MOCK_METHOD((base::expected<void, SendUpstreamMessageError>),
              SendUpstreamMessage,
              (PayloadType payload_type,
               const google::protobuf::MessageLite& message),
              (override));
  MOCK_METHOD(void,
              OnMessage,
              (PayloadType payload_type, std::string_view serialized_payload),
              (override));
  MOCK_METHOD(void, Close, (), (override));

 private:
  std::string session_id_;
};

TEST(ControlTransportHandlerTest, OnMessageStartSessionSendsAck) {
  base::HistogramTester histogram_tester;
  MockTransportSession session("session_1");
  ControlTransportHandler handler(&session, base::DoNothing(),
                                  base::DoNothing());

  EXPECT_CALL(session, SendUpstreamMessage(PayloadType::kControl, testing::_))
      .WillOnce([&](PayloadType type, const google::protobuf::MessageLite& msg)
                    -> base::expected<void, SendUpstreamMessageError> {
        EXPECT_EQ(
            msg.GetTypeName(),
            browser_actuator::ControlCommand::default_instance().GetTypeName());
        const auto* cmd =
            static_cast<const browser_actuator::ControlCommand*>(&msg);
        EXPECT_TRUE(cmd->has_start_session_ack());
        return {};
      });

  browser_actuator::ControlCommand command;
  command.mutable_start_session();

  handler.OnMessage(PayloadType::kControl, command.SerializeAsString());

  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Downstream.ControlCommand",
      DownstreamControlCommand::kStartSession, 1);
}

TEST(ControlTransportHandlerTest, OnMessageCloseChannel) {
  base::HistogramTester histogram_tester;
  bool close_channel_called = false;
  bool close_session_called = false;

  TestTransportSession session("session_1");
  ControlTransportHandler handler(
      &session,
      base::BindLambdaForTesting([&]() { close_channel_called = true; }),
      base::BindLambdaForTesting(
          [&](std::string_view) { close_session_called = true; }));

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  handler.OnMessage(PayloadType::kControl, command.SerializeAsString());

  EXPECT_TRUE(close_channel_called);
  EXPECT_FALSE(close_session_called);
  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Downstream.ControlCommand",
      DownstreamControlCommand::kCloseChannel, 1);
}

TEST(ControlTransportHandlerTest, OnMessageCloseSession) {
  base::HistogramTester histogram_tester;
  bool close_channel_called = false;
  std::string closed_session_id;

  TestTransportSession session("session_1");
  ControlTransportHandler handler(
      &session,
      base::BindLambdaForTesting([&]() { close_channel_called = true; }),
      base::BindLambdaForTesting([&](std::string_view session_id) {
        closed_session_id = session_id;
      }));

  browser_actuator::ControlCommand command;
  command.mutable_close_session();

  handler.OnMessage(PayloadType::kControl, command.SerializeAsString());

  EXPECT_FALSE(close_channel_called);
  EXPECT_EQ(closed_session_id, "session_1");
  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Downstream.ControlCommand",
      DownstreamControlCommand::kCloseSession, 1);
}

TEST(ControlTransportHandlerTest, OnMessageCloseSessionWithNullSessionNoops) {
  base::HistogramTester histogram_tester;
  bool close_channel_called = false;
  bool close_session_called = false;

  ControlTransportHandler handler(
      nullptr,
      base::BindLambdaForTesting([&]() { close_channel_called = true; }),
      base::BindLambdaForTesting(
          [&](std::string_view) { close_session_called = true; }));

  browser_actuator::ControlCommand command;
  command.mutable_close_session();

  handler.OnMessage(PayloadType::kControl, command.SerializeAsString());

  EXPECT_FALSE(close_channel_called);
  EXPECT_FALSE(close_session_called);
  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Downstream.ControlCommand",
      DownstreamControlCommand::kCloseSession, 1);
}

TEST(ControlTransportHandlerTest, OnMessageUnsetCommand) {
  base::HistogramTester histogram_tester;
  bool close_channel_called = false;
  bool close_session_called = false;

  TestTransportSession session("session_1");
  ControlTransportHandler handler(
      &session,
      base::BindLambdaForTesting([&]() { close_channel_called = true; }),
      base::BindLambdaForTesting(
          [&](std::string_view) { close_session_called = true; }));

  browser_actuator::ControlCommand
      command;  // empty, command_case() is COMMAND_NOT_SET

  handler.OnMessage(PayloadType::kControl, command.SerializeAsString());

  EXPECT_FALSE(close_channel_called);
  EXPECT_FALSE(close_session_called);
  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Downstream.ControlCommand",
      DownstreamControlCommand::kUnspecified, 1);
}

// The handler receives raw bytes, so a payload that is not a ControlCommand
// surfaces as a parse failure rather than as a type mismatch. It must be
// dropped without invoking any callback.
TEST(ControlTransportHandlerTest, MalformedPayloadIsIgnored) {
  base::HistogramTester histogram_tester;
  bool close_channel_called = false;
  bool close_session_called = false;

  TestTransportSession session("session_1");
  ControlTransportHandler handler(
      &session,
      base::BindLambdaForTesting([&]() { close_channel_called = true; }),
      base::BindLambdaForTesting(
          [&](std::string_view) { close_session_called = true; }));

  // Field number 13 with wire type 6, which is not a valid wire type.
  handler.OnMessage(PayloadType::kControl, "not a valid proto");

  EXPECT_FALSE(close_channel_called);
  EXPECT_FALSE(close_session_called);
  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Downstream.ControlCommand",
      DownstreamControlCommand::kUnspecified, 1);
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

  browser_actuator::ControlCommand command;
  command.mutable_close_session();

  handler->OnMessage(PayloadType::kControl, command.SerializeAsString());

  EXPECT_EQ(closed_session_id, "session_abc");
}

// Pins `ExpectedTypeUrl` to the generated proto; this layer may include it.
TEST(ControlTransportHandlerTest, ExpectedTypeUrlMatchesControlCommand) {
  EXPECT_EQ(ExpectedTypeUrl(PayloadType::kControl),
            base::StrCat({kTypeUrlPrefix,
                          browser_actuator::ControlCommand().GetTypeName()}));
}

TEST(ControlTransportHandlerTest, OnMessageStartSessionAckIsSilentNoop) {
  base::HistogramTester histogram_tester;
  MockTransportSession session("session_1");
  bool close_channel_called = false;
  bool close_session_called = false;

  ControlTransportHandler handler(
      &session,
      base::BindLambdaForTesting([&]() { close_channel_called = true; }),
      base::BindLambdaForTesting(
          [&](std::string_view) { close_session_called = true; }));

  EXPECT_CALL(session, SendUpstreamMessage(testing::_, testing::_)).Times(0);

  browser_actuator::ControlCommand command;
  command.mutable_start_session_ack();

  handler.OnMessage(PayloadType::kControl, command.SerializeAsString());

  EXPECT_FALSE(close_channel_called);
  EXPECT_FALSE(close_session_called);
  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Downstream.ControlCommand",
      DownstreamControlCommand::kStartSessionAck, 1);
}

}  // namespace
}  // namespace stream_transport
