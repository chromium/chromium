// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/remote_model_execution_session_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/streaming_client/streaming_websocket_client.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace optimization_guide {

using base::test::TestMessage;
using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

namespace {

TestMessage BuildTestMessage(const std::string& text) {
  TestMessage msg;
  msg.set_test(text);
  return msg;
}

proto::ExecuteResponse BuildTestExecuteResponse(
    const TestMessage& message,
    const std::string& execution_id = "") {
  proto::ExecuteResponse response;
  if (!execution_id.empty()) {
    response.set_server_execution_id(execution_id);
  }
  *response.mutable_response_metadata() = AnyWrapProto(message);
  return response;
}

proto::ExecuteResponse BuildErrorExecuteResponse(
    proto::ErrorState error_state) {
  proto::ExecuteResponse response;
  response.mutable_error_response()->set_error_state(error_state);
  return response;
}

class FakeStreamingWebSocketClient
    : public streaming_client::StreamingWebSocketClient {
 public:
  explicit FakeStreamingWebSocketClient(Delegate* delegate = nullptr)
      : StreamingWebSocketClient(GURL(),
                                 /*network_context=*/nullptr,
                                 MISSING_TRAFFIC_ANNOTATION,
                                 delegate) {}
  ~FakeStreamingWebSocketClient() override = default;

  void Connect() override {
    connect_called = true;
    if (on_connect) {
      on_connect.Run();
    }
  }

  void Send(std::vector<uint8_t> request) override {
    sent_requests.push_back(std::move(request));
    if (on_send) {
      on_send.Run();
    }
  }

  void Close() override {
    close_called = true;
    if (on_close) {
      on_close.Run();
    }
  }

  void SimulateConnected() {
    CHECK(delegate_for_testing());
    delegate_for_testing()->OnConnected();
  }

  void SimulateMessage(const proto::ExecuteResponse& response) {
    CHECK(delegate_for_testing());
    std::string serialized;
    ASSERT_TRUE(response.SerializeToString(&serialized));
    delegate_for_testing()->OnMessage(
        std::vector<uint8_t>(serialized.begin(), serialized.end()));
  }

  void SimulateRawMessage(const std::vector<uint8_t>& message) {
    CHECK(delegate_for_testing());
    delegate_for_testing()->OnMessage(message);
  }

  void SimulateConnectionError(const std::string& message,
                               int net_error,
                               int response_code) {
    CHECK(delegate_for_testing());
    delegate_for_testing()->OnConnectionError(message, net_error,
                                              response_code);
  }

  void SimulateDropChannel(bool was_clean,
                           uint16_t code = 1000,
                           const std::string& reason = "Closed") {
    CHECK(delegate_for_testing());
    delegate_for_testing()->OnDropChannel(was_clean, code, reason,
                                          /*elapsed=*/std::nullopt);
  }

  std::vector<network::mojom::HttpHeaderPtr> GetAdditionalHeaders() {
    CHECK(delegate_for_testing());
    return delegate_for_testing()->GetAdditionalHeaders();
  }

  std::optional<std::string> GetHeader(std::string_view name) {
    for (const network::mojom::HttpHeaderPtr& header : GetAdditionalHeaders()) {
      if (header->name == name) {
        return header->value;
      }
    }
    return std::nullopt;
  }

  bool connect_called = false;
  bool close_called = false;
  std::vector<std::vector<uint8_t>> sent_requests;
  base::RepeatingClosure on_connect;
  base::RepeatingClosure on_send;
  base::RepeatingClosure on_close;
};

class TestObserver : public RemoteModelExecutionSession::Observer {
 public:
  void OnConnectionStateChanged(
      RemoteModelExecutionSession::ConnectionState state) override {
    states.push_back(state);
    if (target_state.has_value() && state == *target_state && on_target_state) {
      on_target_state.Run();
    }
  }

  std::vector<RemoteModelExecutionSession::ConnectionState> states;
  std::optional<RemoteModelExecutionSession::ConnectionState> target_state;
  base::RepeatingClosure on_target_state;
};

}  // namespace

class RemoteModelExecutionSessionImplTest : public testing::Test {
 public:
  RemoteModelExecutionSessionImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~RemoteModelExecutionSessionImplTest() override = default;

  void TearDown() override {
    fake_client_ = nullptr;
    session_.reset();
    response_future_.Clear();
  }

  RemoteModelExecutionSessionImpl* CreateSession(
      ModelBasedCapabilityKey feature = ModelBasedCapabilityKey::kScamDetection,
      const StreamingModelExecutionOptions& options = {}) {
    auto fake_client = std::make_unique<FakeStreamingWebSocketClient>();
    fake_client_ = fake_client.get();
    session_ = std::make_unique<RemoteModelExecutionSessionImpl>(
        feature, options, response_future_.GetRepeatingCallback(),
        identity_test_env_.identity_manager(), std::move(fake_client),
        /*logger=*/nullptr);
    return session_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::TestFuture<OptimizationGuideModelStreamingResult>
      response_future_;
  raw_ptr<FakeStreamingWebSocketClient> fake_client_ = nullptr;
  std::unique_ptr<RemoteModelExecutionSessionImpl> session_;
};

TEST_F(RemoteModelExecutionSessionImplTest, ConnectsImmediatelyWithPrewarm) {
  StreamingModelExecutionOptions options;
  options.prewarm_connection = true;

  RemoteModelExecutionSessionImpl* session =
      CreateSession(ModelBasedCapabilityKey::kScamDetection, options);
  TestObserver observer;
  session->AddObserver(&observer);

  EXPECT_TRUE(fake_client_->connect_called);
  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kConnecting);
  EXPECT_EQ(fake_client_->delegate_for_testing(), session);

  fake_client_->SimulateConnected();

  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kConnected);
  EXPECT_THAT(observer.states,
              testing::ElementsAre(
                  RemoteModelExecutionSession::ConnectionState::kConnected));
}

TEST_F(RemoteModelExecutionSessionImplTest, SendConnectsAndSends) {
  RemoteModelExecutionSessionImpl* session =
      CreateSession(ModelBasedCapabilityKey::kScamDetection);
  TestObserver observer;
  session->AddObserver(&observer);

  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kDisconnected);

  session->Send(BuildTestMessage("input message"));
  EXPECT_TRUE(fake_client_->connect_called);
  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kConnecting);

  fake_client_->SimulateConnected();

  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kConnected);

  ASSERT_EQ(fake_client_->sent_requests.size(), 1u);
  proto::ExecuteRequest received_request;
  ASSERT_TRUE(
      received_request.ParseFromArray(fake_client_->sent_requests[0].data(),
                                      fake_client_->sent_requests[0].size()));
  EXPECT_EQ(
      received_request.feature(),
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION);
  ASSERT_OK_AND_ASSIGN(
      TestMessage parsed_msg,
      ParsedAnyMetadata<TestMessage>(received_request.request_metadata()));
  EXPECT_EQ(parsed_msg.test(), "input message");
}

TEST_F(RemoteModelExecutionSessionImplTest,
       MultipleQueuedRequestsFlushedInOrder) {
  auto* session = CreateSession(ModelBasedCapabilityKey::kScamDetection);

  session->Send(BuildTestMessage("request 1"));
  session->Send(BuildTestMessage("request 2"));
  EXPECT_EQ(fake_client_->sent_requests.size(), 0u);

  fake_client_->SimulateConnected();
  ASSERT_EQ(fake_client_->sent_requests.size(), 2u);

  proto::ExecuteRequest received_request_1;
  ASSERT_TRUE(
      received_request_1.ParseFromArray(fake_client_->sent_requests[0].data(),
                                        fake_client_->sent_requests[0].size()));
  ASSERT_OK_AND_ASSIGN(
      TestMessage parsed_msg_1,
      ParsedAnyMetadata<TestMessage>(received_request_1.request_metadata()));
  EXPECT_EQ(parsed_msg_1.test(), "request 1");

  proto::ExecuteRequest received_request_2;
  ASSERT_TRUE(
      received_request_2.ParseFromArray(fake_client_->sent_requests[1].data(),
                                        fake_client_->sent_requests[1].size()));
  ASSERT_OK_AND_ASSIGN(
      TestMessage parsed_msg_2,
      ParsedAnyMetadata<TestMessage>(received_request_2.request_metadata()));
  EXPECT_EQ(parsed_msg_2.test(), "request 2");
}

TEST_F(RemoteModelExecutionSessionImplTest,
       SendPendingRequestsSynchronousFailureDoesNotCrash) {
  auto* session = CreateSession(ModelBasedCapabilityKey::kScamDetection);

  session->Send(BuildTestMessage("request 1"));
  session->Send(BuildTestMessage("request 2"));

  fake_client_->on_send = base::BindRepeating(
      [](FakeStreamingWebSocketClient* client) {
        client->delegate_for_testing()->OnError("Write failed");
      },
      base::Unretained(fake_client_.get()));

  fake_client_->SimulateConnected();

  EXPECT_EQ(fake_client_->sent_requests.size(), 1u);
  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kDisconnected);
  OptimizationGuideModelStreamingResult result = response_future_.Take();
  EXPECT_THAT(result.response, base::test::ErrorIs(testing::Property(
                                   &OptimizationGuideModelExecutionError::error,
                                   ModelExecutionError::kGenericFailure)));
  ASSERT_TRUE(result.execution_info);
  EXPECT_EQ(result.execution_info->model_execution_error_enum(),
            static_cast<uint32_t>(ModelExecutionError::kGenericFailure));
}

TEST_F(RemoteModelExecutionSessionImplTest, SuccessfulStreamingResponses) {
  RemoteModelExecutionSessionImpl* session =
      CreateSession(ModelBasedCapabilityKey::kScamDetection);
  TestObserver observer;
  session->AddObserver(&observer);

  session->Send(BuildTestMessage("query"));
  fake_client_->SimulateConnected();
  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kConnected);

  fake_client_->SimulateMessage(
      BuildTestExecuteResponse(BuildTestMessage("chunk 1"), "exec_1"));
  OptimizationGuideModelStreamingResult result1 = response_future_.Take();

  ASSERT_OK_AND_ASSIGN(const proto::Any& response_1, result1.response);
  ASSERT_OK_AND_ASSIGN(TestMessage parsed_1,
                       ParsedAnyMetadata<TestMessage>(response_1));
  EXPECT_EQ(parsed_1.test(), "chunk 1");
  ASSERT_TRUE(result1.execution_info);
  EXPECT_EQ(result1.execution_info->execution_id(), "exec_1");

  fake_client_->SimulateMessage(
      BuildTestExecuteResponse(BuildTestMessage("chunk 2"), "exec_1"));
  OptimizationGuideModelStreamingResult result2 = response_future_.Take();

  ASSERT_OK_AND_ASSIGN(const proto::Any& response_2, result2.response);
  ASSERT_OK_AND_ASSIGN(TestMessage parsed_2,
                       ParsedAnyMetadata<TestMessage>(response_2));
  EXPECT_EQ(parsed_2.test(), "chunk 2");
  ASSERT_TRUE(result2.execution_info);
  EXPECT_EQ(result2.execution_info->execution_id(), "exec_1");
}

TEST_F(RemoteModelExecutionSessionImplTest,
       SuccessfulStreamingResponsesNoExecutionId) {
  RemoteModelExecutionSessionImpl* session =
      CreateSession(ModelBasedCapabilityKey::kScamDetection);
  TestObserver observer;
  session->AddObserver(&observer);

  session->Send(BuildTestMessage("query"));
  fake_client_->SimulateConnected();
  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kConnected);

  fake_client_->SimulateMessage(
      BuildTestExecuteResponse(BuildTestMessage("chunk")));
  OptimizationGuideModelStreamingResult result = response_future_.Take();

  EXPECT_TRUE(result.response.has_value());
  EXPECT_FALSE(result.execution_info);
}

TEST_F(RemoteModelExecutionSessionImplTest, ServerErrorResponse) {
  RemoteModelExecutionSessionImpl* session =
      CreateSession(ModelBasedCapabilityKey::kScamDetection);
  TestObserver observer;
  session->AddObserver(&observer);

  session->Send(BuildTestMessage("query"));
  fake_client_->SimulateConnected();
  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kConnected);

  fake_client_->SimulateMessage(
      BuildErrorExecuteResponse(proto::ERROR_STATE_FILTERED));
  OptimizationGuideModelStreamingResult result = response_future_.Take();

  EXPECT_THAT(result.response, base::test::ErrorIs(testing::Property(
                                   &OptimizationGuideModelExecutionError::error,
                                   ModelExecutionError::kFiltered)));
  ASSERT_TRUE(result.execution_info);
  EXPECT_EQ(result.execution_info->model_execution_error_enum(),
            static_cast<uint32_t>(ModelExecutionError::kFiltered));
}

TEST_F(RemoteModelExecutionSessionImplTest,
       MalformedMessageDispatchesGenericFailure) {
  RemoteModelExecutionSessionImpl* session =
      CreateSession(ModelBasedCapabilityKey::kScamDetection);
  session->Send(BuildTestMessage("request"));
  fake_client_->SimulateConnected();

  const std::vector<uint8_t> corrupted_bytes = {0xFF, 0xFF, 0xFF, 0xFF};
  fake_client_->SimulateRawMessage(corrupted_bytes);
  OptimizationGuideModelStreamingResult result = response_future_.Take();

  EXPECT_THAT(result.response, base::test::ErrorIs(testing::Property(
                                   &OptimizationGuideModelExecutionError::error,
                                   ModelExecutionError::kGenericFailure)));
  ASSERT_TRUE(result.execution_info);
  EXPECT_EQ(result.execution_info->model_execution_error_enum(),
            static_cast<uint32_t>(ModelExecutionError::kGenericFailure));
}

TEST_F(RemoteModelExecutionSessionImplTest, HandshakeFailure) {
  RemoteModelExecutionSessionImpl* session =
      CreateSession(ModelBasedCapabilityKey::kScamDetection);
  session->Send(BuildTestMessage("query"));

  EXPECT_TRUE(fake_client_->connect_called);

  fake_client_->SimulateConnectionError("Unauthorized", net::ERR_FAILED,
                                        net::HTTP_UNAUTHORIZED);
  OptimizationGuideModelStreamingResult result = response_future_.Take();

  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kDisconnected);
  EXPECT_THAT(result.response, base::test::ErrorIs(testing::Property(
                                   &OptimizationGuideModelExecutionError::error,
                                   ModelExecutionError::kPermissionDenied)));
}

TEST_F(RemoteModelExecutionSessionImplTest,
       CleanChannelDropDoesNotDispatchError) {
  RemoteModelExecutionSessionImpl* session =
      CreateSession(ModelBasedCapabilityKey::kScamDetection);
  TestObserver observer;
  session->AddObserver(&observer);

  session->Send(BuildTestMessage("query"));
  fake_client_->SimulateConnected();
  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kConnected);

  fake_client_->SimulateDropChannel(/*was_clean=*/true);

  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kDisconnected);
  EXPECT_EQ(observer.states.back(),
            RemoteModelExecutionSession::ConnectionState::kDisconnected);
  EXPECT_FALSE(response_future_.IsReady());
}

TEST_F(RemoteModelExecutionSessionImplTest,
       UncleanChannelDropDispatchesGenericFailure) {
  RemoteModelExecutionSessionImpl* session =
      CreateSession(ModelBasedCapabilityKey::kScamDetection);
  TestObserver observer;
  session->AddObserver(&observer);

  session->Send(BuildTestMessage("query"));
  fake_client_->SimulateConnected();

  fake_client_->SimulateDropChannel(/*was_clean=*/false);
  OptimizationGuideModelStreamingResult result = response_future_.Take();

  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kDisconnected);
  EXPECT_EQ(observer.states.back(),
            RemoteModelExecutionSession::ConnectionState::kDisconnected);
  EXPECT_THAT(result.response, base::test::ErrorIs(testing::Property(
                                   &OptimizationGuideModelExecutionError::error,
                                   ModelExecutionError::kGenericFailure)));
  ASSERT_TRUE(result.execution_info);
  EXPECT_EQ(result.execution_info->model_execution_error_enum(),
            static_cast<uint32_t>(ModelExecutionError::kGenericFailure));
}

TEST_F(RemoteModelExecutionSessionImplTest,
       IdleTimeoutDisconnectsAndReconnects) {
  StreamingModelExecutionOptions options;
  options.idle_disconnect_timeout = base::Minutes(5);

  RemoteModelExecutionSessionImpl* session =
      CreateSession(ModelBasedCapabilityKey::kScamDetection, options);
  TestObserver observer;
  session->AddObserver(&observer);

  session->Send(BuildTestMessage("query 1"));
  fake_client_->SimulateConnected();
  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kConnected);

  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_TRUE(fake_client_->close_called);
  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kDisconnected);

  // Next Send should trigger reconnect.
  fake_client_->connect_called = false;
  session->Send(BuildTestMessage("query 2"));
  EXPECT_TRUE(fake_client_->connect_called);
  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kConnecting);

  fake_client_->SimulateConnected();
  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kConnected);
}

TEST_F(RemoteModelExecutionSessionImplTest, AccessTokenRequired) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  RemoteModelExecutionSessionImpl* session =
      CreateSession(ModelBasedCapabilityKey::kWallpaperSearch);

  base::test::TestFuture<void> connect_future;
  fake_client_->on_connect = connect_future.GetRepeatingCallback();

  session->Send(BuildTestMessage("request"));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "test_access_token", base::Time::Max());

  EXPECT_TRUE(connect_future.Wait());

  EXPECT_TRUE(fake_client_->connect_called);
  EXPECT_THAT(fake_client_->GetHeader("Authorization"),
              testing::Optional(std::string("Bearer test_access_token")));
}

TEST_F(RemoteModelExecutionSessionImplTest, AccessTokenRequiredButNotSignedIn) {
  RemoteModelExecutionSessionImpl* session =
      CreateSession(ModelBasedCapabilityKey::kWallpaperSearch);
  session->Send(BuildTestMessage("request"));

  OptimizationGuideModelStreamingResult result = response_future_.Take();

  EXPECT_FALSE(fake_client_->connect_called);
  EXPECT_EQ(session->connection_state(),
            RemoteModelExecutionSession::ConnectionState::kDisconnected);

  EXPECT_THAT(result.response, base::test::ErrorIs(testing::Property(
                                   &OptimizationGuideModelExecutionError::error,
                                   ModelExecutionError::kPermissionDenied)));
}

TEST_F(RemoteModelExecutionSessionImplTest,
       DevStreamUrlBypassesAccessTokenRequirement) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kOptimizationGuideServiceModelExecutionStreamURLSwitch,
      "ws://127.0.0.1:8080/v1:StreamExecute");

  auto* session = CreateSession(ModelBasedCapabilityKey::kWallpaperSearch);

  base::test::TestFuture<void> connect_future;
  fake_client_->on_connect = connect_future.GetRepeatingCallback();

  session->Send(BuildTestMessage("request"));
  EXPECT_TRUE(connect_future.Wait());

  EXPECT_TRUE(fake_client_->connect_called);
  EXPECT_THAT(fake_client_->GetHeader("Authorization"),
              testing::Eq(std::nullopt));
}

}  // namespace optimization_guide
