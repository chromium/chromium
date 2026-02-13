// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/device_statistics_request_impl.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/protocol/sync.pb.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

const char kTestEmail[] = "test@gmail.com";
const char kTestUrl[] = "https://test.com";

class DeviceStatisticsRequestImplTest : public testing::Test {
 public:
  DeviceStatisticsRequestImplTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    account_info_ = identity_test_env_.MakeAccountAvailable(kTestEmail);
  }

 protected:
  void SetOkResponse(std::string_view client_name) {
    sync_pb::ClientToServerResponse response;
    sync_pb::SyncEntity* entity = response.mutable_get_updates()->add_entries();
    entity->mutable_specifics()->mutable_device_info()->set_client_name(
        client_name);

    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        "HTTP/1.1 200 OK\nContent-Type: application/octet-stream");
    network::URLLoaderCompletionStatus status(net::OK);
    status.decoded_body_length = response.SerializeAsString().size();
    test_url_loader_factory_.AddResponse(GURL(kTestUrl), std::move(head),
                                         response.SerializeAsString(), status);
  }

  void SetUnauthorizedResponse() {
    auto unauthorized_head = network::mojom::URLResponseHead::New();
    unauthorized_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        "HTTP/1.1 401 Unauthorized\nContent-Type: application/octet-stream");
    network::URLLoaderCompletionStatus unauthorized_status(net::OK);
    test_url_loader_factory_.AddResponse(
        GURL(kTestUrl), std::move(unauthorized_head), "", unauthorized_status);
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  CoreAccountInfo account_info_;
};

TEST_F(DeviceStatisticsRequestImplTest, ShouldSucceed) {
  SetOkResponse("test_client");

  DeviceStatisticsRequestImpl request(identity_test_env_.identity_manager(),
                                      shared_url_loader_factory_, "user_agent",
                                      account_info_, GURL(kTestUrl));

  base::test::TestFuture<void> future;
  request.Start(future.GetCallback());
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token", base::Time::Max(), "id_token",
          {GaiaConstants::kChromeSyncOAuth2Scope});

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(DeviceStatisticsRequest::State::kComplete, request.GetState());
  ASSERT_EQ(1u, request.GetResults().size());
  EXPECT_EQ("test_client",
            request.GetResults()[0].specifics().device_info().client_name());
}

TEST_F(DeviceStatisticsRequestImplTest, ShouldHandleAuthError) {
  DeviceStatisticsRequestImpl request(identity_test_env_.identity_manager(),
                                      shared_url_loader_factory_, "user_agent",
                                      account_info_, GURL(kTestUrl));

  base::test::TestFuture<void> future;
  request.Start(future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(DeviceStatisticsRequest::State::kFailed, request.GetState());
}

TEST_F(DeviceStatisticsRequestImplTest, ShouldHandleNetworkError) {
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      "HTTP/1.1 500 Internal Server Error\nContent-Type: "
      "application/octet-stream");
  network::URLLoaderCompletionStatus status(net::ERR_FAILED);
  test_url_loader_factory_.AddResponse(GURL(kTestUrl), std::move(head), "",
                                       status);

  DeviceStatisticsRequestImpl request(identity_test_env_.identity_manager(),
                                      shared_url_loader_factory_, "user_agent",
                                      account_info_, GURL(kTestUrl));

  base::test::TestFuture<void> future;
  request.Start(future.GetCallback());
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token", base::Time::Max(), "id_token",
          {GaiaConstants::kChromeSyncOAuth2Scope});
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(DeviceStatisticsRequest::State::kFailed, request.GetState());
}

TEST_F(DeviceStatisticsRequestImplTest, ShouldRetryOnUnauthorized) {
  DeviceStatisticsRequestImpl request(identity_test_env_.identity_manager(),
                                      shared_url_loader_factory_, "user_agent",
                                      account_info_, GURL(kTestUrl));

  // First response is "unauthorized".
  SetUnauthorizedResponse();

  base::test::TestFuture<void> future;
  request.Start(future.GetCallback());
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token", base::Time::Max(), "id_token",
          {GaiaConstants::kChromeSyncOAuth2Scope});

  // The access token should've been revoked, and a new access token request
  // should get triggered.

  //  Set up the second server response as "OK".
  SetOkResponse("test_client");
  // Once the second access token comes in, the entire request should succeed.
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token2", base::Time::Max(), "id_token",
          {GaiaConstants::kChromeSyncOAuth2Scope});

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(DeviceStatisticsRequest::State::kComplete, request.GetState());
  ASSERT_EQ(1u, request.GetResults().size());
  EXPECT_EQ("test_client",
            request.GetResults()[0].specifics().device_info().client_name());
}

TEST_F(DeviceStatisticsRequestImplTest, ShouldRetryOnlyOnceOnUnauthorized) {
  // The server always responds "unauthorized".
  SetUnauthorizedResponse();

  DeviceStatisticsRequestImpl request(identity_test_env_.identity_manager(),
                                      shared_url_loader_factory_, "user_agent",
                                      account_info_, GURL(kTestUrl));

  base::test::TestFuture<void> future;
  request.Start(future.GetCallback());
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token", base::Time::Max(), "id_token",
          {GaiaConstants::kChromeSyncOAuth2Scope});
  // The request should retry with a new access token exactly once.
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token2", base::Time::Max(), "id_token",
          {GaiaConstants::kChromeSyncOAuth2Scope});

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(DeviceStatisticsRequest::State::kFailed, request.GetState());
}

}  // namespace

}  // namespace syncer
