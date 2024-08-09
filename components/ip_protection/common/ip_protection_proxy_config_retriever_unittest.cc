// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_config_retriever.h"

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/ip_protection/get_proxy_config.pb.h"
#include "google_apis/common/api_key_request_test_util.h"
#include "net/base/features.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

class IpProtectionProxyConfigRetrieverTest : public testing::Test {
 protected:
  void SetUp() override {
    http_fetcher_ = std::make_unique<IpProtectionProxyConfigRetriever>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        "test_service_type", "test_api_key");
    token_server_get_proxy_config_url_ = GURL(base::StrCat(
        {net::features::kIpPrivacyTokenServer.Get(),
         net::features::kIpPrivacyTokenServerGetProxyConfigPath.Get()}));
    ASSERT_TRUE(token_server_get_proxy_config_url_.is_valid());
  }
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IpProtectionProxyConfigRetriever> http_fetcher_;
  GURL token_server_get_proxy_config_url_;
};

TEST_F(IpProtectionProxyConfigRetrieverTest, GetProxyConfigSuccess) {
  std::map<std::string, std::string> parameters;
  parameters["IpPrivacyDebugExperimentArm"] = "42";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));
  ip_protection::GetProxyConfigResponse response_proto;

  ip_protection::GetProxyConfigResponse_ProxyChain* proxyChain =
      response_proto.add_proxy_chain();
  proxyChain->set_proxy_a("proxyA");
  proxyChain->set_proxy_b("proxyB");
  std::string response_str = response_proto.SerializeAsString();

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ASSERT_TRUE(request.url.is_valid());
        ASSERT_EQ(request.url, token_server_get_proxy_config_url_);

        EXPECT_FALSE(
            request.headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
        EXPECT_TRUE(google_apis::test_util::HasAPIKey(request));
        EXPECT_THAT(
            request.headers.GetHeader("Ip-Protection-Debug-Experiment-Arm"),
            testing::Optional(std::string("42")));

        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            token_server_get_proxy_config_url_, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::OK));
      }));

  base::test::TestFuture<
      base::expected<ip_protection::GetProxyConfigResponse, std::string>>
      result_future;
  http_fetcher_->GetProxyConfig(std::nullopt, result_future.GetCallback(),
                                /*for_testing=*/true);

  base::expected<ip_protection::GetProxyConfigResponse, std::string> result =
      result_future.Get();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("proxyA", result->proxy_chain().at(0).proxy_a());
  EXPECT_EQ("proxyB", result->proxy_chain().at(0).proxy_b());
}

TEST_F(IpProtectionProxyConfigRetrieverTest,
       GetProxyConfigSuccessWithOAuthToken) {
  ip_protection::GetProxyConfigResponse response_proto;
  std::string oauth_token = "token";

  ip_protection::GetProxyConfigResponse_ProxyChain* proxyChain =
      response_proto.add_proxy_chain();
  proxyChain->set_proxy_a("proxyA");
  proxyChain->set_proxy_b("proxyB");
  std::string response_str = response_proto.SerializeAsString();

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ASSERT_TRUE(request.url.is_valid());
        ASSERT_EQ(request.url, token_server_get_proxy_config_url_);

        EXPECT_TRUE(
            request.headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
        EXPECT_FALSE(google_apis::test_util::HasAPIKey(request));

        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            token_server_get_proxy_config_url_, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::OK));
      }));

  base::test::TestFuture<
      base::expected<ip_protection::GetProxyConfigResponse, std::string>>
      result_future;
  http_fetcher_->GetProxyConfig(oauth_token, result_future.GetCallback(),
                                /*for_testing=*/true);

  base::expected<ip_protection::GetProxyConfigResponse, std::string> result =
      result_future.Get();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("proxyA", result->proxy_chain().at(0).proxy_a());
  EXPECT_EQ("proxyB", result->proxy_chain().at(0).proxy_b());
}

TEST_F(IpProtectionProxyConfigRetrieverTest, GetProxyConfigEmpty) {
  ip_protection::GetProxyConfigResponse response_proto;
  std::string response_str = response_proto.SerializeAsString();

  auto head = network::mojom::URLResponseHead::New();
  test_url_loader_factory_.AddResponse(
      token_server_get_proxy_config_url_, std::move(head), response_str,
      network::URLLoaderCompletionStatus(net::OK));

  base::test::TestFuture<
      base::expected<ip_protection::GetProxyConfigResponse, std::string>>
      result_future;
  http_fetcher_->GetProxyConfig(std::nullopt, result_future.GetCallback(),
                                /*for_testing=*/true);

  base::expected<ip_protection::GetProxyConfigResponse, std::string> result =
      result_future.Get();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(0, result->proxy_chain_size());
}

TEST_F(IpProtectionProxyConfigRetrieverTest, GetProxyConfigFails) {
  auto head = network::mojom::URLResponseHead::New();

  test_url_loader_factory_.AddResponse(
      token_server_get_proxy_config_url_, std::move(head), "uhoh",
      network::URLLoaderCompletionStatus(net::HTTP_BAD_REQUEST));

  base::test::TestFuture<
      base::expected<ip_protection::GetProxyConfigResponse, std::string>>
      result_future;
  http_fetcher_->GetProxyConfig(std::nullopt, result_future.GetCallback(),
                                /*for_testing=*/true);

  base::expected<ip_protection::GetProxyConfigResponse, std::string> result =
      result_future.Get();

  ASSERT_FALSE(result.has_value());
}

}  // namespace ip_protection
