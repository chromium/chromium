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
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/browser_actuator/internal/control_transport_handler.h"
#include "components/browser_actuator/internal/features.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport/message_stream_client.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "components/browser_actuator/internal/transport/test_support/wait_for.h"
#include "components/browser_actuator/internal/transport/upstream_message_client/upstream_message_client.h"
#include "components/browser_actuator/internal/transport_session_impl.h"
#include "components/browser_actuator/internal/transport_session_registry_impl.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "components/browser_actuator/public/transport_handler_factory.h"
#include "components/browser_actuator/public/transport_handler_factory_registry.h"
#include "components/browser_actuator/test_support/mock_transport_handler.h"
#include "components/browser_actuator/test_support/mock_transport_handler_factory.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
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

class TransportChannelImplTestBase : public testing::Test {
 protected:
  explicit TransportChannelImplTestBase(
      base::test::TaskEnvironment::TimeSource time_source)
      : task_environment_(time_source),
        identity_test_env_(&test_url_loader_factory_) {
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    identity_test_env_.MakePrimaryAccountAvailable(
        "test@gmail.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);

    auto client = std::make_unique<FakeMessageStreamClient>();
    fake_client_ = client->GetWeakPtr();
    channel_ = std::make_unique<TransportChannelImpl>(
        std::make_unique<UpstreamMessageClient>(
            test_shared_url_loader_factory_,
            identity_test_env_.identity_manager(),
            GetSendSessionMessageEndpoint(), TRAFFIC_ANNOTATION_FOR_TESTS),
        base::BindOnce(
            [](std::unique_ptr<FakeMessageStreamClient> owned_client,
               std::unique_ptr<StreamConnectionDelegate>)
                -> std::unique_ptr<MessageStreamClient> {
              // The resume delegate isn't exercised here; the test drives the
              // body builder directly.
              return std::move(owned_client);
            },
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

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::unique_ptr<TransportChannelImpl> channel_;
  base::WeakPtr<FakeMessageStreamClient> fake_client_;
};

class TransportChannelImplTest : public TransportChannelImplTestBase {
 protected:
  TransportChannelImplTest()
      : TransportChannelImplTestBase(
            base::test::TaskEnvironment::TimeSource::DEFAULT) {}
};

class TransportChannelImplTestWithMockTime
    : public TransportChannelImplTestBase {
 protected:
  TransportChannelImplTestWithMockTime()
      : TransportChannelImplTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
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

TEST_F(TransportChannelImplTest, ConnectsWhenNewSessionRegisteredAndConnected) {
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
    typed->mutable_proto_payload()->set_value(payload.value.data(),
                                              payload.value.size());
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
TEST_F(TransportChannelImplTest, SendUpstreamMessage) {
  fake_client_->Dispatch(SerializedDownstream("s1", 1));

  ControlCommand command;
  command.mutable_close_channel();
  channel_->SendUpstreamMessage("s1", PayloadType::kControl, command);

  ASSERT_TRUE(
      WaitFor([&]() { return test_url_loader_factory_.NumPending() == 1; }));
  const network::TestURLLoaderFactory::PendingRequest* pending =
      test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_NE(pending, nullptr);

  EXPECT_EQ(pending->request.url, GetSendSessionMessageEndpoint());
  EXPECT_EQ(pending->request.method, "POST");

  std::optional<std::string> content_type =
      pending->request.headers.GetHeader(net::HttpRequestHeaders::kContentType);
  ASSERT_TRUE(content_type.has_value());
  EXPECT_EQ(*content_type, "application/x-protobuf");

  std::string upload_body = network::GetUploadData(pending->request);
  SendSessionMessageRequest request;
  ASSERT_TRUE(request.ParseFromString(upload_body));
  EXPECT_FALSE(request.request_id().empty());

  const ActuatorUpstreamMessage& upstream = request.actuator_upstream_message();
  EXPECT_EQ(upstream.session_id(), "s1");
  EXPECT_EQ(upstream.client_sequence_number(), 1);
  EXPECT_EQ(upstream.responding_to_sequence_number(), 1);

  ASSERT_EQ(upstream.typed_payloads_size(), 1);
  EXPECT_EQ(upstream.typed_payloads(0).payload_type(),
            ACTUATOR_UPSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  EXPECT_EQ(upstream.typed_payloads(0).proto_payload().value(),
            command.SerializeAsString());
  EXPECT_EQ(upstream.typed_payloads(0).proto_payload().type_url(),
            "type.googleapis.com/browser_actuator.ControlCommand");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending->request.url.spec(), /*content=*/"", net::HTTP_OK);

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(TransportChannelImplTest, SendMultipleUpstreamMessagesConcurrently) {
  fake_client_->Dispatch(SerializedDownstream("s1", 1));

  ControlCommand command1;
  command1.mutable_close_channel();
  ControlCommand command2;
  command2.mutable_close_session();

  channel_->SendUpstreamMessage("s1", PayloadType::kControl, command1);
  channel_->SendUpstreamMessage("s1", PayloadType::kControl, command2);

  ASSERT_TRUE(
      WaitFor([&]() { return test_url_loader_factory_.NumPending() == 2; }));

  const network::TestURLLoaderFactory::PendingRequest* pending1 =
      test_url_loader_factory_.GetPendingRequest(0);
  const network::TestURLLoaderFactory::PendingRequest* pending2 =
      test_url_loader_factory_.GetPendingRequest(1);

  ASSERT_NE(pending1, nullptr);
  ASSERT_NE(pending2, nullptr);

  SendSessionMessageRequest request1;
  ASSERT_TRUE(
      request1.ParseFromString(network::GetUploadData(pending1->request)));
  const ActuatorUpstreamMessage& upstream1 =
      request1.actuator_upstream_message();
  EXPECT_EQ(upstream1.client_sequence_number(), 1);
  EXPECT_EQ(upstream1.responding_to_sequence_number(), 1);
  EXPECT_EQ(upstream1.typed_payloads(0).proto_payload().value(),
            command1.SerializeAsString());

  SendSessionMessageRequest request2;
  ASSERT_TRUE(
      request2.ParseFromString(network::GetUploadData(pending2->request)));
  const ActuatorUpstreamMessage& upstream2 =
      request2.actuator_upstream_message();
  EXPECT_EQ(upstream2.client_sequence_number(), 2);
  EXPECT_EQ(upstream2.responding_to_sequence_number(), 1);
  EXPECT_EQ(upstream2.typed_payloads(0).proto_payload().value(),
            command2.SerializeAsString());

  std::string request_url = pending1->request.url.spec();

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      request_url, /*content=*/"", net::HTTP_OK);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      request_url, /*content=*/"", net::HTTP_OK);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(TransportChannelImplTest, SendUpstreamMessagesAcrossMultipleSessions) {
  fake_client_->Dispatch(SerializedDownstream("s1", 1));
  fake_client_->Dispatch(SerializedDownstream("s2", 3));

  ControlCommand command_s1;
  command_s1.mutable_close_channel();
  ControlCommand command_s2;
  command_s2.mutable_close_session();

  channel_->SendUpstreamMessage("s1", PayloadType::kControl, command_s1);
  channel_->SendUpstreamMessage("s2", PayloadType::kControl, command_s2);

  ASSERT_TRUE(
      WaitFor([&]() { return test_url_loader_factory_.NumPending() == 2; }));

  const network::TestURLLoaderFactory::PendingRequest* pending1 =
      test_url_loader_factory_.GetPendingRequest(0);
  const network::TestURLLoaderFactory::PendingRequest* pending2 =
      test_url_loader_factory_.GetPendingRequest(1);

  ASSERT_NE(pending1, nullptr);
  ASSERT_NE(pending2, nullptr);

  SendSessionMessageRequest request1;
  ASSERT_TRUE(
      request1.ParseFromString(network::GetUploadData(pending1->request)));
  const ActuatorUpstreamMessage& upstream1 =
      request1.actuator_upstream_message();
  EXPECT_EQ(upstream1.session_id(), "s1");
  EXPECT_EQ(upstream1.client_sequence_number(), 1);
  EXPECT_EQ(upstream1.responding_to_sequence_number(), 1);

  SendSessionMessageRequest request2;
  ASSERT_TRUE(
      request2.ParseFromString(network::GetUploadData(pending2->request)));
  const ActuatorUpstreamMessage& upstream2 =
      request2.actuator_upstream_message();
  EXPECT_EQ(upstream2.session_id(), "s2");
  EXPECT_EQ(upstream2.client_sequence_number(), 1);
  EXPECT_EQ(upstream2.responding_to_sequence_number(), 3);

  std::string request_url = pending1->request.url.spec();
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      request_url, /*content=*/"", net::HTTP_OK);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      request_url, /*content=*/"", net::HTTP_OK);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(TransportChannelImplTestWithMockTime, SendUpstreamMessageHTTPError) {
  fake_client_->Dispatch(SerializedDownstream("s1", 1));
  ControlCommand command;
  command.mutable_close_channel();
  channel_->SendUpstreamMessage("s1", PayloadType::kControl, command);

  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(
        WaitFor([&]() { return test_url_loader_factory_.NumPending() == 1; }));
    const network::TestURLLoaderFactory::PendingRequest* pending =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_NE(pending, nullptr);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        pending->request.url.spec(), /*content=*/"",
        net::HTTP_INTERNAL_SERVER_ERROR);
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

}  // namespace
}  // namespace browser_actuator
