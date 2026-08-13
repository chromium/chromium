// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_session_impl.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport_handler_factory_registry_impl.h"
#include "components/browser_actuator/test_support/mock_transport_channel.h"
#include "components/browser_actuator/test_support/mock_transport_handler.h"
#include "components/browser_actuator/test_support/mock_transport_handler_factory.h"
#include "components/browser_actuator/test_support/test_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

TEST(TransportSessionImplTest, GetSessionId) {
  MockTransportChannel channel;
  TransportSessionImpl session("test_session", channel.GetWeakPtr());
  EXPECT_EQ(session.GetSessionId(), "test_session");
}

TEST(TransportSessionImplTest, SendMessage) {
  MockTransportChannel channel;
  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  ControlCommand command;
  command.mutable_close_channel();

  EXPECT_CALL(channel,
              SendUpstreamMessage("test_session", PayloadType::kUnspecified,
                                  testing::Ref(command)));
  EXPECT_TRUE(
      session.SendMessage(PayloadType::kUnspecified, command).has_value());
}

TEST(TransportSessionImplTest, SendMessageAfterChannelDestruction) {
  auto channel = std::make_unique<MockTransportChannel>();
  TransportSessionImpl session("test_session", channel->GetWeakPtr());

  ControlCommand command;
  command.mutable_close_channel();

  EXPECT_CALL(*channel, SendUpstreamMessage).Times(0);
  channel.reset();
  base::expected<void, SendMessageError> result =
      session.SendMessage(PayloadType::kUnspecified, command);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SendMessageError::kChannelDisconnected);
}

TEST(TransportSessionImplTest, ClientSequenceNumbers) {
  MockTransportChannel channel;
  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  EXPECT_EQ(session.client_sequence_number(), 0);

  EXPECT_EQ(session.IncrementClientSequenceNumber(), 1);
  EXPECT_EQ(session.client_sequence_number(), 1);
}

TEST(TransportSessionImplTest, ServerSequenceNumbers) {
  MockTransportChannel channel;
  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  EXPECT_EQ(session.last_seen_sequence_number(), 0);

  EXPECT_TRUE(session.RecordServerSequenceNumber(42));
  EXPECT_EQ(session.last_seen_sequence_number(), 42);

  // Past sequence numbers should be ignored.
  EXPECT_FALSE(session.RecordServerSequenceNumber(30));
  EXPECT_EQ(session.last_seen_sequence_number(), 42);

  // Non-positive sequence numbers should be ignored.
  EXPECT_FALSE(session.RecordServerSequenceNumber(0));
  EXPECT_FALSE(session.RecordServerSequenceNumber(-5));
  EXPECT_EQ(session.last_seen_sequence_number(), 42);
}

TEST(TransportSessionImplTest, GetWeakPtr) {
  MockTransportChannel channel;
  auto session = std::make_unique<TransportSessionImpl>("test_session",
                                                        channel.GetWeakPtr());
  base::WeakPtr<TransportSessionImpl> weak_session = session->GetWeakPtr();

  EXPECT_NE(weak_session.get(), nullptr);
  session.reset();
  EXPECT_EQ(weak_session.get(), nullptr);
}

TEST(TransportSessionImplTest, LazyInstantiationAndRouting) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory({PayloadType::kUnspecified});
  registry.RegisterFactory(&factory);

  auto handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* handler_ptr = handler.get();

  int message_count = 0;
  EXPECT_CALL(factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(handler)));
  EXPECT_CALL(*handler_ptr, OnMessage)
      .WillRepeatedly([&message_count](const google::protobuf::MessageLite&) {
        message_count++;
      });

  ControlCommand command;

  // First dispatch triggers factory.OnNewSession
  EXPECT_TRUE(
      session.ProcessPayload(PayloadType::kUnspecified, command).has_value());

  // Second dispatch should reuse the handler (OnNewSession not called again)
  EXPECT_TRUE(
      session.ProcessPayload(PayloadType::kUnspecified, command).has_value());

  EXPECT_EQ(message_count, 2);
}

TEST(TransportSessionImplTest, HandlerDestroysSessionDuringDispatch) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  auto session = std::make_unique<TransportSessionImpl>("test_session",
                                                        channel.GetWeakPtr());
  TransportSessionImpl* session_ptr = session.get();

  MockTransportHandlerFactory factory1({PayloadType::kUnspecified});
  MockTransportHandlerFactory factory2({PayloadType::kUnspecified},
                                       kTestFactoryId2);
  registry.RegisterFactory(&factory1);
  registry.RegisterFactory(&factory2);

  auto handler1 = std::make_unique<CallbackTransportHandler>(base::BindOnce(
      [](std::unique_ptr<TransportSessionImpl>* s) { s->reset(); }, &session));

  auto handler2 = std::make_unique<MockTransportHandler>();
  MockTransportHandler* handler_ptr2 = handler2.get();

  EXPECT_CALL(factory1, OnNewSession(session_ptr))
      .WillOnce(testing::Return(std::move(handler1)));
  EXPECT_CALL(factory2, OnNewSession(session_ptr))
      .WillOnce(testing::Return(std::move(handler2)));

  // The second handler must not be called because the loop should break after
  // the session is destroyed.
  EXPECT_CALL(*handler_ptr2, OnMessage).Times(0);

  ControlCommand command;

  // This should not crash and should return success.
  EXPECT_TRUE(session_ptr->ProcessPayload(PayloadType::kUnspecified, command)
                  .has_value());
}

TEST(TransportSessionImplTest, ProcessPayloadAfterChannelDestruction) {
  auto channel = std::make_unique<MockTransportChannel>();
  TransportSessionImpl session("test_session", channel->GetWeakPtr());

  channel.reset();
  ControlCommand command;
  auto result = session.ProcessPayload(PayloadType::kUnspecified, command);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            TransportSessionImpl::ProcessPayloadError::kChannelDisconnected);
}

TEST(TransportSessionImplTest, FactoryDestroysSessionDuringOnNewSession) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  auto session = std::make_unique<TransportSessionImpl>("test_session",
                                                        channel.GetWeakPtr());
  TransportSessionImpl* session_ptr = session.get();

  MockTransportHandlerFactory factory({PayloadType::kUnspecified});
  registry.RegisterFactory(&factory);

  // When OnNewSession is called, it deletes the session pointer
  EXPECT_CALL(factory, OnNewSession(session_ptr))
      .WillOnce([&session](TransportSession* s) {
        session.reset();
        return nullptr;
      });

  ControlCommand command;
  auto result = session_ptr->ProcessPayload(PayloadType::kUnspecified, command);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            TransportSessionImpl::ProcessPayloadError::kSessionNotFound);
}

TEST(TransportSessionImplTest, MultipleHandlersForSamePayloadType) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory1({PayloadType::kUnspecified});
  MockTransportHandlerFactory factory2({PayloadType::kUnspecified},
                                       kTestFactoryId2);
  registry.RegisterFactory(&factory1);
  registry.RegisterFactory(&factory2);

  auto handler1 = std::make_unique<MockTransportHandler>();
  MockTransportHandler* handler_ptr1 = handler1.get();
  auto handler2 = std::make_unique<MockTransportHandler>();
  MockTransportHandler* handler_ptr2 = handler2.get();

  EXPECT_CALL(factory1, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(handler1)));
  EXPECT_CALL(factory2, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(handler2)));

  ControlCommand command;

  EXPECT_CALL(*handler_ptr1, OnMessage(testing::Ref(command)));
  EXPECT_CALL(*handler_ptr2, OnMessage(testing::Ref(command)));

  EXPECT_TRUE(
      session.ProcessPayload(PayloadType::kUnspecified, command).has_value());
}

TEST(TransportSessionImplTest, NoFactoriesRegistered) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  ControlCommand command;
  auto result = session.ProcessPayload(PayloadType::kUnspecified, command);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            TransportSessionImpl::ProcessPayloadError::kNoFactoriesRegistered);
}

TEST(TransportSessionImplTest, HandlerInstantiationFailed) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory({PayloadType::kUnspecified});
  registry.RegisterFactory(&factory);

  EXPECT_CALL(factory, OnNewSession(&session))
      .WillOnce(testing::Return(nullptr));

  ControlCommand command;
  auto result = session.ProcessPayload(PayloadType::kUnspecified, command);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error(),
      TransportSessionImpl::ProcessPayloadError::kHandlerInstantiationFailed);
}

TEST(TransportSessionImplTest, ProcessDownstreamMessageRoutesControlCommand) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory({PayloadType::kControl});
  registry.RegisterFactory(&factory);

  ControlCommand command;
  command.mutable_close_session();
  std::string serialized_command = command.SerializeAsString();

  auto handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* handler_ptr = handler.get();

  EXPECT_CALL(factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(handler)));
  EXPECT_CALL(*handler_ptr,
              OnMessage(testing::Property(
                  &google::protobuf::MessageLite::SerializeAsString,
                  serialized_command)));

  ActuatorDownstreamMessage message;
  message.set_session_id("test_session");
  message.set_sequence_number(1);
  auto* typed = message.add_typed_payloads();
  typed->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  typed->mutable_proto_payload()->set_value(serialized_command);

  session.ProcessDownstreamMessage(message);
}

TEST(TransportSessionImplTest,
     ProcessDownstreamMessageIgnoresUnspecifiedPayload) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory({PayloadType::kUnspecified});
  registry.RegisterFactory(&factory);

  EXPECT_CALL(factory, OnNewSession).Times(0);

  ActuatorDownstreamMessage message;
  message.set_session_id("test_session");
  message.set_sequence_number(1);
  auto* typed = message.add_typed_payloads();
  typed->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_UNSPECIFIED);

  session.ProcessDownstreamMessage(message);
}

TEST(TransportSessionImplTest, OnMessage) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory({PayloadType::kControl});
  registry.RegisterFactory(&factory);

  auto handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* handler_ptr = handler.get();

  EXPECT_CALL(factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(handler)));

  ControlCommand command;
  command.mutable_close_session();

  EXPECT_CALL(*handler_ptr, OnMessage(testing::Ref(command)));

  session.OnMessage(PayloadType::kControl, command);
}

TEST(TransportSessionImplTest, StartTimeReturnsCreationTimestamp) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MockTransportChannel channel;
  base::TimeTicks expected_time = base::TimeTicks::Now();
  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  EXPECT_EQ(session.start_time(), expected_time);
}

}  // namespace
}  // namespace browser_actuator
