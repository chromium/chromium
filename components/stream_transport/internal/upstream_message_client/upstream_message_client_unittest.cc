// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/internal/upstream_message_client/upstream_message_client.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/stream_transport/internal/features.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"
#include "components/stream_transport/public/common.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace stream_transport {
namespace {

constexpr char kEndpoint[] = "https://example.com/v1/send";

class BrowserActuatorUpstreamMessageClientTest
    : public testing::Test,
      public signin::IdentityManager::DiagnosticsObserver {
 protected:
  BrowserActuatorUpstreamMessageClientTest()
      : identity_test_env_(&test_url_loader_factory_) {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [this](const network::ResourceRequest& request) {
          requests_.push_back(request);
        }));

    identity_test_env_.identity_manager()->AddDiagnosticsObserver(this);
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  ~BrowserActuatorUpstreamMessageClientTest() override {
    identity_test_env_.identity_manager()->RemoveDiagnosticsObserver(this);
  }

  // signin::IdentityManager::DiagnosticsObserver:
  void OnAccessTokenRemovedFromCache(const CoreAccountId& account_id,
                                     const signin::ScopeSet& scopes) override {
    removed_token_account_ids_.push_back(account_id);
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::vector<network::ResourceRequest> requests_;
  std::vector<CoreAccountId> removed_token_account_ids_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(BrowserActuatorUpstreamMessageClientTest, SendMessageSuccess) {
  base::HistogramTester histogram_tester;
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "OK", net::HTTP_OK);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  browser_actuator::ActuatorUpstreamMessage returned_upstream =
      client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                                 /*responding_to_sequence_number=*/std::nullopt,
                                 PayloadType::kControl, command,
                                 future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(net::HTTP_OK, status);

  ASSERT_EQ(1u, requests_.size());
  const auto& request = requests_[0];
  EXPECT_EQ(kEndpoint, request.url.spec());
  EXPECT_EQ("POST", request.method);
  EXPECT_EQ(network::mojom::RedirectMode::kError, request.redirect_mode);
  EXPECT_FALSE(request.headers.HasHeader("X-Developer-Key"));

  browser_actuator::SendSessionMessageRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(network::GetUploadData(request)));

  const auto& upstream = request_proto.actuator_upstream_message();
  EXPECT_EQ("session_1", upstream.session_id());
  EXPECT_EQ(1, upstream.client_sequence_number());
  EXPECT_FALSE(upstream.has_responding_to_sequence_number());
  EXPECT_TRUE(upstream.has_capabilities());
  EXPECT_GT(upstream.capabilities().chrome_major_version_number(), 0);
  EXPECT_EQ(version_info::GetMajorVersionNumberAsInt(),
            upstream.capabilities().chrome_major_version_number());
  EXPECT_FALSE(upstream.capabilities().chrome_version().empty());
  EXPECT_EQ(version_info::GetVersionNumber(),
            upstream.capabilities().chrome_version());
  EXPECT_TRUE(upstream.capabilities().supported_features().empty());
  EXPECT_EQ(returned_upstream.SerializeAsString(),
            upstream.SerializeAsString());

  ASSERT_EQ(1, upstream.typed_payloads_size());
  const auto& typed_payload = upstream.typed_payloads(0);
  EXPECT_EQ(browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
            typed_payload.payload_type());
  EXPECT_EQ("type.googleapis.com/browser_actuator.ControlCommand",
            typed_payload.proto_payload().type_url());
  EXPECT_EQ(command.SerializeAsString(), typed_payload.proto_payload().value());

  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Upstream.PayloadSize.Control", command.ByteSizeLong(),
      1);
  histogram_tester.ExpectTotalCount(
      "StreamTransport.Upstream.UploadLatency.Control", 1);
  histogram_tester.ExpectUniqueSample("StreamTransport.Upstream.HttpStatus",
                                      net::HTTP_OK, 1);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendMessageExperimentalTriggeringSuccess) {
  base::HistogramTester histogram_tester;
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "OK", net::HTTP_OK);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kExperimentalTriggering, command,
                             future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(net::HTTP_OK, status);

  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Upstream.PayloadSize.ExperimentalTriggering",
      command.ByteSizeLong(), 1);
  histogram_tester.ExpectTotalCount(
      "StreamTransport.Upstream.UploadLatency."
      "ExperimentalTriggering",
      1);
  histogram_tester.ExpectUniqueSample("StreamTransport.Upstream.HttpStatus",
                                      net::HTTP_OK, 1);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest, SendMessageProbeSuccess) {
  base::HistogramTester histogram_tester;
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "OK", net::HTTP_OK);

  browser_actuator::TransportProbe probe;
  probe.mutable_response()->set_probe_id("probe_1");

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kProbe, probe, future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(net::HTTP_OK, status);

  ASSERT_EQ(1u, requests_.size());
  browser_actuator::SendSessionMessageRequest request_proto;
  ASSERT_TRUE(
      request_proto.ParseFromString(network::GetUploadData(requests_[0])));

  const auto& upstream = request_proto.actuator_upstream_message();
  ASSERT_EQ(1, upstream.typed_payloads_size());
  const auto& typed_payload = upstream.typed_payloads(0);
  EXPECT_EQ(browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_PROBE,
            typed_payload.payload_type());
  EXPECT_EQ("type.googleapis.com/browser_actuator.TransportProbe",
            typed_payload.proto_payload().type_url());
  EXPECT_EQ(probe.SerializeAsString(), typed_payload.proto_payload().value());

  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Upstream.PayloadSize.Probe", probe.ByteSizeLong(), 1);
  histogram_tester.ExpectTotalCount(
      "StreamTransport.Upstream.UploadLatency.Probe", 1);
  histogram_tester.ExpectUniqueSample("StreamTransport.Upstream.HttpStatus",
                                      net::HTTP_OK, 1);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendMessageTaskIngressSuccess) {
  base::HistogramTester histogram_tester;
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "OK", net::HTTP_OK);

  // `ControlCommand` is used as a schema-agnostic `MessageLite` stand-in here
  // because stream_transport cannot depend on `task_ingress.proto`.
  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kTaskIngress, command,
                             future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(net::HTTP_OK, status);

  ASSERT_EQ(1u, requests_.size());
  browser_actuator::SendSessionMessageRequest request_proto;
  ASSERT_TRUE(
      request_proto.ParseFromString(network::GetUploadData(requests_[0])));

  const browser_actuator::ActuatorUpstreamMessage& upstream =
      request_proto.actuator_upstream_message();
  ASSERT_EQ(1, upstream.typed_payloads_size());
  const browser_actuator::ActuatorUpstreamTypedPayload& typed_payload =
      upstream.typed_payloads(0);
  EXPECT_EQ(browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_TASK_INGRESS,
            typed_payload.payload_type());
  EXPECT_EQ("type.googleapis.com/browser_actuator.ControlCommand",
            typed_payload.proto_payload().type_url());
  EXPECT_EQ(command.SerializeAsString(), typed_payload.proto_payload().value());

  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Upstream.PayloadSize.TaskIngress",
      command.ByteSizeLong(), 1);
  histogram_tester.ExpectTotalCount(
      "StreamTransport.Upstream.UploadLatency.TaskIngress", 1);
  histogram_tester.ExpectUniqueSample("StreamTransport.Upstream.HttpStatus",
                                      net::HTTP_OK, 1);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendMessageWithRespondingToSequenceNumber) {
  base::HistogramTester histogram_tester;
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "OK", net::HTTP_OK);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  browser_actuator::ActuatorUpstreamMessage returned_upstream =
      client.SendUpstreamMessage("session_1", /*client_sequence_number=*/2,
                                 /*responding_to_sequence_number=*/42,
                                 PayloadType::kControl, command,
                                 future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(net::HTTP_OK, status);

  ASSERT_EQ(1u, requests_.size());
  const auto& request = requests_[0];

  browser_actuator::SendSessionMessageRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(network::GetUploadData(request)));

  const auto& upstream = request_proto.actuator_upstream_message();
  EXPECT_EQ("session_1", upstream.session_id());
  EXPECT_EQ(2, upstream.client_sequence_number());
  EXPECT_TRUE(upstream.has_responding_to_sequence_number());
  EXPECT_EQ(42, upstream.responding_to_sequence_number());
  EXPECT_EQ(returned_upstream.SerializeAsString(),
            upstream.SerializeAsString());

  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Upstream.PayloadSize.Control", command.ByteSizeLong(),
      1);
  histogram_tester.ExpectTotalCount(
      "StreamTransport.Upstream.UploadLatency.Control", 1);
  histogram_tester.ExpectUniqueSample("StreamTransport.Upstream.HttpStatus",
                                      net::HTTP_OK, 1);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest, SendMessageHttpError) {
  base::HistogramTester histogram_tester;
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "Error",
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(net::HTTP_INTERNAL_SERVER_ERROR, status);

  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Upstream.PayloadSize.Control", command.ByteSizeLong(),
      1);
  histogram_tester.ExpectTotalCount(
      "StreamTransport.Upstream.UploadLatency.Control", 1);
  histogram_tester.ExpectUniqueSample("StreamTransport.Upstream.HttpStatus",
                                      net::HTTP_INTERNAL_SERVER_ERROR, 1);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       FailsWhenNetworkErrorOccursAfterReceivingHttpOkHeaders) {
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(
      GURL(kEndpoint), network::CreateURLResponseHead(net::HTTP_OK),
      /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET),
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::kSendHeadersOnNetworkError);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(-1, status);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       DestructionCancelsInFlightRequest) {
  base::HistogramTester histogram_tester;
  auto client = std::make_unique<UpstreamMessageClient>(
      test_shared_loader_factory_, identity_test_env_.identity_manager(),
      GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client->SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                              /*responding_to_sequence_number=*/std::nullopt,
                              PayloadType::kControl, command,
                              future.GetCallback());

  EXPECT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 1u; }));

  // Payload size should be logged immediately upon sending.
  histogram_tester.ExpectUniqueSample(
      "StreamTransport.Upstream.PayloadSize.Control", command.ByteSizeLong(),
      1);

  // Destroying the client while the network request is pending cancels cleanly.
  client.reset();

  EXPECT_FALSE(future.IsReady());

  // Latency and HttpStatus should not be logged for cancelled/aborted requests.
  histogram_tester.ExpectTotalCount(
      "StreamTransport.Upstream.UploadLatency.Control", 0);
  histogram_tester.ExpectTotalCount("StreamTransport.Upstream.HttpStatus", 0);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       CancelPendingRequestsClearsInFlightRequestsWithoutRunningCallbacks) {
  base::HistogramTester histogram_tester;
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(0u, client.GetPendingRequestsCountForTesting());

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> first_future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kControl, command,
                             first_future.GetCallback());
  base::test::TestFuture<int> second_future;
  client.SendUpstreamMessage("session_2", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kControl, command,
                             second_future.GetCallback());

  EXPECT_EQ(2u, client.GetPendingRequestsCountForTesting());
  EXPECT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 2u; }));

  client.CancelPendingRequests();

  EXPECT_EQ(0u, client.GetPendingRequestsCountForTesting());
  EXPECT_FALSE(first_future.IsReady());
  EXPECT_FALSE(second_future.IsReady());

  // Latency and HttpStatus should not be logged for cancelled requests.
  histogram_tester.ExpectTotalCount(
      "StreamTransport.Upstream.UploadLatency.Control", 0);
  histogram_tester.ExpectTotalCount("StreamTransport.Upstream.HttpStatus", 0);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       RejectsUntrustworthyCleartextEndpoint) {
  UpstreamMessageClient client(
      test_shared_loader_factory_, identity_test_env_.identity_manager(),
      GURL("http://example.com/v1/send"), TRAFFIC_ANNOTATION_FOR_TESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(-1, status);
  EXPECT_TRUE(requests_.empty());
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       EnforcesMaxPendingUpstreamRequests) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kBrowserActuatorProtoStreamTransport,
      {{kMaxPendingUpstreamRequests.name, "2"}});

  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> f1;
  base::test::TestFuture<int> f2;
  base::test::TestFuture<int> f3;

  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command, f1.GetCallback());
  client.SendUpstreamMessage("session_1", 2, std::nullopt,
                             PayloadType::kControl, command, f2.GetCallback());
  client.SendUpstreamMessage("session_1", 3, std::nullopt,
                             PayloadType::kControl, command, f3.GetCallback());

  int status3 = f3.Get();
  EXPECT_EQ(-1, status3);
  EXPECT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 2u; }));
  EXPECT_EQ(2u, requests_.size());

  // Completing one in-flight request frees capacity for a subsequent send.
  test_url_loader_factory_.SimulateResponseForPendingRequest(kEndpoint, "",
                                                             net::HTTP_OK);
  int status1 = f1.Get();
  EXPECT_EQ(net::HTTP_OK, status1);

  base::test::TestFuture<int> f4;
  client.SendUpstreamMessage("session_1", 4, std::nullopt,
                             PayloadType::kControl, command, f4.GetCallback());
  EXPECT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 3u; }));
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       RetriesOnceOnHttpUnauthorizedWithRefreshedToken) {
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "stale_token", base::Time::Now() + base::Hours(1));
  ASSERT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 1u; }));
  EXPECT_EQ("Bearer stale_token", requests_[0].headers.GetHeader(
                                      net::HttpRequestHeaders::kAuthorization));

  // Respond with HTTP 401 to the first attempt.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kEndpoint, "Unauthorized", net::HTTP_UNAUTHORIZED);
  EXPECT_EQ(1u, removed_token_account_ids_.size());

  // Client invalidates cached token and fetches a fresh token once.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "fresh_token", base::Time::Now() + base::Hours(1));
  ASSERT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 2u; }));
  EXPECT_EQ("Bearer fresh_token", requests_[1].headers.GetHeader(
                                      net::HttpRequestHeaders::kAuthorization));

  test_url_loader_factory_.SimulateResponseForPendingRequest(kEndpoint, "OK",
                                                             net::HTTP_OK);

  int status = future.Get();
  EXPECT_EQ(net::HTTP_OK, status);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       StopsAfterSingleRetryOnRepeatedHttpUnauthorized) {
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "first_token", base::Time::Now() + base::Hours(1));
  ASSERT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 1u; }));
  EXPECT_EQ("Bearer first_token", requests_[0].headers.GetHeader(
                                      net::HttpRequestHeaders::kAuthorization));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kEndpoint, "Unauthorized", net::HTTP_UNAUTHORIZED);
  EXPECT_EQ(1u, removed_token_account_ids_.size());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "second_token", base::Time::Now() + base::Hours(1));
  ASSERT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 2u; }));
  EXPECT_EQ(
      "Bearer second_token",
      requests_[1].headers.GetHeader(net::HttpRequestHeaders::kAuthorization));

  // Second 401 terminates without another retry and evicts the second token.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kEndpoint, "Unauthorized", net::HTTP_UNAUTHORIZED);

  int status = future.Get();
  EXPECT_EQ(net::HTTP_UNAUTHORIZED, status);
  EXPECT_EQ(2u, requests_.size());
  EXPECT_EQ(2u, removed_token_account_ids_.size());
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       FailsWhenAccessTokenFetchFails) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kBrowserActuatorProtoStreamTransport,
      {{kMaxPendingUpstreamRequests.name, "1"}});
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future1;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future1.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromServiceError(""));

  int status1 = future1.Get();
  EXPECT_EQ(-1, status1);
  EXPECT_TRUE(requests_.empty());

  // Verify the failed request was removed from `pending_requests_` so the
  // single-request capacity slot is free for subsequent calls.
  test_url_loader_factory_.AddResponse(kEndpoint, "OK", net::HTTP_OK);
  base::test::TestFuture<int> future2;
  client.SendUpstreamMessage("session_1", 2, std::nullopt,
                             PayloadType::kControl, command,
                             future2.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "valid_token", base::Time::Now() + base::Hours(1));

  int status2 = future2.Get();
  EXPECT_EQ(net::HTTP_OK, status2);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       DoesNotRetryHttpUnauthorizedAfterPrimaryAccountSwitch) {
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);
  const CoreAccountId initial_account_id =
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin);

  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "account1_token", base::Time::Now() + base::Hours(1));
  ASSERT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 1u; }));

  // Switch primary account while the initial HTTP request is in flight.
  const AccountInfo new_account =
      identity_test_env_.MakePrimaryAccountAvailable(
          "other@example.com", signin::ConsentLevel::kSignin);
  ASSERT_NE(initial_account_id, new_account.GetAccountId());

  // Respond with HTTP 401: the client must evict the token under the issuing
  // account and complete immediately without retrying under the new account.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kEndpoint, "Unauthorized", net::HTTP_UNAUTHORIZED);

  int status = future.Get();
  EXPECT_EQ(net::HTTP_UNAUTHORIZED, status);
  EXPECT_EQ(1u, requests_.size());
  ASSERT_EQ(1u, removed_token_account_ids_.size());
  EXPECT_EQ(initial_account_id, removed_token_account_ids_[0]);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(BrowserActuatorUpstreamMessageClientTest,
       DoesNotRetryHttpUnauthorizedAfterSignOut) {
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);
  const CoreAccountId initial_account_id =
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin);

  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "account1_token", base::Time::Now() + base::Hours(1));
  ASSERT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 1u; }));

  // Sign out while the initial HTTP request is in flight.
  identity_test_env_.ClearPrimaryAccount();

  // Respond with HTTP 401: the client must evict the token under the issuing
  // account and complete immediately without hanging in `kWaitUntilAvailable`.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kEndpoint, "Unauthorized", net::HTTP_UNAUTHORIZED);

  int status = future.Get();
  EXPECT_EQ(net::HTTP_UNAUTHORIZED, status);
  EXPECT_EQ(1u, requests_.size());
  ASSERT_EQ(1u, removed_token_account_ids_.size());
  EXPECT_EQ(initial_account_id, removed_token_account_ids_[0]);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendCompleteCallbackReceives204StatusCode) {
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "", net::HTTP_NO_CONTENT);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(status, net::HTTP_NO_CONTENT);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendCompleteCallbackReceives409StatusCode) {
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "Conflict",
                                       net::HTTP_CONFLICT);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(status, net::HTTP_CONFLICT);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendCompleteCallbackReceives503StatusCode) {
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "Unavailable",
                                       net::HTTP_SERVICE_UNAVAILABLE);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(status, net::HTTP_SERVICE_UNAVAILABLE);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendCompleteCallbackReceives429StatusCode) {
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "Too Many Requests",
                                       net::HTTP_TOO_MANY_REQUESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(status, net::HTTP_TOO_MANY_REQUESTS);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendCompleteCallbackReceives400StatusCode) {
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "Bad Request",
                                       net::HTTP_BAD_REQUEST);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  int status = future.Get();
  EXPECT_EQ(status, net::HTTP_BAD_REQUEST);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendCompleteCallbackReceivesMinusOneOnNetworkError) {
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  network::URLLoaderCompletionStatus status(net::ERR_CONNECTION_RESET);
  test_url_loader_factory_.AddResponse(
      GURL(kEndpoint), network::mojom::URLResponseHead::New(), "", status);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  int response_code = future.Get();
  EXPECT_EQ(response_code, -1);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendCompleteCallbackReceivesMinusOneOnOAuthFailure) {
  // Use a separate unauthenticated `IdentityTestEnvironment` so no primary
  // account is signed in (avoiding `ClearPrimaryAccount()`, which is
  // disallowed on ChromeOS).
  signin::IdentityTestEnvironment unauthenticated_identity_env(
      &test_url_loader_factory_);

  UpstreamMessageClient client(test_shared_loader_factory_,
                               unauthenticated_identity_env.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  int response_code = future.Get();
  EXPECT_EQ(response_code, -1);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendCompleteCallbackReceivesMinusOneOnTransientOAuthFailure) {
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<int> future;
  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_TIMED_OUT));

  int response_code = future.Get();
  EXPECT_EQ(response_code, -1);
  EXPECT_TRUE(requests_.empty());
}

}  // namespace
}  // namespace stream_transport
