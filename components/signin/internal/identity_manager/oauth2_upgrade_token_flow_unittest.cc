// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth2_upgrade_token_flow.h"

#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "components/signin/public/base/signin_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace signin {

using ::testing::Optional;
using ::testing::Test;

class OAuth2UpgradeTokenFlowTest : public Test {
 protected:
  OAuth2UpgradeTokenFlowTest()
      : shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void AddFetchResult(const GURL& url,
                      bool fetch_succeeds,
                      net::HttpStatusCode response_code,
                      const std::string& body) {
    net::Error error = fetch_succeeds ? net::OK : net::ERR_FAILED;

    auto http_head = network::CreateURLResponseHead(response_code);
    test_url_loader_factory_.AddResponse(
        url, std::move(http_head), body,
        network::URLLoaderCompletionStatus(error));
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  base::HistogramTester histogram_tester_;
};

TEST_F(OAuth2UpgradeTokenFlowTest, RequestConfig) {
  base::test::TestFuture<void> future;
  OAuth2UpgradeTokenFlow flow(
      "test_token", switches::RefreshTokenBindingUpgradeType::kLiveLaunch,
      "test_device_id", shared_factory_, future.GetCallback());
  flow.StartWithRegistrationToken("token_binding_assertion");

  const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
      *test_url_loader_factory_.pending_requests();
  ASSERT_EQ(pending.size(), 1u);
  EXPECT_EQ(pending[0].request.url,
            GaiaUrls::GetInstance()->oauth2_upgrade_token_url());
  EXPECT_EQ(pending[0].request.method, "POST");

  EXPECT_FALSE(pending[0].request.headers.HasHeader("Authorization"));
  EXPECT_THAT(network::GetUploadData(pending[0].request),
              base::test::IsJson(R"({
                "deviceId": "test_device_id",
                "token": "test_token",
                "tokenBindingRegistrationJwt": "token_binding_assertion",
                "upgradeType": "BIND_TO_KEY"
              })"));
}

TEST_F(OAuth2UpgradeTokenFlowTest, RequestConfigDarkLaunchNoDeviceId) {
  base::test::TestFuture<void> future;
  OAuth2UpgradeTokenFlow flow(
      "test_token", switches::RefreshTokenBindingUpgradeType::kDarkLaunch, "",
      shared_factory_, future.GetCallback());
  flow.StartWithRegistrationToken("token_binding_assertion");

  const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
      *test_url_loader_factory_.pending_requests();
  ASSERT_EQ(pending.size(), 1u);
  EXPECT_THAT(network::GetUploadData(pending[0].request),
              base::test::IsJson(R"({
                "token": "test_token",
                "tokenBindingRegistrationJwt": "token_binding_assertion",
                "upgradeType": "DARK_LAUNCH_BIND_TO_KEY"
              })"));
}

TEST_F(OAuth2UpgradeTokenFlowTest, AssertionGenerationFailure) {
  base::test::TestFuture<void> future;
  OAuth2UpgradeTokenFlow flow(
      "test_token", switches::RefreshTokenBindingUpgradeType::kLiveLaunch,
      "test_device_id", shared_factory_, future.GetCallback());
  flow.AbortWithError(OAuth2UpgradeTokenFlowResult::kTokenGenerationFailure);

  EXPECT_TRUE(future.Wait());
  histogram_tester_.ExpectUniqueSample(
      "Signin.TokenBinding.UpgradeResult",
      OAuth2UpgradeTokenFlowResult::kTokenGenerationFailure, 1);
  histogram_tester_.ExpectTotalCount("Signin.TokenBinding.UpgradeDuration", 1);
}

TEST_F(OAuth2UpgradeTokenFlowTest, Success) {
  AddFetchResult(GaiaUrls::GetInstance()->oauth2_upgrade_token_url(),
                 /*fetch_succeeds=*/true, net::HTTP_OK, "");

  base::test::TestFuture<void> future;
  OAuth2UpgradeTokenFlow flow(
      "test_token", switches::RefreshTokenBindingUpgradeType::kLiveLaunch,
      "test_device_id", shared_factory_, future.GetCallback());
  flow.StartWithRegistrationToken("token_binding_assertion");

  EXPECT_TRUE(future.Wait());
  histogram_tester_.ExpectUniqueSample("Signin.TokenBinding.UpgradeHttpResult",
                                       net::HTTP_OK, 1);
  histogram_tester_.ExpectUniqueSample("Signin.TokenBinding.UpgradeResult",
                                       OAuth2UpgradeTokenFlowResult::kSuccess,
                                       1);
  histogram_tester_.ExpectTotalCount("Signin.TokenBinding.UpgradeDuration", 1);
}

TEST_F(OAuth2UpgradeTokenFlowTest, NetworkError) {
  AddFetchResult(GaiaUrls::GetInstance()->oauth2_upgrade_token_url(),
                 /*fetch_succeeds=*/false, net::HTTP_OK, "");

  base::test::TestFuture<void> future;
  OAuth2UpgradeTokenFlow flow(
      "test_token", switches::RefreshTokenBindingUpgradeType::kLiveLaunch,
      "test_device_id", shared_factory_, future.GetCallback());
  flow.StartWithRegistrationToken("token_binding_assertion");

  EXPECT_TRUE(future.Wait());
  histogram_tester_.ExpectUniqueSample("Signin.TokenBinding.UpgradeHttpResult",
                                       net::ERR_FAILED, 1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.TokenBinding.UpgradeResult",
      OAuth2UpgradeTokenFlowResult::kNetworkError, 1);
  histogram_tester_.ExpectTotalCount("Signin.TokenBinding.UpgradeDuration", 1);
}

TEST_F(OAuth2UpgradeTokenFlowTest, HttpError) {
  AddFetchResult(GaiaUrls::GetInstance()->oauth2_upgrade_token_url(),
                 /*fetch_succeeds=*/true, net::HTTP_UNAUTHORIZED, "");

  base::test::TestFuture<void> future;
  OAuth2UpgradeTokenFlow flow(
      "test_token", switches::RefreshTokenBindingUpgradeType::kLiveLaunch,
      "test_device_id", shared_factory_, future.GetCallback());
  flow.StartWithRegistrationToken("token_binding_assertion");

  EXPECT_TRUE(future.Wait());
  histogram_tester_.ExpectUniqueSample("Signin.TokenBinding.UpgradeHttpResult",
                                       net::HTTP_UNAUTHORIZED, 1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.TokenBinding.UpgradeResult",
      OAuth2UpgradeTokenFlowResult::kServerError, 1);
  histogram_tester_.ExpectTotalCount("Signin.TokenBinding.UpgradeDuration", 1);
}

}  // namespace signin
