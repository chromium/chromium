// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/transport_session_impl.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/browser_actuator/internal/transport/downstream_message_sequencer.h"
#include "components/browser_actuator/internal/transport/metrics_utils.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport/transport_handler_factory_registry_impl.h"
#include "components/browser_actuator/internal/transport/transport_session_registry_impl.h"
#include "components/browser_actuator/public/transport/payload_type_mapping.h"
#include "components/browser_actuator/test_support/transport/mock_transport_channel.h"
#include "components/browser_actuator/test_support/transport/mock_transport_handler.h"
#include "components/browser_actuator/test_support/transport/mock_transport_handler_factory.h"
#include "components/browser_actuator/test_support/transport/test_constants.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator::transport {
namespace {

constexpr std::string_view kTestPayload = "test-payload";

ActuatorDownstreamMessage MakeDownstreamMessage(
    ActuatorDownstreamPayloadType wire_type,
    std::string_view type_url,
    std::string_view value,
    int64_t sequence_number = 1) {
  ActuatorDownstreamMessage message;
  message.set_session_id("test_session");
  message.set_sequence_number(sequence_number);
  auto* typed = message.add_typed_payloads();
  typed->set_payload_type(wire_type);
  typed->mutable_proto_payload()->set_type_url(std::string(type_url));
  typed->mutable_proto_payload()->set_value(std::string(value));
  return message;
}

ActuatorDownstreamMessage MakeSequenceOnlyMessage(int64_t sequence_number) {
  ActuatorDownstreamMessage message;
  message.set_session_id("test_session");
  message.set_sequence_number(sequence_number);
  return message;
}

TEST(TransportSessionImplTest, GetSessionId) {
  MockTransportChannel channel;
  TransportSessionImpl session("test_session", channel.GetWeakPtr());
  EXPECT_EQ(session.GetSessionId(), "test_session");
}

TEST(TransportSessionImplTest, SendUpstreamMessage) {
  MockTransportChannel channel;
  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  ControlCommand command;
  command.mutable_close_channel();

  EXPECT_CALL(channel, SendUpstreamMessage(
                           "test_session", PayloadType::kUnspecified,
                           testing::Ref(command), std::optional<int64_t>()));
  EXPECT_TRUE(session.SendUpstreamMessage(PayloadType::kUnspecified, command)
                  .has_value());
}

TEST(TransportSessionImplTest,
     SendUpstreamControlMessageDuringDownstreamDispatchSetsRespondingToSeq) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;
  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory control_factory({PayloadType::kControl});
  MockTransportHandlerFactory feature_factory(
      {PayloadType::kExperimentalTriggering}, kTestFactoryId2);
  registry.RegisterFactory(&control_factory);
  registry.RegisterFactory(&feature_factory);

  auto control_handler = std::make_unique<MockTransportHandler>(&session);
  MockTransportHandler* control_handler_ptr = control_handler.get();
  EXPECT_CALL(control_factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(control_handler)));

  auto feature_handler = std::make_unique<MockTransportHandler>(&session);
  MockTransportHandler* feature_handler_ptr = feature_handler.get();
  EXPECT_CALL(feature_factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(feature_handler)));

  ControlCommand ack;
  ack.mutable_start_session_ack();

  EXPECT_CALL(*control_handler_ptr, OnMessage)
      .WillOnce([&](PayloadType, std::string_view) {
        EXPECT_TRUE(session.SendUpstreamMessage(PayloadType::kControl, ack)
                        .has_value());
      });
  EXPECT_CALL(*feature_handler_ptr, OnMessage)
      .WillOnce([&](PayloadType, std::string_view) {
        EXPECT_TRUE(
            session
                .SendUpstreamMessage(PayloadType::kExperimentalTriggering, ack)
                .has_value());
      });

  // Control reply during ProcessDownstreamMessage attaches sequence_number 1.
  EXPECT_CALL(channel, SendUpstreamMessage(
                           "test_session", PayloadType::kControl,
                           testing::Ref(ack), std::optional<int64_t>(1)));
  // Non-control reply omits responding_to_sequence_number.
  EXPECT_CALL(channel, SendUpstreamMessage(
                           "test_session", PayloadType::kExperimentalTriggering,
                           testing::Ref(ack), std::optional<int64_t>()));

  ActuatorDownstreamMessage downstream =
      MakeDownstreamMessage(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
                            ExpectedTypeUrl(PayloadType::kControl), "payload",
                            /*sequence_number=*/1);
  auto* feature_payload = downstream.add_typed_payloads();
  feature_payload->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  feature_payload->mutable_proto_payload()->set_type_url(
      std::string(ExpectedTypeUrl(PayloadType::kExperimentalTriggering)));
  feature_payload->mutable_proto_payload()->set_value("payload");

  EXPECT_OK(session.ProcessDownstreamMessage(downstream));
}

TEST(TransportSessionImplTest, SendUpstreamMessageAfterChannelDestruction) {
  auto channel = std::make_unique<MockTransportChannel>();
  TransportSessionImpl session("test_session", channel->GetWeakPtr());

  ControlCommand command;
  command.mutable_close_channel();

  EXPECT_CALL(*channel, SendUpstreamMessage).Times(0);
  channel.reset();
  base::expected<void, SendUpstreamMessageError> result =
      session.SendUpstreamMessage(PayloadType::kUnspecified, command);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SendUpstreamMessageError::kChannelDisconnected);
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

  EXPECT_EQ(session.last_seen_sequence_number(), -1);
  EXPECT_FALSE(session.has_last_seen_sequence_number());

  // Out-of-band FCM wake message (sequence 0) advances
  // `last_seen_sequence_number()` to 0.
  EXPECT_OK(session.ProcessDownstreamMessage(MakeSequenceOnlyMessage(0)));
  EXPECT_EQ(session.last_seen_sequence_number(), 0);
  EXPECT_TRUE(session.has_last_seen_sequence_number());

  EXPECT_OK(session.ProcessDownstreamMessage(MakeSequenceOnlyMessage(1)));
  EXPECT_EQ(session.last_seen_sequence_number(), 1);
  EXPECT_TRUE(session.has_last_seen_sequence_number());

  EXPECT_OK(session.ProcessDownstreamMessage(MakeSequenceOnlyMessage(2)));
  EXPECT_EQ(session.last_seen_sequence_number(), 2);

  // Duplicate sequence numbers (including late sequence 0 retries) are ignored.
  EXPECT_OK(session.ProcessDownstreamMessage(MakeSequenceOnlyMessage(0)));
  EXPECT_OK(session.ProcessDownstreamMessage(MakeSequenceOnlyMessage(1)));
  EXPECT_EQ(session.last_seen_sequence_number(), 2);

  // Negative sequence numbers are rejected.
  EXPECT_THAT(session.ProcessDownstreamMessage(MakeSequenceOnlyMessage(-1)),
              base::test::ErrorIs(
                  DownstreamMessageSequencer::Error::kInvalidSequenceNumber));
  EXPECT_THAT(session.ProcessDownstreamMessage(MakeSequenceOnlyMessage(-5)),
              base::test::ErrorIs(
                  DownstreamMessageSequencer::Error::kInvalidSequenceNumber));
  EXPECT_EQ(session.last_seen_sequence_number(), 2);
}

TEST(TransportSessionImplTest,
     ProcessDownstreamMessageDispatchesZeroSequenceNumberOnce) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;
  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory({PayloadType::kControl});
  registry.RegisterFactory(&factory);

  auto handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* handler_ptr = handler.get();

  ControlCommand start_cmd;
  start_cmd.mutable_start_session();
  const std::string serialized_start = start_cmd.SerializeAsString();

  EXPECT_CALL(factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(handler)));
  EXPECT_CALL(*handler_ptr, OnMessage(PayloadType::kControl, serialized_start))
      .WillOnce([&](PayloadType, std::string_view) {
        EXPECT_EQ(session.last_seen_sequence_number(), 0);
        EXPECT_TRUE(session.has_last_seen_sequence_number());
      });

  // First wake message (sequence 0) is dispatched to the handler and advances
  // `last_seen_sequence_number()` from -1 to 0.
  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
      ExpectedTypeUrl(PayloadType::kControl), serialized_start,
      /*sequence_number=*/0)));
  EXPECT_EQ(session.last_seen_sequence_number(), 0);
  EXPECT_TRUE(session.has_last_seen_sequence_number());
  EXPECT_EQ(session.consecutive_invalid_sequence_numbers(), 0u);

  // Duplicate wake message (sequence 0) is dropped without invoking the
  // handler again or incrementing `consecutive_invalid_sequence_numbers()`.
  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
      ExpectedTypeUrl(PayloadType::kControl), serialized_start,
      /*sequence_number=*/0)));
  EXPECT_EQ(session.last_seen_sequence_number(), 0);
  EXPECT_TRUE(session.has_last_seen_sequence_number());
  EXPECT_EQ(session.consecutive_invalid_sequence_numbers(), 0u);
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
      .WillRepeatedly(
          [&message_count](PayloadType, std::string_view) { message_count++; });

  // First dispatch triggers factory.OnNewSession
  EXPECT_TRUE(session.ProcessPayload(PayloadType::kUnspecified, kTestPayload)
                  .has_value());

  // Second dispatch should reuse the handler (OnNewSession not called again)
  EXPECT_TRUE(session.ProcessPayload(PayloadType::kUnspecified, kTestPayload)
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
      .WillOnce(testing::Return(std::move(handler1)));
  EXPECT_CALL(factory2, OnNewSession(session_ptr))
      .WillOnce(testing::Return(std::move(handler2)));

  // The second handler must not be called because the loop should break after
  // the session is destroyed.
  EXPECT_CALL(*handler_ptr2, OnMessage).Times(0);

  // This should not crash and should return success.
  EXPECT_TRUE(
      session_ptr->ProcessPayload(PayloadType::kUnspecified, kTestPayload)
          .has_value());
}

TEST(TransportSessionImplTest, ProcessPayloadAfterChannelDestruction) {
  auto channel = std::make_unique<MockTransportChannel>();
  TransportSessionImpl session("test_session", channel->GetWeakPtr());

  channel.reset();
  auto result = session.ProcessPayload(PayloadType::kUnspecified, kTestPayload);
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
      session_ptr->ProcessPayload(PayloadType::kUnspecified, kTestPayload);
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

  EXPECT_CALL(*handler_ptr1,
              OnMessage(PayloadType::kUnspecified, kTestPayload));
  EXPECT_CALL(*handler_ptr2,
              OnMessage(PayloadType::kUnspecified, kTestPayload));

  EXPECT_TRUE(session.ProcessPayload(PayloadType::kUnspecified, kTestPayload)
                  .has_value());
}

TEST(TransportSessionImplTest, NoFactoriesRegistered) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  auto result = session.ProcessPayload(PayloadType::kUnspecified, kTestPayload);
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

  auto result = session.ProcessPayload(PayloadType::kUnspecified, kTestPayload);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error(),
      TransportSessionImpl::ProcessPayloadError::kHandlerInstantiationFailed);
}

TEST(TransportSessionImplTest, ProcessDownstreamMessageRoutesControlCommand) {
  base::HistogramTester histogram_tester;
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
              OnMessage(PayloadType::kControl, serialized_command));

  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
      ExpectedTypeUrl(PayloadType::kControl), serialized_command)));

  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.Downstream.PayloadDropped", 0);
}

// Regression test for crbug.com/560176806.
TEST(TransportSessionImplTest,
     ProcessDownstreamMessageRoutesExperimentalTriggering) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory({PayloadType::kExperimentalTriggering});
  registry.RegisterFactory(&factory);

  auto handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* handler_ptr = handler.get();

  EXPECT_CALL(factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(handler)));
  EXPECT_CALL(*handler_ptr,
              OnMessage(PayloadType::kExperimentalTriggering, kTestPayload));

  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING,
      ExpectedTypeUrl(PayloadType::kExperimentalTriggering), kTestPayload)));
}

TEST(TransportSessionImplTest,
     ProcessDownstreamMessageRoutesMultiplePayloadTypes) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory control_factory({PayloadType::kControl},
                                              FactoryId::kControl);
  MockTransportHandlerFactory triggering_factory(
      {PayloadType::kExperimentalTriggering},
      FactoryId::kExperimentalTriggering);
  registry.RegisterFactory(&control_factory);
  registry.RegisterFactory(&triggering_factory);

  auto control_handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* control_handler_ptr = control_handler.get();
  auto triggering_handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* triggering_handler_ptr = triggering_handler.get();

  EXPECT_CALL(control_factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(control_handler)));
  EXPECT_CALL(triggering_factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(triggering_handler)));
  EXPECT_CALL(*control_handler_ptr,
              OnMessage(PayloadType::kControl, "control-bytes"));
  EXPECT_CALL(
      *triggering_handler_ptr,
      OnMessage(PayloadType::kExperimentalTriggering, "triggering-bytes"));

  ActuatorDownstreamMessage message;
  message.set_session_id("test_session");
  message.set_sequence_number(1);
  auto* control = message.add_typed_payloads();
  control->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  control->mutable_proto_payload()->set_value("control-bytes");
  auto* triggering = message.add_typed_payloads();
  triggering->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  triggering->mutable_proto_payload()->set_value("triggering-bytes");

  EXPECT_OK(session.ProcessDownstreamMessage(message));
}

TEST(TransportSessionImplTest,
     ProcessDownstreamMessageRejectsMismatchedTypeUrl) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory({PayloadType::kControl});
  registry.RegisterFactory(&factory);

  EXPECT_CALL(factory, OnNewSession).Times(0);

  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
      ExpectedTypeUrl(PayloadType::kExperimentalTriggering), kTestPayload)));

  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Downstream.PayloadDropped",
      DownstreamPayloadDropReason::kTypeUrlMismatch, 1);
}

TEST(TransportSessionImplTest, ProcessDownstreamMessageAcceptsEmptyTypeUrl) {
  base::HistogramTester histogram_tester;
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
  EXPECT_CALL(*handler_ptr, OnMessage(PayloadType::kControl, kTestPayload));

  EXPECT_OK(session.ProcessDownstreamMessage(
      MakeDownstreamMessage(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
                            /*type_url=*/"", kTestPayload)));

  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.Downstream.PayloadDropped", 0);
}

TEST(TransportSessionImplTest,
     ProcessDownstreamMessageIgnoresUnknownPayloadType) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory({PayloadType::kControl});
  registry.RegisterFactory(&factory);

  EXPECT_CALL(factory, OnNewSession).Times(0);

  EXPECT_OK(session.ProcessDownstreamMessage(
      MakeDownstreamMessage(static_cast<ActuatorDownstreamPayloadType>(999),
                            /*type_url=*/"", kTestPayload)));

  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Downstream.PayloadDropped",
      DownstreamPayloadDropReason::kUnknownType, 1);
}

TEST(TransportSessionImplTest,
     ProcessDownstreamMessageIgnoresUnspecifiedPayload) {
  base::HistogramTester histogram_tester;
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

  EXPECT_OK(session.ProcessDownstreamMessage(message));

  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Downstream.PayloadDropped",
      DownstreamPayloadDropReason::kUnknownType, 1);
}

TEST(TransportSessionImplTest, ProcessDownstreamMessageReportsMissingHandler) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING,
      ExpectedTypeUrl(PayloadType::kExperimentalTriggering), kTestPayload)));

  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Downstream.PayloadDropped",
      DownstreamPayloadDropReason::kNoHandler, 1);
}

TEST(TransportSessionImplTest, ProcessDownstreamMessageReportsDispatchFailure) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory({PayloadType::kControl});
  registry.RegisterFactory(&factory);

  EXPECT_CALL(factory, OnNewSession(&session))
      .WillOnce(testing::Return(nullptr));

  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
      ExpectedTypeUrl(PayloadType::kControl), kTestPayload)));

  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Downstream.PayloadDropped",
      DownstreamPayloadDropReason::kDispatchFailed, 1);
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
  const std::string serialized_command = command.SerializeAsString();

  EXPECT_CALL(*handler_ptr,
              OnMessage(PayloadType::kControl, serialized_command));

  session.OnMessage(PayloadType::kControl, serialized_command);
}

TEST(TransportSessionImplTest, OnMessageSetsZeroSequenceForControlReplies) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;
  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory control_factory({PayloadType::kControl});
  MockTransportHandlerFactory triggering_factory(
      {PayloadType::kExperimentalTriggering},
      FactoryId::kExperimentalTriggering);
  registry.RegisterFactory(&control_factory);
  registry.RegisterFactory(&triggering_factory);

  auto control_handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* control_handler_ptr = control_handler.get();
  auto triggering_handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* triggering_handler_ptr = triggering_handler.get();

  EXPECT_CALL(control_factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(control_handler)));
  EXPECT_CALL(triggering_factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(triggering_handler)));

  ControlCommand start_cmd;
  start_cmd.mutable_start_session();
  const std::string serialized_start = start_cmd.SerializeAsString();

  EXPECT_CALL(*control_handler_ptr,
              OnMessage(PayloadType::kControl, serialized_start))
      .WillOnce([&](PayloadType, std::string_view) {
        ControlCommand ack;
        ack.mutable_start_session_ack();
        EXPECT_TRUE(session.SendUpstreamMessage(PayloadType::kControl, ack)
                        .has_value());
      });
  EXPECT_CALL(channel,
              SendUpstreamMessage("test_session", PayloadType::kControl,
                                  testing::_, std::optional<int64_t>(0)));

  session.OnMessage(PayloadType::kControl, serialized_start);

  // Subsequent non-control payload in the same wake bundle does not populate
  // `responding_to_sequence_number`.
  EXPECT_CALL(*triggering_handler_ptr,
              OnMessage(PayloadType::kExperimentalTriggering, "trigger-bytes"))
      .WillOnce([&](PayloadType, std::string_view) {
        ControlCommand reply;
        EXPECT_TRUE(session
                        .SendUpstreamMessage(
                            PayloadType::kExperimentalTriggering, reply)
                        .has_value());
      });
  EXPECT_CALL(channel, SendUpstreamMessage(
                           "test_session", PayloadType::kExperimentalTriggering,
                           testing::_, std::optional<int64_t>(std::nullopt)));

  session.OnMessage(PayloadType::kExperimentalTriggering, "trigger-bytes");

  // Unsolicited control message sent outside `OnMessage()` has `std::nullopt`.
  ControlCommand unsolicited;
  EXPECT_CALL(channel, SendUpstreamMessage(
                           "test_session", PayloadType::kControl, testing::_,
                           std::optional<int64_t>(std::nullopt)));
  EXPECT_TRUE(session.SendUpstreamMessage(PayloadType::kControl, unsolicited)
                  .has_value());
}

TEST(TransportSessionImplTest, OnMessageHandlesSessionDestructionSafely) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;
  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  auto session = std::make_unique<TransportSessionImpl>("test_session",
                                                        channel.GetWeakPtr());
  TransportSessionImpl* session_ptr = session.get();

  MockTransportHandlerFactory factory({PayloadType::kControl});
  registry.RegisterFactory(&factory);

  auto handler = std::make_unique<CallbackTransportHandler>(base::BindOnce(
      [](std::unique_ptr<TransportSessionImpl>* s) { s->reset(); }, &session));
  EXPECT_CALL(factory, OnNewSession(session_ptr))
      .WillOnce(testing::Return(std::move(handler)));

  session_ptr->OnMessage(PayloadType::kControl, "close-bytes");
  EXPECT_EQ(session, nullptr);
}

TEST(TransportSessionImplTest, StartTimeReturnsCreationTimestamp) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MockTransportChannel channel;
  base::TimeTicks expected_time = base::TimeTicks::Now();
  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  EXPECT_EQ(session.start_time(), expected_time);
}

TEST(TransportSessionImplTest,
     ProcessDownstreamMessageBuffersOutOfOrderAndDrainsInSequence) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;
  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory({PayloadType::kExperimentalTriggering});
  registry.RegisterFactory(&factory);

  auto handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* handler_ptr = handler.get();

  std::vector<std::pair<std::string, int64_t>> delivered;
  EXPECT_CALL(factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(handler)));
  EXPECT_CALL(*handler_ptr,
              OnMessage(PayloadType::kExperimentalTriggering, testing::_))
      .WillRepeatedly([&](PayloadType, std::string_view payload) {
        delivered.emplace_back(std::string(payload),
                               session.last_seen_sequence_number());
      });

  // Sequence 3 and 2 arrive before 1 and must be buffered without advancing
  // `last_seen_sequence_number()`.
  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING,
      ExpectedTypeUrl(PayloadType::kExperimentalTriggering), "payload-3",
      /*sequence_number=*/3)));
  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING,
      ExpectedTypeUrl(PayloadType::kExperimentalTriggering), "payload-2",
      /*sequence_number=*/2)));
  EXPECT_TRUE(delivered.empty());
  EXPECT_EQ(session.last_seen_sequence_number(), -1);

  // Sequence 1 fills the gap and drains 1, 2, 3 in ascending order, advancing
  // `last_seen_sequence_number()` before each message's handlers run.
  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING,
      ExpectedTypeUrl(PayloadType::kExperimentalTriggering), "payload-1",
      /*sequence_number=*/1)));

  EXPECT_THAT(delivered, testing::ElementsAre(
                             std::pair<std::string, int64_t>("payload-1", 1),
                             std::pair<std::string, int64_t>("payload-2", 2),
                             std::pair<std::string, int64_t>("payload-3", 3)));
  EXPECT_EQ(session.last_seen_sequence_number(), 3);
}

TEST(TransportSessionImplTest,
     ProcessDownstreamMessageFlushesBufferOnTerminatingControlCommand) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;
  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory control_factory({PayloadType::kControl},
                                              FactoryId::kControl);
  MockTransportHandlerFactory triggering_factory(
      {PayloadType::kExperimentalTriggering},
      FactoryId::kExperimentalTriggering);
  registry.RegisterFactory(&control_factory);
  registry.RegisterFactory(&triggering_factory);

  auto control_handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* control_handler_ptr = control_handler.get();
  auto triggering_handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* triggering_handler_ptr = triggering_handler.get();

  std::vector<int64_t> delivered_sequences;
  EXPECT_CALL(control_factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(control_handler)));
  EXPECT_CALL(triggering_factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(triggering_handler)));
  EXPECT_CALL(*control_handler_ptr,
              OnMessage(PayloadType::kControl, testing::_))
      .WillRepeatedly([&](PayloadType, std::string_view) {
        delivered_sequences.push_back(session.last_seen_sequence_number());
      });
  EXPECT_CALL(*triggering_handler_ptr,
              OnMessage(PayloadType::kExperimentalTriggering, testing::_))
      .WillRepeatedly([&](PayloadType, std::string_view) {
        delivered_sequences.push_back(session.last_seen_sequence_number());
      });

  ControlCommand start_cmd;
  start_cmd.mutable_start_session();
  std::string serialized_start = start_cmd.SerializeAsString();

  ControlCommand close_cmd;
  close_cmd.mutable_close_session();
  std::string serialized_close = close_cmd.SerializeAsString();

  // Deliver sequence 1 in-order so `last_seen_sequence_number()` is 1.
  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
      ExpectedTypeUrl(PayloadType::kControl), serialized_start,
      /*sequence_number=*/1)));
  EXPECT_THAT(delivered_sequences, testing::ElementsAre(1));

  // Buffer sequences 3 and 5 behind missing sequence 2. Neither a
  // non-terminating ControlCommand (StartSession at sequence 6) nor a duplicate
  // terminating ControlCommand (CloseSession at sequence 1) may flush them.
  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING,
      ExpectedTypeUrl(PayloadType::kExperimentalTriggering), "buffered-3",
      /*sequence_number=*/3)));
  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING,
      ExpectedTypeUrl(PayloadType::kExperimentalTriggering), "buffered-5",
      /*sequence_number=*/5)));
  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
      ExpectedTypeUrl(PayloadType::kControl), serialized_start,
      /*sequence_number=*/6)));
  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
      ExpectedTypeUrl(PayloadType::kControl), serialized_close,
      /*sequence_number=*/1)));
  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
      ExpectedTypeUrl(PayloadType::kControl), serialized_close,
      /*sequence_number=*/0)));
  EXPECT_THAT(delivered_sequences, testing::ElementsAre(1));

  // Terminating CloseSession at sequence 4 (inside the gap below buffered 5 &
  // 6) flushes the buffer and dispatches [3, 4, 5, 6] in ascending order.
  EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
      ExpectedTypeUrl(PayloadType::kControl), serialized_close,
      /*sequence_number=*/4)));
  EXPECT_THAT(delivered_sequences, testing::ElementsAre(1, 3, 4, 5, 6));
  EXPECT_EQ(session.last_seen_sequence_number(), 6);
}

TEST(TransportSessionImplTest,
     ProcessDownstreamMessageFlushesFullBufferOnTerminatingControlCommand) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;
  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory control_factory({PayloadType::kControl},
                                              FactoryId::kControl);
  MockTransportHandlerFactory triggering_factory(
      {PayloadType::kExperimentalTriggering},
      FactoryId::kExperimentalTriggering);
  registry.RegisterFactory(&control_factory);
  registry.RegisterFactory(&triggering_factory);

  auto control_handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* control_handler_ptr = control_handler.get();
  auto triggering_handler = std::make_unique<MockTransportHandler>();
  MockTransportHandler* triggering_handler_ptr = triggering_handler.get();

  std::vector<int64_t> delivered_sequences;
  EXPECT_CALL(control_factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(control_handler)));
  EXPECT_CALL(triggering_factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(triggering_handler)));
  EXPECT_CALL(*control_handler_ptr,
              OnMessage(PayloadType::kControl, testing::_))
      .WillRepeatedly([&](PayloadType, std::string_view) {
        delivered_sequences.push_back(session.last_seen_sequence_number());
      });
  EXPECT_CALL(*triggering_handler_ptr,
              OnMessage(PayloadType::kExperimentalTriggering, testing::_))
      .WillRepeatedly([&](PayloadType, std::string_view) {
        delivered_sequences.push_back(session.last_seen_sequence_number());
      });

  // Fill the reorder buffer to `kDefaultMaxBufferedMessages` behind missing
  // sequence 1.
  std::vector<int64_t> expected_sequences;
  for (size_t i = 0;
       i < DownstreamMessageSequencer::kDefaultMaxBufferedMessages; ++i) {
    const int64_t seq = static_cast<int64_t>(i + 2);
    expected_sequences.push_back(seq);
    EXPECT_OK(session.ProcessDownstreamMessage(MakeDownstreamMessage(
        ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING,
        ExpectedTypeUrl(PayloadType::kExperimentalTriggering), "buffered",
        seq)));
  }
  EXPECT_TRUE(delivered_sequences.empty());

  // A terminating CloseSession command arriving when the buffer is already
  // full bypasses the capacity check, drains the full buffer, and completes.
  ControlCommand close_cmd;
  close_cmd.mutable_close_session();
  const int64_t close_seq = static_cast<int64_t>(
      DownstreamMessageSequencer::kDefaultMaxBufferedMessages + 2);
  expected_sequences.push_back(close_seq);
  EXPECT_OK(session.ProcessDownstreamMessage(
      MakeDownstreamMessage(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
                            ExpectedTypeUrl(PayloadType::kControl),
                            close_cmd.SerializeAsString(), close_seq)));

  EXPECT_EQ(delivered_sequences, expected_sequences);
  EXPECT_EQ(session.last_seen_sequence_number(), close_seq);
}

TEST(TransportSessionImplTest, HandlerDestroysSessionDuringMultiMessageDrain) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;
  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  auto session = std::make_unique<TransportSessionImpl>("test_session",
                                                        channel.GetWeakPtr());
  TransportSessionImpl* session_ptr = session.get();

  MockTransportHandlerFactory factory({PayloadType::kExperimentalTriggering});
  registry.RegisterFactory(&factory);

  int dispatch_count = 0;
  auto handler = std::make_unique<CallbackTransportHandler>(base::BindOnce(
      [](std::unique_ptr<TransportSessionImpl>* s, int* count) {
        ++(*count);
        s->reset();
      },
      &session, &dispatch_count));
  EXPECT_CALL(factory, OnNewSession(session_ptr))
      .WillOnce(testing::Return(std::move(handler)));

  // Buffer sequence 2 behind missing sequence 1, then deliver sequence 1 so
  // [1, 2] drain in a single batch while message 1's handler destroys
  // `session`.
  EXPECT_OK(session_ptr->ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING,
      ExpectedTypeUrl(PayloadType::kExperimentalTriggering), "msg-2",
      /*sequence_number=*/2)));
  EXPECT_OK(session_ptr->ProcessDownstreamMessage(MakeDownstreamMessage(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING,
      ExpectedTypeUrl(PayloadType::kExperimentalTriggering), "msg-1",
      /*sequence_number=*/1)));
  EXPECT_EQ(dispatch_count, 1);
  EXPECT_EQ(session, nullptr);
}

TEST(TransportSessionImplTest,
     ProcessDownstreamMessageReturnsBufferFullOnOverflow) {
  MockTransportChannel channel;
  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  for (size_t i = 0;
       i < DownstreamMessageSequencer::kDefaultMaxBufferedMessages; ++i) {
    EXPECT_OK(session.ProcessDownstreamMessage(
        MakeSequenceOnlyMessage(static_cast<int64_t>(i + 2))));
  }

  EXPECT_THAT(
      session.ProcessDownstreamMessage(
          MakeSequenceOnlyMessage(static_cast<int64_t>(
              DownstreamMessageSequencer::kDefaultMaxBufferedMessages + 2))),
      base::test::ErrorIs(DownstreamMessageSequencer::Error::kBufferFull));
  EXPECT_EQ(session.last_seen_sequence_number(), -1);
}

TEST(TransportSessionImplTest,
     TracksAndResetsConsecutiveInvalidSequenceNumbers) {
  MockTransportChannel channel;
  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  EXPECT_EQ(session.consecutive_invalid_sequence_numbers(), 0u);

  EXPECT_THAT(session.ProcessDownstreamMessage(MakeSequenceOnlyMessage(-1)),
              base::test::ErrorIs(
                  DownstreamMessageSequencer::Error::kInvalidSequenceNumber));
  EXPECT_EQ(session.consecutive_invalid_sequence_numbers(), 1u);

  EXPECT_THAT(session.ProcessDownstreamMessage(MakeSequenceOnlyMessage(-2)),
              base::test::ErrorIs(
                  DownstreamMessageSequencer::Error::kInvalidSequenceNumber));
  EXPECT_EQ(session.consecutive_invalid_sequence_numbers(), 2u);

  // An out-of-band wake message (sequence 0) resets the consecutive counter.
  EXPECT_OK(session.ProcessDownstreamMessage(MakeSequenceOnlyMessage(0)));
  EXPECT_EQ(session.consecutive_invalid_sequence_numbers(), 0u);

  EXPECT_THAT(session.ProcessDownstreamMessage(MakeSequenceOnlyMessage(-1)),
              base::test::ErrorIs(
                  DownstreamMessageSequencer::Error::kInvalidSequenceNumber));
  EXPECT_EQ(session.consecutive_invalid_sequence_numbers(), 1u);

  // A message with a valid stream sequence number also resets the counter.
  EXPECT_OK(session.ProcessDownstreamMessage(MakeSequenceOnlyMessage(1)));
  EXPECT_EQ(session.consecutive_invalid_sequence_numbers(), 0u);
}

TEST(TransportSessionImplTest,
     UnregisterFactoryDestroysHandlerAndStopsRouting) {
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl registry;

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&registry));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory1({PayloadType::kUnspecified},
                                       kTestFactoryId1);
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

  EXPECT_CALL(*handler_ptr1, OnMessage(PayloadType::kUnspecified, kTestPayload))
      .Times(1);
  EXPECT_CALL(*handler_ptr2, OnMessage(PayloadType::kUnspecified, kTestPayload))
      .Times(2);

  EXPECT_TRUE(session.ProcessPayload(PayloadType::kUnspecified, kTestPayload)
                  .has_value());

  registry.UnregisterFactory(&factory1);

  EXPECT_TRUE(session.ProcessPayload(PayloadType::kUnspecified, kTestPayload)
                  .has_value());
}

TEST(TransportSessionImplTest,
     RegistryDestroyedBeforeSessionResetsObservation) {
  MockTransportChannel channel;
  auto registry = std::make_unique<TransportHandlerFactoryRegistryImpl>();

  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(registry.get()));

  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  MockTransportHandlerFactory factory({PayloadType::kUnspecified},
                                      kTestFactoryId1);
  registry->RegisterFactory(&factory);

  auto handler = std::make_unique<MockTransportHandler>();
  EXPECT_CALL(*handler, OnMessage(PayloadType::kUnspecified, kTestPayload))
      .Times(1);
  EXPECT_CALL(factory, OnNewSession(&session))
      .WillOnce(testing::Return(std::move(handler)));

  EXPECT_TRUE(session.ProcessPayload(PayloadType::kUnspecified, kTestPayload)
                  .has_value());

  registry.reset();
}

TEST(TransportSessionImplTest, CloseSendsCloseSessionAndDestroysSession) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl session_registry(channel.GetWeakPtr());
  EXPECT_CALL(channel, GetSessionRegistry())
      .WillRepeatedly(testing::Return(&session_registry));

  TransportSession* session = session_registry.CreateSession("s1");
  ASSERT_NE(session, nullptr);
  EXPECT_NE(session_registry.GetSession("s1"), nullptr);

  EXPECT_CALL(channel,
              SendUpstreamMessage(std::string_view("s1"), PayloadType::kControl,
                                  testing::_, std::optional<int64_t>()))
      .WillOnce([&](std::string_view, PayloadType,
                    const google::protobuf::MessageLite& msg,
                    std::optional<int64_t>) {
        // Session must still exist in the registry when SendUpstreamMessage
        // runs so TransportChannelImpl::SendUpstreamMessage can look it up.
        EXPECT_NE(session_registry.GetSession("s1"), nullptr);
        const auto& cmd = static_cast<const ControlCommand&>(msg);
        EXPECT_TRUE(cmd.has_close_session());
      });

  session->Close();
  EXPECT_EQ(session_registry.GetSession("s1"), nullptr);
}

}  // namespace
}  // namespace browser_actuator::transport
