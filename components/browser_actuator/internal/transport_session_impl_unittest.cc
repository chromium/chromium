// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_session_impl.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
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

  EXPECT_CALL(channel,
              SendUpstreamMessage("test_session", PayloadType::kUnspecified,
                                  "test_payload"));
  EXPECT_TRUE(session.SendMessage(PayloadType::kUnspecified, "test_payload")
                  .has_value());
}

TEST(TransportSessionImplTest, SendMessageAfterChannelDestruction) {
  auto channel = std::make_unique<MockTransportChannel>();
  TransportSessionImpl session("test_session", channel->GetWeakPtr());

  EXPECT_CALL(*channel, SendUpstreamMessage(testing::_, testing::_, testing::_))
      .Times(0);
  channel.reset();
  base::expected<void, SendMessageError> result =
      session.SendMessage(PayloadType::kUnspecified, "test_payload");
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
      .WillOnce(testing::Return(testing::ByMove(std::move(handler))));
  EXPECT_CALL(*handler_ptr, OnMessage(testing::_))
      .WillRepeatedly(
          [&message_count](std::string_view payload) { message_count++; });

  // First dispatch triggers factory.OnNewSession
  EXPECT_TRUE(session.ProcessPayload(PayloadType::kUnspecified, "hello_payload")
                  .has_value());

  // Second dispatch should reuse the handler (OnNewSession not called again)
  EXPECT_TRUE(
      session.ProcessPayload(PayloadType::kUnspecified, "second_payload")
          .has_value());

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
      .WillOnce(testing::Return(testing::ByMove(std::move(handler1))));
  EXPECT_CALL(factory2, OnNewSession(session_ptr))
      .WillOnce(testing::Return(testing::ByMove(std::move(handler2))));

  // The second handler must not be called because the loop should break after
  // the session is destroyed.
  EXPECT_CALL(*handler_ptr2, OnMessage(testing::_)).Times(0);

  // This should not crash and should return success.
  EXPECT_TRUE(
      session_ptr->ProcessPayload(PayloadType::kUnspecified, "hello_payload")
          .has_value());
}

TEST(TransportSessionImplTest, ProcessPayloadAfterChannelDestruction) {
  auto channel = std::make_unique<MockTransportChannel>();
  TransportSessionImpl session("test_session", channel->GetWeakPtr());

  channel.reset();
  auto result =
      session.ProcessPayload(PayloadType::kUnspecified, "test_payload");
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

  auto result =
      session_ptr->ProcessPayload(PayloadType::kUnspecified, "hello_payload");
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            TransportSessionImpl::ProcessPayloadError::kSessionNotFound);
}

}  // namespace
}  // namespace browser_actuator
