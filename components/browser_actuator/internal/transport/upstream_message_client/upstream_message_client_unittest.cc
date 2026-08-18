// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/upstream_message_client/upstream_message_client.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/common.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browser_actuator {
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
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "OK", net::HTTP_OK);

  ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<bool, int> future;
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

  SendSessionMessageRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(network::GetUploadData(request)));

  const auto& upstream = request_proto.actuator_upstream_message();
  EXPECT_EQ("session_1", upstream.session_id());
  EXPECT_EQ(1, upstream.client_sequence_number());
  EXPECT_FALSE(upstream.has_responding_to_sequence_number());

  ASSERT_EQ(1, upstream.typed_payloads_size());
  const auto& typed_payload = upstream.typed_payloads(0);
  EXPECT_EQ(ACTUATOR_UPSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND,
            typed_payload.payload_type());
  EXPECT_EQ("type.googleapis.com/browser_actuator.ControlCommand",
            typed_payload.proto_payload().type_url());
  EXPECT_EQ(command.SerializeAsString(), typed_payload.proto_payload().value());
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       SendMessageWithRespondingToSequenceNumber) {
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "OK", net::HTTP_OK);

  ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<bool, int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/2,
                             /*responding_to_sequence_number=*/42,
                             PayloadType::kControl, command,
                             future.GetCallback());

  EXPECT_TRUE(future.Get<0>());
  EXPECT_EQ(net::HTTP_OK, future.Get<1>());

  ASSERT_EQ(1u, requests_.size());
  const auto& request = requests_[0];

  SendSessionMessageRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(network::GetUploadData(request)));

  const auto& upstream = request_proto.actuator_upstream_message();
  EXPECT_EQ("session_1", upstream.session_id());
  EXPECT_EQ(2, upstream.client_sequence_number());
  EXPECT_TRUE(upstream.has_responding_to_sequence_number());
  EXPECT_EQ(42, upstream.responding_to_sequence_number());
}

TEST_F(BrowserActuatorUpstreamMessageClientTest, SendMessageHttpError) {
  UpstreamMessageClient client(test_shared_loader_factory_,
                               identity_test_env_.identity_manager(),
                               GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  test_url_loader_factory_.AddResponse(kEndpoint, "Error",
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<bool, int> future;
  client.SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                             /*responding_to_sequence_number=*/std::nullopt,
                             PayloadType::kControl, command,
                             future.GetCallback());

  EXPECT_FALSE(future.Get<0>());
  EXPECT_EQ(net::HTTP_INTERNAL_SERVER_ERROR, future.Get<1>());
}

TEST_F(BrowserActuatorUpstreamMessageClientTest,
       DestructionCancelsInFlightRequest) {
  auto client = std::make_unique<UpstreamMessageClient>(
      test_shared_loader_factory_, identity_test_env_.identity_manager(),
      GURL(kEndpoint), TRAFFIC_ANNOTATION_FOR_TESTS);

  ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<bool, int> future;
  client->SendUpstreamMessage("session_1", /*client_sequence_number=*/1,
                              /*responding_to_sequence_number=*/std::nullopt,
                              PayloadType::kControl, command,
                              future.GetCallback());

  EXPECT_TRUE(
      base::test::RunUntil([this]() { return requests_.size() == 1u; }));

  // Destroying the client while the network request is pending cancels cleanly.
  client.reset();

  EXPECT_FALSE(future.IsReady());
}

}  // namespace
}  // namespace browser_actuator
