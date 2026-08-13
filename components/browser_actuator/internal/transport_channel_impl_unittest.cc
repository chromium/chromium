// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_channel_impl.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "components/browser_actuator/internal/control_transport_handler.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport/message_stream_client.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "components/browser_actuator/internal/transport_session_impl.h"
#include "components/browser_actuator/internal/transport_session_registry_impl.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "components/browser_actuator/public/transport_handler_factory.h"
#include "components/browser_actuator/public/transport_handler_factory_registry.h"
#include "components/browser_actuator/test_support/mock_transport_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

using ::testing::AllOf;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

// Minimal MessageStreamClient that lets the test push downstream messages
// straight to the channel's observer.
class FakeMessageStreamClient : public MessageStreamClient {
 public:
  FakeMessageStreamClient() = default;

  void Connect() override {
    is_connected_ = true;
    connect_count_++;
  }
  void Disconnect() override {
    is_connected_ = false;
    disconnect_count_++;
    disconnected_called_ = true;
  }
  bool IsConnected() const override { return is_connected_; }
  void AddObserver(Observer* observer) override { observer_ = observer; }
  void RemoveObserver(Observer* observer) override {
    if (observer_ == observer) {
      observer_ = nullptr;
    }
  }

  void set_connected(bool connected) { is_connected_ = connected; }
  int connect_count() const { return connect_count_; }
  int disconnect_count() const { return disconnect_count_; }

  void Dispatch(const std::string& message) {
    ASSERT_TRUE(observer_);
    observer_->OnStreamMessage(message);
  }

  bool disconnected_called() const { return disconnected_called_; }

  base::WeakPtr<FakeMessageStreamClient> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool disconnected_called_ = false;
  raw_ptr<Observer> observer_ = nullptr;
  bool is_connected_ = false;
  int connect_count_ = 0;
  int disconnect_count_ = 0;

  base::WeakPtrFactory<FakeMessageStreamClient> weak_factory_{this};
};

class FakeTransportHandler : public TransportHandler {
 public:
  explicit FakeTransportHandler(
      base::RepeatingCallback<void(const google::protobuf::MessageLite&)>
          on_message_cb)
      : on_message_cb_(std::move(on_message_cb)) {}
  ~FakeTransportHandler() override = default;

  void OnMessage(const google::protobuf::MessageLite& message) override {
    on_message_cb_.Run(message);
  }

 private:
  base::RepeatingCallback<void(const google::protobuf::MessageLite&)>
      on_message_cb_;
};

class FakeTransportHandlerFactory : public TransportHandlerFactory {
 public:
  FakeTransportHandlerFactory(
      const std::vector<PayloadType>& supported_types,
      base::RepeatingCallback<std::unique_ptr<TransportHandler>(
          TransportSession*)> on_new_session_cb,
      FactoryId factory_id = FactoryId::kUnset)
      : supported_types_(supported_types),
        on_new_session_cb_(std::move(on_new_session_cb)),
        factory_id_(factory_id) {}
  ~FakeTransportHandlerFactory() override = default;

  FactoryId GetFactoryId() const override { return factory_id_; }

  std::vector<PayloadType> GetSupportedPayloadTypes() const override {
    return supported_types_;
  }

  std::unique_ptr<TransportHandler> OnNewSession(
      TransportSession* session) override {
    return on_new_session_cb_.Run(session);
  }

 private:
  const std::vector<PayloadType> supported_types_;
  base::RepeatingCallback<std::unique_ptr<TransportHandler>(TransportSession*)>
      on_new_session_cb_;
  const FactoryId factory_id_;
};

std::string SerializedDownstream(std::string_view session_id, int64_t seq) {
  WatchSessionsResponse response;
  ActuatorDownstreamMessage* message =
      response.mutable_actuator_downstream_message();
  message->set_session_id(session_id);
  message->set_sequence_number(seq);
  return response.SerializeAsString();
}

class TransportChannelImplTest : public testing::Test {
 protected:
  TransportChannelImplTest() {
    auto client = std::make_unique<FakeMessageStreamClient>();
    fake_client_ = client->GetWeakPtr();
    channel_ = std::make_unique<TransportChannelImpl>(base::BindOnce(
        [](std::unique_ptr<FakeMessageStreamClient> client,
           std::unique_ptr<StreamConnectionDelegate>)
            -> std::unique_ptr<MessageStreamClient> { return client; },
        std::move(client)));
  }

  WatchSessionsRequest ResumeBody() {
    WatchSessionsRequest request;
    EXPECT_TRUE(request.ParseFromString(
        channel_->BuildWatchSessionsRequestBodyForTesting()));
    return request;
  }

  TransportSessionRegistryImpl* GetSessionRegistryImpl() {
    return static_cast<TransportSessionRegistryImpl*>(
        channel_->GetSessionRegistry());
  }

  std::unique_ptr<TransportChannelImpl> channel_;
  base::WeakPtr<FakeMessageStreamClient> fake_client_;
};

TEST_F(TransportChannelImplTest, RecordsPerSessionResumePositions) {
  fake_client_->Dispatch(SerializedDownstream("s1", 5));
  fake_client_->Dispatch(SerializedDownstream("s2", 2));

  const WatchSessionsRequest body = ResumeBody();
  EXPECT_FALSE(body.request_id().empty());
  EXPECT_THAT(
      body.sessions(),
      UnorderedElementsAre(
          AllOf(Property(&WatchSessionsRequest::Session::session_id, "s1"),
                Property(
                    &WatchSessionsRequest::Session::last_seen_sequence_number,
                    5)),
          AllOf(Property(&WatchSessionsRequest::Session::session_id, "s2"),
                Property(
                    &WatchSessionsRequest::Session::last_seen_sequence_number,
                    2))));
}

TEST_F(TransportChannelImplTest, AdvanceIsMonotonicAndIgnoresNonPositive) {
  fake_client_->Dispatch(SerializedDownstream("s1", 5));
  fake_client_->Dispatch(SerializedDownstream("s1", 3));  // Out of order.
  fake_client_->Dispatch(SerializedDownstream("s1", 0));  // Unset.

  const WatchSessionsRequest body = ResumeBody();
  ASSERT_EQ(body.sessions_size(), 1);
  EXPECT_EQ(body.sessions(0).session_id(), "s1");
  EXPECT_EQ(body.sessions(0).last_seen_sequence_number(), 5);
}

TEST_F(TransportChannelImplTest,
       SessionWithoutResumePositionIsIncludedInWatchRequest) {
  GetSessionRegistryImpl()->GetOrCreateSession("s1");

  const WatchSessionsRequest body = ResumeBody();
  ASSERT_EQ(body.sessions_size(), 1);
  EXPECT_EQ(body.sessions(0).session_id(), "s1");
  EXPECT_EQ(body.sessions(0).last_seen_sequence_number(), 0);
}

TEST_F(TransportChannelImplTest,
       ReconnectsWhenNewSessionRegisteredAndConnected) {
  fake_client_->set_connected(true);

  GetSessionRegistryImpl()->GetOrCreateSession("s1");

  EXPECT_EQ(fake_client_->disconnect_count(), 1);
  EXPECT_EQ(fake_client_->connect_count(), 1);
}

TEST_F(TransportChannelImplTest,
       DoesNotReconnectWhenExistingSessionRegistered) {
  fake_client_->set_connected(true);
  GetSessionRegistryImpl()->GetOrCreateSession("s1");

  GetSessionRegistryImpl()->GetOrCreateSession("s1");

  EXPECT_EQ(fake_client_->disconnect_count(), 1);
  EXPECT_EQ(fake_client_->connect_count(), 1);
}

TEST_F(TransportChannelImplTest,
       ConnectsWhenNewSessionRegisteredAndDisconnected) {
  fake_client_->set_connected(false);

  GetSessionRegistryImpl()->GetOrCreateSession("s1");

  EXPECT_EQ(fake_client_->disconnect_count(), 0);
  EXPECT_EQ(fake_client_->connect_count(), 1);
}

struct TestPayload {
  ActuatorDownstreamPayloadType type;
  std::string value;
};

std::string SerializedDownstreamMessage(
    std::string_view session_id,
    int64_t seq,
    const std::vector<TestPayload>& payloads) {
  WatchSessionsResponse response;
  ActuatorDownstreamMessage* message =
      response.mutable_actuator_downstream_message();
  message->set_session_id(std::string(session_id));
  message->set_sequence_number(seq);

  for (const auto& payload : payloads) {
    auto* typed = message->add_typed_payloads();
    typed->set_payload_type(payload.type);
    typed->mutable_proto_payload()->set_value(payload.value);
  }

  return response.SerializeAsString();
}

TEST_F(TransportChannelImplTest, RoutesPayloadTypeToHandler) {
  ControlCommand command;

  bool message_handled = false;
  FakeTransportHandlerFactory factory(
      {PayloadType::kControl},
      base::BindLambdaForTesting(
          [&](TransportSession*) -> std::unique_ptr<TransportHandler> {
            return std::make_unique<FakeTransportHandler>(
                base::BindLambdaForTesting(
                    [&](const google::protobuf::MessageLite&) {
                      message_handled = true;
                    }));
          }));
  channel_->GetHandlerFactoryRegistry()->RegisterFactory(&factory);

  fake_client_->Dispatch(SerializedDownstreamMessage(
      "s1", 1,
      {{ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
        command.SerializeAsString()}}));

  EXPECT_TRUE(message_handled);
}

TEST_F(TransportChannelImplTest, SessionDestructionMidLoop) {
  TransportSessionRegistryImpl* registry =
      static_cast<TransportSessionRegistryImpl*>(
          channel_->GetSessionRegistry());
  registry->GetOrCreateSession("s1");

  ControlCommand command;

  bool first_message_handled = false;
  FakeTransportHandlerFactory factory(
      {PayloadType::kControl},
      base::BindLambdaForTesting(
          [&](TransportSession*) -> std::unique_ptr<TransportHandler> {
            return std::make_unique<CallbackTransportHandler>(base::BindOnce(
                [](TransportSessionRegistryImpl* r, bool* handled) {
                  *handled = true;
                  r->DestroySession("s1");
                },
                registry, &first_message_handled));
          }));
  channel_->GetHandlerFactoryRegistry()->RegisterFactory(&factory);

  fake_client_->Dispatch(SerializedDownstreamMessage(
      "s1", 1,
      {{ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
        command.SerializeAsString()},
       {ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
        command.SerializeAsString()}}));

  EXPECT_TRUE(first_message_handled);
  EXPECT_EQ(registry->GetSession("s1"), nullptr);
}

TEST_F(TransportChannelImplTest, ControlHandlerFactoryRegisteredByDefault) {
  std::vector<TransportHandlerFactory*> factories =
      channel_->GetHandlerFactoryRegistry()->GetFactories(
          PayloadType::kControl);
  ASSERT_EQ(factories.size(), 1u);
  EXPECT_EQ(factories[0]->GetFactoryId(), FactoryId::kControl);
}

TEST_F(TransportChannelImplTest, ControlCommandCloseSessionDestroysSession) {
  TransportSessionRegistryImpl* registry =
      static_cast<TransportSessionRegistryImpl*>(
          channel_->GetSessionRegistry());
  ASSERT_NE(registry->GetOrCreateSession("s1"), nullptr);

  ControlCommand command;
  command.mutable_close_session();

  fake_client_->Dispatch(SerializedDownstreamMessage(
      "s1", 1,
      {{ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
        command.SerializeAsString()}}));

  // The session was created, processed the CloseSession command, and was
  // destroyed.
  EXPECT_EQ(registry->GetSession("s1"), nullptr);
}

TEST_F(TransportChannelImplTest, ControlCommandCloseChannelDisconnectsStream) {
  EXPECT_FALSE(fake_client_->disconnected_called());

  ControlCommand command;
  command.mutable_close_channel();

  fake_client_->Dispatch(SerializedDownstreamMessage(
      "s1", 1,
      {{ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
        command.SerializeAsString()}}));

  // The CloseChannel command triggers Disconnect on the stream client.
  EXPECT_TRUE(fake_client_->disconnected_called());
}

TEST_F(TransportChannelImplTest, PassesSequenceNumberToSession) {
  TransportSessionRegistryImpl* registry =
      static_cast<TransportSessionRegistryImpl*>(
          channel_->GetSessionRegistry());

  fake_client_->Dispatch(SerializedDownstreamMessage(
      "s1", 2,
      {{ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_UNSPECIFIED, "payload_data"}}));

  TransportSessionImpl* session = registry->GetSessionImpl("s1");
  ASSERT_NE(session, nullptr);
  EXPECT_EQ(session->last_seen_sequence_number(), 2);
}

TEST_F(TransportChannelImplTest, UnknownPayloadTypesAreIgnored) {
  bool factory_called = false;
  FakeTransportHandlerFactory factory(
      {PayloadType::kUnspecified},
      base::BindLambdaForTesting(
          [&](TransportSession*) -> std::unique_ptr<TransportHandler> {
            factory_called = true;
            return nullptr;
          }));
  channel_->GetHandlerFactoryRegistry()->RegisterFactory(&factory);

  ActuatorDownstreamPayloadType unknown_type =
      static_cast<ActuatorDownstreamPayloadType>(999);

  fake_client_->Dispatch(
      SerializedDownstreamMessage("s1", 1, {{unknown_type, "some_data"}}));

  EXPECT_FALSE(factory_called);
  TransportSessionRegistry* registry = channel_->GetSessionRegistry();
  EXPECT_NE(registry->GetSession("s1"), nullptr);
}

TEST_F(TransportChannelImplTest,
       DownstreamConnectionStateReturnsDisconnectedByDefault) {
  EXPECT_EQ(channel_->downstream_connection_state(),
            DownstreamConnectionState::kDisconnected);
}

TEST_F(TransportChannelImplTest,
       OnSessionRegisteredUpdatesDownstreamConnectionStateToConnecting) {
  channel_->OnSessionRegistered(nullptr);

  EXPECT_EQ(channel_->downstream_connection_state(),
            DownstreamConnectionState::kConnecting);
}

TEST_F(
    TransportChannelImplTest,
    OnStreamConnectionStateChangeUpdatesDownstreamConnectionStateToConnected) {
  channel_->OnStreamConnectionStateChange(/*connected=*/true);

  EXPECT_EQ(channel_->downstream_connection_state(),
            DownstreamConnectionState::kConnected);
}

TEST_F(
    TransportChannelImplTest,
    OnStreamConnectionStateChangeUpdatesDownstreamConnectionStateToDisconnected) {
  channel_->OnStreamConnectionStateChange(/*connected=*/true);

  channel_->OnStreamConnectionStateChange(/*connected=*/false);

  EXPECT_EQ(channel_->downstream_connection_state(),
            DownstreamConnectionState::kDisconnected);
}

TEST_F(TransportChannelImplTest,
       OnStreamStatusUpdatesDownstreamConnectionStateToDisconnected) {
  channel_->OnStreamConnectionStateChange(/*connected=*/true);

  channel_->OnStreamStatus("status");

  EXPECT_EQ(channel_->downstream_connection_state(),
            DownstreamConnectionState::kDisconnected);
}

}  // namespace
}  // namespace browser_actuator
