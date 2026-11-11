// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/internal/upstream_message_client/upstream_message_client.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/stream_transport/internal/features.h"
#include "components/stream_transport/internal/proto/task_ingress.pb.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"
#include "components/stream_transport/public/common.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace stream_transport {
namespace {

constexpr char kEndpoint[] = "https://example.com/v1/send";

class BrowserActuatorUpstreamMessageClientTest : public testing::Test {
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

    identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::vector<network::ResourceRequest> requests_;
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

  base::test::TestFuture<bool, int> future;
  browser_actuator::ActuatorUpstreamMessage returned_upstream =
      client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                                 /*responding_to_sequence_number=*/std::nullopt,
                                 PayloadType::kControl, command,
                                 future.GetCallback());

  EXPECT_TRUE(future.Get<0>());
  EXPECT_EQ(net::HTTP_OK, future.Get<1>());

  ASSERT_EQ(1u, requests_.size());
  const auto& request = requests_[0];
  EXPECT_EQ(kEndpoint, request.url.spec());
  EXPECT_EQ("POST", request.method);
  EXPECT_EQ(network::mojom::RedirectMode::kError, request.redirect_mode);

  browser_actuator::SendSessionMessageRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(network::GetUploadData(request)));

  const auto& upstream = request_proto.actuator_upstream_message();
  EXPECT_EQ("session_1", upstream.session_id());
  EXPECT_EQ(1, upstream.client_sequence_number());
  EXPECT_FALSE(upstream.has_responding_to_sequence_number());
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
      "Browser.Actuator.Transport.Upstream.PayloadSize.Control",
      command.ByteSizeLong(), 1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.Upstream.UploadLatency.Control", 1);
  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Upstream.HttpStatus", net::HTTP_OK, 1);
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

  base::test::TestFuture<bool, int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kExperimentalTriggering, command,
                             future.GetCallback());

  EXPECT_TRUE(future.Get<0>());
  EXPECT_EQ(net::HTTP_OK, future.Get<1>());

  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Upstream.PayloadSize.ExperimentalTriggering",
      command.ByteSizeLong(), 1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.Upstream.UploadLatency."
      "ExperimentalTriggering",
      1);
  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Upstream.HttpStatus", net::HTTP_OK, 1);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest, SendMessageProbeSuccess) {
  base::HistogramTester histogram_tester;
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "OK", net::HTTP_OK);

  browser_actuator::TransportProbe probe;
  probe.mutable_response()->set_probe_id("probe_1");

  base::test::TestFuture<bool, int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kProbe, probe, future.GetCallback());

  EXPECT_TRUE(future.Get<0>());
  EXPECT_EQ(net::HTTP_OK, future.Get<1>());

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
      "Browser.Actuator.Transport.Upstream.PayloadSize.Probe",
      probe.ByteSizeLong(), 1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.Upstream.UploadLatency.Probe", 1);
  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Upstream.HttpStatus", net::HTTP_OK, 1);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendMessageTaskIngressSuccess) {
  base::HistogramTester histogram_tester;
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "OK", net::HTTP_OK);

  browser_actuator::TaskIngressUpstreamPayload payload;
  payload.mutable_start_task()->set_use_case(
      browser_actuator::TASK_USE_CASE_AUTO_PASSWORD_CHANGE);

  base::test::TestFuture<bool, int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kTaskIngress, payload,
                             future.GetCallback());

  EXPECT_TRUE(future.Get<0>());
  EXPECT_EQ(net::HTTP_OK, future.Get<1>());

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
  EXPECT_EQ("type.googleapis.com/browser_actuator.TaskIngressUpstreamPayload",
            typed_payload.proto_payload().type_url());
  EXPECT_EQ(payload.SerializeAsString(), typed_payload.proto_payload().value());

  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Upstream.PayloadSize.TaskIngress",
      payload.ByteSizeLong(), 1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.Upstream.UploadLatency.TaskIngress", 1);
  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Upstream.HttpStatus", net::HTTP_OK, 1);
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

  base::test::TestFuture<bool, int> future;
  browser_actuator::ActuatorUpstreamMessage returned_upstream =
      client.SendUpstreamMessage("session_1", /*client_sequence_number=*/2,
                                 /*responding_to_sequence_number=*/42,
                                 PayloadType::kControl, command,
                                 future.GetCallback());

  EXPECT_TRUE(future.Get<0>());
  EXPECT_EQ(net::HTTP_OK, future.Get<1>());

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
      "Browser.Actuator.Transport.Upstream.PayloadSize.Control",
      command.ByteSizeLong(), 1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.Upstream.UploadLatency.Control", 1);
  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Upstream.HttpStatus", net::HTTP_OK, 1);
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

  base::test::TestFuture<bool, int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  EXPECT_FALSE(future.Get<0>());
  EXPECT_EQ(net::HTTP_INTERNAL_SERVER_ERROR, future.Get<1>());

  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Upstream.PayloadSize.Control",
      command.ByteSizeLong(), 1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.Upstream.UploadLatency.Control", 1);
  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Upstream.HttpStatus",
      net::HTTP_INTERNAL_SERVER_ERROR, 1);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       DestructionCancelsInFlightRequest) {
  base::HistogramTester histogram_tester;
  auto client = std::make_unique<UpstreamMessageClient>(
      test_shared_loader_factory_, identity_test_env_.identity_manager(),
      GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<bool, int> future;
  client->SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                              /*responding_to_sequence_number=*/std::nullopt,
                              PayloadType::kControl, command,
                              future.GetCallback());

  EXPECT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 1u; }));

  // Payload size should be logged immediately upon sending.
  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.Upstream.PayloadSize.Control",
      command.ByteSizeLong(), 1);

  // Destroying the client while the network request is pending cancels cleanly.
  client.reset();

  EXPECT_FALSE(future.IsReady());

  // Latency and HttpStatus should not be logged for cancelled/aborted requests.
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.Upstream.UploadLatency.Control", 0);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.Upstream.HttpStatus", 0);
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

  base::test::TestFuture<bool, int> first_future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kControl, command,
                             first_future.GetCallback());
  base::test::TestFuture<bool, int> second_future;
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
      "Browser.Actuator.Transport.Upstream.UploadLatency.Control", 0);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.Upstream.HttpStatus", 0);
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       RejectsUntrustworthyCleartextEndpoint) {
  UpstreamMessageClient client(
      test_shared_loader_factory_, identity_test_env_.identity_manager(),
      GURL("http://example.com/v1/send"), TRAFFIC_ANNOTATION_FOR_TESTS);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<bool, int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  EXPECT_FALSE(future.Get<0>());
  EXPECT_EQ(-1, future.Get<1>());
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

  base::test::TestFuture<bool, int> f1;
  base::test::TestFuture<bool, int> f2;
  base::test::TestFuture<bool, int> f3;

  client.SendUpstreamMessage("session_1", 1, std::nullopt,
                             PayloadType::kControl, command, f1.GetCallback());
  client.SendUpstreamMessage("session_1", 2, std::nullopt,
                             PayloadType::kControl, command, f2.GetCallback());
  client.SendUpstreamMessage("session_1", 3, std::nullopt,
                             PayloadType::kControl, command, f3.GetCallback());

  EXPECT_FALSE(f3.Get<0>());
  EXPECT_EQ(-1, f3.Get<1>());
  EXPECT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 2u; }));
  EXPECT_EQ(2u, requests_.size());

  // Completing one in-flight request frees capacity for a subsequent send.
  test_url_loader_factory_.SimulateResponseForPendingRequest(kEndpoint, "",
                                                             net::HTTP_OK);
  EXPECT_TRUE(f1.Get<0>());
  EXPECT_EQ(net::HTTP_OK, f1.Get<1>());

  base::test::TestFuture<bool, int> f4;
  client.SendUpstreamMessage("session_1", 4, std::nullopt,
                             PayloadType::kControl, command, f4.GetCallback());
  EXPECT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 3u; }));
}

}  // namespace
}  // namespace stream_transport
