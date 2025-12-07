// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/receiver_connection_info_poller.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/util.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca_receiver {
namespace {

constexpr char kReceiverId[] = "receiver-id";
constexpr char kConnectionId[] = "connection-id";

constexpr char kConnectedResponse[] =
    R"({"connectionId": "connection-id",
        "receiverConnectionState": "CONNECTED"})";
constexpr char kStopRequestedResponse[] =
    R"({"connectionId": "connection-id",
        "receiverConnectionState": "STOP_REQUESTED"})";

class ReceiverConnectionInfoPollerTest : public testing::Test {
 protected:
  void SetUp() override {
    identity_test_env_.MakeAccountAvailable("test@example.com");
    identity_test_env_.SetPrimaryAccount("test@example.com",
                                         signin::ConsentLevel::kSync);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  }

  std::unique_ptr<google_apis::RequestSender> CreateRequestSender() {
    auto auth_service = std::make_unique<google_apis::AuthService>(
        identity_test_env_.identity_manager(),
        identity_test_env_.identity_manager()->GetPrimaryAccountId(
            signin::ConsentLevel::kSignin),
        url_loader_factory_.GetSafeWeakWrapper(),
        signin::OAuthConsumerId::kChromeOsBocaSchoolToolsAuth);
    return std::make_unique<google_apis::RequestSender>(
        std::move(auth_service), url_loader_factory_.GetSafeWeakWrapper(),
        task_environment_.GetMainThreadTaskRunner(), "test-user-agent",
        TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  GURL GetConnectionInfoUrl(std::string_view connection_id) {
    return GURL(base::StrCat(
        {boca::GetSchoolToolsUrl(),
         base::ReplaceStringPlaceholders(
             "/v1/receivers/$1/kioskReceiver:getConnectionInfo?connectionId=$2",
             {std::string(kReceiverId), std::string(connection_id)},
             nullptr)}));
  }

  void SimulateResponse(std::string_view connection_id,
                        std::string_view content,
                        net::HttpStatusCode status_code = net::HTTP_OK) {
    url_loader_factory_.SimulateResponseForPendingRequest(
        GetConnectionInfoUrl(connection_id).spec(), content, status_code,
        network::TestURLLoaderFactory::ResponseMatchFlags::kWaitForRequest);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory url_loader_factory_;
  ReceiverConnectionInfoPoller poller_;
};

TEST_F(ReceiverConnectionInfoPollerTest, StartsAndPollsSuccessfully) {
  base::test::TestFuture<bool> stop_future;
  poller_.Start(kReceiverId, kConnectionId, CreateRequestSender(),
                stop_future.GetCallback());

  // First poll after interval.
  task_environment_.FastForwardBy(base::Seconds(10));
  SimulateResponse(kConnectionId, kConnectedResponse);
  EXPECT_FALSE(stop_future.IsReady());

  // Second poll.
  task_environment_.FastForwardBy(base::Seconds(10));
  SimulateResponse(kConnectionId, kConnectedResponse);
  EXPECT_FALSE(stop_future.IsReady());
}

TEST_F(ReceiverConnectionInfoPollerTest, StopsOnServerRequest) {
  base::test::TestFuture<bool> stop_future;
  poller_.Start(kReceiverId, kConnectionId, CreateRequestSender(),
                stop_future.GetCallback());

  task_environment_.FastForwardBy(base::Seconds(10));
  SimulateResponse(kConnectionId, kStopRequestedResponse);
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_FALSE(stop_future.Get());  // server_unreachable is false.
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
}

TEST_F(ReceiverConnectionInfoPollerTest, StopsAfterFailures) {
  base::test::TestFuture<bool> stop_future;
  poller_.Start(kReceiverId, kConnectionId, CreateRequestSender(),
                stop_future.GetCallback());

  // First failure (with one retry).
  task_environment_.FastForwardBy(base::Seconds(10));
  SimulateResponse(kConnectionId, /*content=*/"", net::HTTP_NOT_FOUND);
  SimulateResponse(kConnectionId, /*content=*/"", net::HTTP_NOT_FOUND);

  // Second failure.
  task_environment_.FastForwardBy(base::Seconds(10));
  SimulateResponse(kConnectionId, /*content=*/"", net::HTTP_NOT_FOUND);
  SimulateResponse(kConnectionId, /*content=*/"", net::HTTP_NOT_FOUND);

  // Third failure.
  task_environment_.FastForwardBy(base::Seconds(10));
  SimulateResponse(kConnectionId, /*content=*/"", net::HTTP_NOT_FOUND);
  SimulateResponse(kConnectionId, /*content=*/"", net::HTTP_NOT_FOUND);

  EXPECT_TRUE(stop_future.Get());  // server_unreachable is true.
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
}

TEST_F(ReceiverConnectionInfoPollerTest, ExplicitStop) {
  base::test::TestFuture<bool> stop_future;
  task_environment_.FastForwardBy(base::Seconds(10));
  poller_.Start(kReceiverId, kConnectionId, CreateRequestSender(),
                stop_future.GetCallback());

  task_environment_.FastForwardBy(base::Seconds(2));
  poller_.Stop();
  // No polling should happen.
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
}

TEST_F(ReceiverConnectionInfoPollerTest, CustomPollingEnabled) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["BocaReceiverCustomPollingInterval"] = "5s";
  params["BocaReceiverCustomPollingMaxFailuresCount"] = "2";
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kBocaReceiverCustomPolling, params);

  base::test::TestFuture<bool> stop_future;
  poller_.Start(kReceiverId, kConnectionId, CreateRequestSender(),
                stop_future.GetCallback());

  // First poll after custom interval.
  task_environment_.FastForwardBy(base::Seconds(5));
  SimulateResponse(kConnectionId, kConnectedResponse);
  EXPECT_FALSE(stop_future.IsReady());

  // First failure (with one retry).
  task_environment_.FastForwardBy(base::Seconds(5));
  SimulateResponse(kConnectionId, /*content=*/"", net::HTTP_NOT_FOUND);
  SimulateResponse(kConnectionId, /*content=*/"", net::HTTP_NOT_FOUND);
  EXPECT_FALSE(stop_future.IsReady());

  // Second failure. Poller should stop.
  task_environment_.FastForwardBy(base::Seconds(5));
  SimulateResponse(kConnectionId, /*content=*/"", net::HTTP_NOT_FOUND);
  SimulateResponse(kConnectionId, /*content=*/"", net::HTTP_NOT_FOUND);

  EXPECT_TRUE(stop_future.Get());  // server_unreachable is true.
}

}  // namespace
}  // namespace ash::boca_receiver
