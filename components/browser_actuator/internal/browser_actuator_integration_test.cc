// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/browser_actuator/internal/browser_actuator_service_impl.h"
#include "components/browser_actuator/internal/features.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/session_stream_recorder.h"
#include "components/browser_actuator/internal/transport/test_support/fake_proto_stream_server.h"
#include "components/browser_actuator/internal/transport/test_support/wait_for.h"
#include "components/browser_actuator/internal/transport_channel_impl.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/payload_type_mapping.h"
#include "components/browser_actuator/public/transport_channel.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "components/browser_actuator/public/transport_handler_factory.h"
#include "components/browser_actuator/public/transport_handler_factory_registry.h"
#include "components/browser_actuator/public/transport_session.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

ActuatorDownstreamMessage BuildDownstreamControlMessage(
    std::string_view session_id,
    int64_t sequence_number,
    const ControlCommand& command) {
  ActuatorDownstreamMessage msg;
  msg.set_session_id(std::string(session_id));
  msg.set_sequence_number(sequence_number);
  auto* typed = msg.add_typed_payloads();
  typed->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  typed->mutable_proto_payload()->set_type_url(
      std::string(ExpectedTypeUrl(PayloadType::kControl)));
  typed->mutable_proto_payload()->set_value(command.SerializeAsString());
  return msg;
}

ActuatorDownstreamMessage BuildDownstreamExperimentalTriggeringMessage(
    std::string_view session_id,
    int64_t sequence_number,
    const components_sharing_message::GlicExperimentalTriggering& payload) {
  ActuatorDownstreamMessage msg;
  msg.set_session_id(std::string(session_id));
  msg.set_sequence_number(sequence_number);
  auto* typed = msg.add_typed_payloads();
  typed->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  typed->mutable_proto_payload()->set_type_url(
      std::string(ExpectedTypeUrl(PayloadType::kExperimentalTriggering)));
  typed->mutable_proto_payload()->set_value(payload.SerializeAsString());
  return msg;
}

// A real feature handler that records incoming `kExperimentalTriggering`
// payloads and optionally sends an upstream reply on `session_`.
class EchoExperimentalTriggeringHandler : public TransportHandler {
 public:
  EchoExperimentalTriggeringHandler(
      TransportSession* session,
      std::vector<components_sharing_message::GlicExperimentalTriggering>*
          received_messages,
      bool send_upstream_reply)
      : session_(session),
        received_messages_(received_messages),
        send_upstream_reply_(send_upstream_reply) {}

  void OnMessage(PayloadType payload_type,
                 std::string_view serialized_payload) override {
    EXPECT_EQ(payload_type, PayloadType::kExperimentalTriggering);
    components_sharing_message::GlicExperimentalTriggering parsed;
    ASSERT_TRUE(parsed.ParseFromArray(serialized_payload.data(),
                                      serialized_payload.size()));
    received_messages_->push_back(parsed);

    if (send_upstream_reply_) {
      components_sharing_message::GlicExperimentalTriggering reply;
      reply.set_context_id("ack:" + parsed.context_id());
      EXPECT_TRUE(
          session_
              ->SendUpstreamMessage(PayloadType::kExperimentalTriggering, reply)
              .has_value());
    }
  }

 private:
  const raw_ptr<TransportSession> session_;
  const raw_ptr<
      std::vector<components_sharing_message::GlicExperimentalTriggering>>
      received_messages_;
  const bool send_upstream_reply_;
};

// TODO(crbug.com/566363773): Add an integration test for the concrete
// BrowserActuator action payload handler once the wire payload type is
// defined in ActuatorDownstreamPayloadType.
class EchoExperimentalTriggeringFactory : public TransportHandlerFactory {
 public:
  explicit EchoExperimentalTriggeringFactory(
      std::vector<components_sharing_message::GlicExperimentalTriggering>*
          received_messages,
      bool send_upstream_reply = true)
      : received_messages_(received_messages),
        send_upstream_reply_(send_upstream_reply) {}

  FactoryId GetFactoryId() const override {
    return FactoryId::kExperimentalTriggering;
  }

  std::vector<PayloadType> GetSupportedPayloadTypes() const override {
    return {PayloadType::kExperimentalTriggering};
  }

  std::unique_ptr<TransportHandler> OnNewSession(
      TransportSession* session) override {
    return std::make_unique<EchoExperimentalTriggeringHandler>(
        session, received_messages_, send_upstream_reply_);
  }

 private:
  const raw_ptr<
      std::vector<components_sharing_message::GlicExperimentalTriggering>>
      received_messages_;
  const bool send_upstream_reply_;
};

// Component-level integration tests exercising the full
// `BrowserActuatorServiceImpl` stack (`ProtoStreamClient`, `RustStreamFramer`,
// `UpstreamMessageClient`, `OAuthTokenGetterImpl`, `TransportChannelImpl`,
// `TransportSessionRegistryImpl`, `TransportSessionImpl`, and
// `ControlTransportHandler`) against `FakeProtoStreamServer` and
// `TestURLLoaderFactory`.
class BrowserActuatorIntegrationTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kBrowserActuator, kBrowserActuatorChannelEnabled,
                              kBrowserActuatorInternals},
        /*disabled_features=*/{});

    identity_test_env_.MakePrimaryAccountAvailable(
        "user@example.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);

    service_ = std::make_unique<BrowserActuatorServiceImpl>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_.identity_manager(),
        /*extra_factories=*/
        std::vector<std::unique_ptr<TransportHandlerFactory>>{});
  }

  // Waits for an upstream HTTP POST request, parses its protobuf body into
  // `SendSessionMessageRequest`, and completes the request with HTTP 200 OK.
  SendSessionMessageRequest WaitForUpstreamRequest() {
    const GURL upstream_url = GetSendSessionMessageEndpoint();
    SendSessionMessageRequest parsed;
    bool received = WaitFor([&]() {
      auto* pending_requests = test_url_loader_factory_.pending_requests();
      for (auto it = pending_requests->begin(); it != pending_requests->end();
           ++it) {
        if (it->request.url == upstream_url) {
          EXPECT_TRUE(
              parsed.ParseFromString(network::GetUploadData(it->request)));

          SendSessionMessageResponse response;
          test_url_loader_factory_.SimulateResponseForPendingRequest(
              upstream_url.spec(), response.SerializeAsString(), net::HTTP_OK,
              network::TestURLLoaderFactory::kUrlMatchPrefix);
          return true;
        }
      }
      return false;
    });
    EXPECT_TRUE(received) << "Timed out waiting for upstream POST to "
                          << upstream_url;
    return parsed;
  }

  void TearDown() override {
    service_.reset();
    received_messages_.clear();
  }

  TransportChannelImpl* channel_impl() {
    return static_cast<TransportChannelImpl*>(service_->GetChannel());
  }

  std::vector<components_sharing_message::GlicExperimentalTriggering>
      received_messages_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<BrowserActuatorServiceImpl> service_;
};

TEST_F(BrowserActuatorIntegrationTest,
       DownstreamStartSessionTriggersUpstreamStartSessionAck) {
  FakeProtoStreamServer stream_server(&test_url_loader_factory_,
                                      GetWatchSessionsEndPoint());

  TransportSession* session =
      service_->GetOrCreateSession("session-handshake-1");
  ASSERT_NE(session, nullptr);

  stream_server.WaitForConnection();
  ASSERT_EQ(stream_server.last_watch_request().sessions_size(), 1);
  EXPECT_EQ(stream_server.last_watch_request().sessions(0).session_id(),
            "session-handshake-1");
  EXPECT_EQ(stream_server.last_watch_request()
                .sessions(0)
                .last_seen_sequence_number(),
            0);

  // Push a downstream StartSession ControlCommand on the stream.
  ControlCommand start_cmd;
  start_cmd.mutable_start_session();
  stream_server.PushMessage(BuildDownstreamControlMessage(
      "session-handshake-1", /*sequence_number=*/1, start_cmd));

  // ControlTransportHandler must unpack StartSession and reply with an upstream
  // StartSessionAck via UpstreamMessageClient.
  SendSessionMessageRequest upstream_req = WaitForUpstreamRequest();
  const ActuatorUpstreamMessage& upstream_msg =
      upstream_req.actuator_upstream_message();
  EXPECT_EQ(upstream_msg.session_id(), "session-handshake-1");
  EXPECT_EQ(upstream_msg.client_sequence_number(), 1);
  EXPECT_EQ(upstream_msg.responding_to_sequence_number(), 1);
  ASSERT_EQ(upstream_msg.typed_payloads_size(), 1);
  EXPECT_EQ(upstream_msg.typed_payloads(0).payload_type(),
            ACTUATOR_UPSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  EXPECT_EQ(upstream_msg.typed_payloads(0).proto_payload().type_url(),
            ExpectedTypeUrl(PayloadType::kControl));

  ControlCommand ack_cmd;
  ASSERT_TRUE(ack_cmd.ParseFromString(
      upstream_msg.typed_payloads(0).proto_payload().value()));
  EXPECT_TRUE(ack_cmd.has_start_session_ack());

  // Verify chrome://browser-actuator-internals SessionStreamRecorderFactory
  // records both the downstream StartSession message (via TransportHandler)
  // and the upstream StartSessionAck message (via TransportMessageObserver).
  auto* recorder_factory = static_cast<SessionStreamRecorderFactory*>(
      service_->GetFactory(FactoryId::kSessionStreamRecorder));
  ASSERT_NE(recorder_factory, nullptr);
  base::DictValue dump = recorder_factory->ExportAllSessionsAsValue();
  const base::ListValue* sessions = dump.FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(), 1u);
  const base::DictValue* session_dict = (*sessions)[0].GetIfDict();
  ASSERT_NE(session_dict, nullptr);
  EXPECT_EQ(*session_dict->FindString("session_id"), "session-handshake-1");
  EXPECT_EQ(session_dict->FindBool("is_active"), true);
  EXPECT_EQ(session_dict->FindInt("total_downstream_messages"), 1);
  EXPECT_EQ(session_dict->FindInt("total_upstream_messages"), 1);
}

TEST_F(
    BrowserActuatorIntegrationTest,
    DownstreamExperimentalTriggeringReachesFeatureHandlerAndCorrelatesUpstreamReply) {
  EchoExperimentalTriggeringFactory feature_factory(
      &received_messages_,
      /*send_upstream_reply=*/true);
  service_->GetChannel()->GetHandlerFactoryRegistry()->RegisterFactory(
      &feature_factory);
  base::ScopedClosureRunner unregister_runner(base::BindOnce(
      &TransportHandlerFactoryRegistry::UnregisterFactory,
      base::Unretained(service_->GetChannel()->GetHandlerFactoryRegistry()),
      &feature_factory));

  FakeProtoStreamServer stream_server(&test_url_loader_factory_,
                                      GetWatchSessionsEndPoint());

  TransportSession* session = service_->GetOrCreateSession("session-feature-1");
  ASSERT_NE(session, nullptr);
  stream_server.WaitForConnection();

  components_sharing_message::GlicExperimentalTriggering downstream_payload;
  downstream_payload.set_context_id("navigate:https://example.com");
  stream_server.PushMessage(BuildDownstreamExperimentalTriggeringMessage(
      "session-feature-1", /*sequence_number=*/7, downstream_payload));

  SendSessionMessageRequest upstream_req = WaitForUpstreamRequest();
  ASSERT_EQ(received_messages_.size(), 1u);
  EXPECT_EQ(received_messages_[0].context_id(), "navigate:https://example.com");

  const ActuatorUpstreamMessage& upstream_msg =
      upstream_req.actuator_upstream_message();
  EXPECT_EQ(upstream_msg.session_id(), "session-feature-1");
  EXPECT_EQ(upstream_msg.client_sequence_number(), 1);
  EXPECT_EQ(upstream_msg.responding_to_sequence_number(), 7);
  ASSERT_EQ(upstream_msg.typed_payloads_size(), 1);
  EXPECT_EQ(upstream_msg.typed_payloads(0).payload_type(),
            ACTUATOR_UPSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  EXPECT_EQ(upstream_msg.typed_payloads(0).proto_payload().type_url(),
            ExpectedTypeUrl(PayloadType::kExperimentalTriggering));

  components_sharing_message::GlicExperimentalTriggering upstream_reply;
  ASSERT_TRUE(upstream_reply.ParseFromString(
      upstream_msg.typed_payloads(0).proto_payload().value()));
  EXPECT_EQ(upstream_reply.context_id(), "ack:navigate:https://example.com");
}

TEST_F(BrowserActuatorIntegrationTest, DownstreamCloseSessionDestroysSession) {
  FakeProtoStreamServer stream_server(&test_url_loader_factory_,
                                      GetWatchSessionsEndPoint());

  ASSERT_NE(service_->GetOrCreateSession("session-to-close"), nullptr);
  stream_server.WaitForConnection();

  ControlCommand close_cmd;
  close_cmd.mutable_close_session();
  stream_server.PushMessage(BuildDownstreamControlMessage(
      "session-to-close", /*sequence_number=*/2, close_cmd));

  ASSERT_TRUE(WaitFor(
      [&]() { return service_->GetSession("session-to-close") == nullptr; }));

  auto* recorder_factory = static_cast<SessionStreamRecorderFactory*>(
      service_->GetFactory(FactoryId::kSessionStreamRecorder));
  ASSERT_NE(recorder_factory, nullptr);
  base::DictValue dump = recorder_factory->ExportAllSessionsAsValue();
  const base::ListValue* sessions = dump.FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(), 1u);
  const base::DictValue* session_dict = (*sessions)[0].GetIfDict();
  ASSERT_NE(session_dict, nullptr);
  EXPECT_EQ(*session_dict->FindString("session_id"), "session-to-close");
  EXPECT_EQ(session_dict->FindBool("is_active"), false);
}

TEST_F(BrowserActuatorIntegrationTest,
       DownstreamCloseChannelDisconnectsStream) {
  FakeProtoStreamServer stream_server(&test_url_loader_factory_,
                                      GetWatchSessionsEndPoint());

  ASSERT_NE(service_->GetOrCreateSession("session-channel-close"), nullptr);
  stream_server.WaitForConnection();
  ASSERT_TRUE(WaitFor([&]() {
    return channel_impl()->downstream_connection_state() ==
           DownstreamConnectionState::kConnected;
  }));

  ControlCommand close_channel_cmd;
  close_channel_cmd.mutable_close_channel();
  stream_server.PushMessage(BuildDownstreamControlMessage(
      "session-channel-close", /*sequence_number=*/3, close_channel_cmd));

  ASSERT_TRUE(WaitFor([&]() {
    return channel_impl()->downstream_connection_state() ==
           DownstreamConnectionState::kDisconnected;
  }));
}

TEST_F(BrowserActuatorIntegrationTest,
       KeepAliveNoopIgnoredAndReconnectResumesFromLastSeenSequenceNumber) {
  EchoExperimentalTriggeringFactory feature_factory(
      &received_messages_, /*send_upstream_reply=*/false);
  service_->GetChannel()->GetHandlerFactoryRegistry()->RegisterFactory(
      &feature_factory);
  base::ScopedClosureRunner unregister_runner(base::BindOnce(
      &TransportHandlerFactoryRegistry::UnregisterFactory,
      base::Unretained(service_->GetChannel()->GetHandlerFactoryRegistry()),
      &feature_factory));

  FakeProtoStreamServer stream_server(&test_url_loader_factory_,
                                      GetWatchSessionsEndPoint());

  ASSERT_NE(service_->GetOrCreateSession("session-resume"), nullptr);
  stream_server.WaitForConnection();
  EXPECT_EQ(stream_server.connection_count(), 1);
  EXPECT_EQ(stream_server.last_watch_request()
                .sessions(0)
                .last_seen_sequence_number(),
            0);

  // 1. Push a StreamBody noop keep-alive followed by sequence_number = 42.
  stream_server.PushKeepAlive("keepalive-1");
  components_sharing_message::GlicExperimentalTriggering payload_42;
  payload_42.set_context_id("step-42");
  stream_server.PushMessage(BuildDownstreamExperimentalTriggeringMessage(
      "session-resume", /*sequence_number=*/42, payload_42));
  ASSERT_TRUE(WaitFor([&]() { return received_messages_.size() == 1u; }));
  EXPECT_EQ(received_messages_[0].context_id(), "step-42");

  // 2. Abort the stream with a network reset and advance mock time by the 3s
  // base reconnection delay so ProtoStreamClient reconnects with the updated
  // resume position.
  stream_server.AbortWithNetworkError(net::ERR_CONNECTION_RESET);
  task_environment_.FastForwardBy(base::Seconds(3));
  stream_server.WaitForConnection();

  EXPECT_EQ(stream_server.connection_count(), 2);
  ASSERT_EQ(stream_server.last_watch_request().sessions_size(), 1);
  EXPECT_EQ(stream_server.last_watch_request().sessions(0).session_id(),
            "session-resume");
  EXPECT_EQ(stream_server.last_watch_request()
                .sessions(0)
                .last_seen_sequence_number(),
            42);
}

TEST_F(BrowserActuatorIntegrationTest,
       StreamStatusTrailersCleanlyFinishWithoutReconnecting) {
  FakeProtoStreamServer stream_server(&test_url_loader_factory_,
                                      GetWatchSessionsEndPoint());

  ASSERT_NE(service_->GetOrCreateSession("session-status-finish"), nullptr);
  stream_server.WaitForConnection();
  ASSERT_TRUE(WaitFor([&]() {
    return channel_impl()->downstream_connection_state() ==
           DownstreamConnectionState::kConnected;
  }));

  stream_server.PushTrailersAndFinish(/*rpc_status_code=*/0, "OK");
  ASSERT_TRUE(WaitFor([&]() {
    return channel_impl()->downstream_connection_state() ==
           DownstreamConnectionState::kDisconnected;
  }));
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_EQ(channel_impl()->downstream_connection_state(),
            DownstreamConnectionState::kDisconnected);
  EXPECT_EQ(stream_server.connection_count(), 1);
}

TEST_F(
    BrowserActuatorIntegrationTest,
    DownstreamUnknownPayloadTypeDroppedGracefullyAndSubsequentPayloadDelivered) {
  EchoExperimentalTriggeringFactory feature_factory(
      &received_messages_, /*send_upstream_reply=*/false);
  service_->GetChannel()->GetHandlerFactoryRegistry()->RegisterFactory(
      &feature_factory);
  base::ScopedClosureRunner unregister_runner(base::BindOnce(
      &TransportHandlerFactoryRegistry::UnregisterFactory,
      base::Unretained(service_->GetChannel()->GetHandlerFactoryRegistry()),
      &feature_factory));

  FakeProtoStreamServer stream_server(&test_url_loader_factory_,
                                      GetWatchSessionsEndPoint());

  ASSERT_NE(service_->GetOrCreateSession("session-unknown-type"), nullptr);
  stream_server.WaitForConnection();

  // 1. Push a downstream message carrying an unknown / future payload type
  // (such as a future BrowserActuator wire payload before proto enum
  // assignment).
  ActuatorDownstreamMessage unknown_msg;
  unknown_msg.set_session_id("session-unknown-type");
  unknown_msg.set_sequence_number(1);
  auto* typed = unknown_msg.add_typed_payloads();
  typed->set_payload_type(static_cast<ActuatorDownstreamPayloadType>(999));
  typed->mutable_proto_payload()->set_type_url(
      "type.googleapis.com/testing.UnknownFuturePayload");
  typed->mutable_proto_payload()->set_value("raw_payload");
  stream_server.PushMessage(unknown_msg);

  // 2. Push a subsequent valid experimental triggering message.
  components_sharing_message::GlicExperimentalTriggering downstream_payload;
  downstream_payload.set_context_id("step-after-unknown");
  stream_server.PushMessage(BuildDownstreamExperimentalTriggeringMessage(
      "session-unknown-type", /*sequence_number=*/2, downstream_payload));

  ASSERT_TRUE(WaitFor([&]() { return received_messages_.size() == 1u; }));
  EXPECT_EQ(received_messages_[0].context_id(), "step-after-unknown");
  EXPECT_NE(service_->GetSession("session-unknown-type"), nullptr);
}

}  // namespace
}  // namespace browser_actuator
