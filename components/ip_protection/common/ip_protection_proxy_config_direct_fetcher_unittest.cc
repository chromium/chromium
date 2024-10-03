// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_config_direct_fetcher.h"

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/ip_protection/common/ip_protection_core_host_helper.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/get_proxy_config.pb.h"
#include "net/base/features.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom-shared.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

namespace {

constexpr char kServiceType[] = "test_service_type";

const GeoHint kGeoHint = {.country_code = "US",
                          .iso_region = "US-AL",
                          .city_name = "ALABASTER"};
class MockIpProtectionProxyConfigRetriever
    : public IpProtectionProxyConfigDirectFetcher::Retriever {
 public:
  using MockGetProxyConfig = base::RepeatingCallback<
      base::expected<GetProxyConfigResponse, std::string>()>;
  // Construct a mock retriever that will call the given closure for each call
  // to GetProxyConfig.
  explicit MockIpProtectionProxyConfigRetriever(
      MockGetProxyConfig get_proxy_config)
      : IpProtectionProxyConfigDirectFetcher::Retriever(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>(),
            kServiceType,
            IpProtectionProxyConfigDirectFetcher::AuthenticateCallback()),
        get_proxy_config_(get_proxy_config) {}

  // Construct a mock retriever that always returns the same response.
  explicit MockIpProtectionProxyConfigRetriever(
      std::optional<GetProxyConfigResponse> proxy_config_response)
      : MockIpProtectionProxyConfigRetriever(base::BindLambdaForTesting(
            [proxy_config_response = std::move(proxy_config_response)]()
                -> base::expected<GetProxyConfigResponse, std::string> {
              if (!proxy_config_response.has_value()) {
                return base::unexpected("uhoh");
              }
              return base::ok(*proxy_config_response);
            })) {}

  void RetrieveProxyConfig(
      IpProtectionProxyConfigDirectFetcher::Retriever::RetrieveCallback
          callback) override {
    std::move(callback).Run(get_proxy_config_.Run());
  }

 private:
  MockGetProxyConfig get_proxy_config_;
};

}  // namespace

class IpProtectionProxyConfigDirectFetcherRetrieverTest : public testing::Test {
 protected:
  void SetUp() override {
    retriever_ =
        std::make_unique<IpProtectionProxyConfigDirectFetcher::Retriever>(
            test_url_loader_factory_.GetSafeWeakWrapper(), "test_service_type",
            base::BindRepeating(
                &IpProtectionProxyConfigDirectFetcherRetrieverTest::
                    AuthenticateCallback,
                base::Unretained(this)));
    token_server_get_proxy_config_url_ = GURL(base::StrCat(
        {net::features::kIpPrivacyTokenServer.Get(),
         net::features::kIpPrivacyTokenServerGetProxyConfigPath.Get()}));
    ASSERT_TRUE(token_server_get_proxy_config_url_.is_valid());
  }

  void AuthenticateCallback(
      std::unique_ptr<network::ResourceRequest> resource_request,
      IpProtectionProxyConfigDirectFetcher::AuthenticateDoneCallback callback) {
    resource_request->headers.SetHeader("AuthenticationHeader", "Added");
    std::move(callback).Run(authenticate_callback_result_,
                            std::move(resource_request));
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IpProtectionProxyConfigDirectFetcher::Retriever> retriever_;
  GURL token_server_get_proxy_config_url_;
  bool authenticate_callback_result_ = true;
};

TEST_F(IpProtectionProxyConfigDirectFetcherRetrieverTest,
       GetProxyConfigSuccess) {
  std::map<std::string, std::string> parameters;
  parameters["IpPrivacyDebugExperimentArm"] = "42";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));
  GetProxyConfigResponse response_proto;

  GetProxyConfigResponse_ProxyChain* proxyChain =
      response_proto.add_proxy_chain();
  proxyChain->set_proxy_a("proxyA");
  proxyChain->set_proxy_b("proxyB");
  std::string response_str = response_proto.SerializeAsString();

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ASSERT_TRUE(request.url.is_valid());
        ASSERT_EQ(request.url, token_server_get_proxy_config_url_);

        EXPECT_THAT(request.headers.GetHeader("AuthenticationHeader"),
                    testing::Optional(std::string("Added")));
        EXPECT_THAT(
            request.headers.GetHeader("Ip-Protection-Debug-Experiment-Arm"),
            testing::Optional(std::string("42")));

        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            token_server_get_proxy_config_url_, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::OK));
      }));

  base::test::TestFuture<base::expected<GetProxyConfigResponse, std::string>>
      result_future;
  retriever_->RetrieveProxyConfig(result_future.GetCallback());

  base::expected<GetProxyConfigResponse, std::string> result =
      result_future.Get();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("proxyA", result->proxy_chain().at(0).proxy_a());
  EXPECT_EQ("proxyB", result->proxy_chain().at(0).proxy_b());
}

TEST_F(IpProtectionProxyConfigDirectFetcherRetrieverTest,
       GetProxyConfigAuthCallbackFails) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  authenticate_callback_result_ = false;
  test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
      [&](const network::ResourceRequest& request) { FAIL(); }));

  base::test::TestFuture<base::expected<GetProxyConfigResponse, std::string>>
      result_future;
  retriever_->RetrieveProxyConfig(result_future.GetCallback());

  base::expected<GetProxyConfigResponse, std::string> result =
      result_future.Get();

  ASSERT_FALSE(result.has_value());
}

TEST_F(IpProtectionProxyConfigDirectFetcherRetrieverTest, GetProxyConfigEmpty) {
  GetProxyConfigResponse response_proto;
  std::string response_str = response_proto.SerializeAsString();

  auto head = network::mojom::URLResponseHead::New();
  test_url_loader_factory_.AddResponse(
      token_server_get_proxy_config_url_, std::move(head), response_str,
      network::URLLoaderCompletionStatus(net::OK));

  base::test::TestFuture<base::expected<GetProxyConfigResponse, std::string>>
      result_future;
  retriever_->RetrieveProxyConfig(result_future.GetCallback());

  base::expected<GetProxyConfigResponse, std::string> result =
      result_future.Get();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(0, result->proxy_chain_size());
}

TEST_F(IpProtectionProxyConfigDirectFetcherRetrieverTest, GetProxyConfigFails) {
  auto head = network::mojom::URLResponseHead::New();

  test_url_loader_factory_.AddResponse(
      token_server_get_proxy_config_url_, std::move(head), "uhoh",
      network::URLLoaderCompletionStatus(net::HTTP_BAD_REQUEST));

  base::test::TestFuture<base::expected<GetProxyConfigResponse, std::string>>
      result_future;
  retriever_->RetrieveProxyConfig(result_future.GetCallback());

  base::expected<GetProxyConfigResponse, std::string> result =
      result_future.Get();

  ASSERT_FALSE(result.has_value());
}

class IpProtectionProxyConfigDirectFetcherTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::TestFuture<const std::optional<std::vector<net::ProxyChain>>&,
                         const std::optional<GeoHint>&>
      proxy_list_future_;
};

TEST_F(IpProtectionProxyConfigDirectFetcherTest, GetProxyConfigProxyChains) {
  GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy1");
  chain->set_proxy_b("proxy1b");
  chain->set_chain_id(1);
  chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy2");
  chain->set_proxy_b("proxy2b");
  chain->set_chain_id(2);

  response.mutable_geo_hint()->set_country_code(kGeoHint.country_code);
  response.mutable_geo_hint()->set_iso_region(kGeoHint.iso_region);
  response.mutable_geo_hint()->set_city_name(kGeoHint.city_name);

  IpProtectionProxyConfigDirectFetcher fetcher(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response));

  fetcher.GetProxyConfig(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyConfig did not call back";

  std::vector<net::ProxyChain> exp_proxy_list = {
      IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"proxy1", "proxy1b"}, 1),
      IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"proxy2", "proxy2b"}, 2)};

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(), testing::ElementsAreArray(exp_proxy_list));

  ASSERT_TRUE(geo_hint);  // Check that GeoHintPtr is not null.
  EXPECT_TRUE(geo_hint == kGeoHint);
}

TEST_F(IpProtectionProxyConfigDirectFetcherTest,
       GetProxyConfigProxyChainsWithPorts) {
  GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy1");
  chain->set_proxy_b("proxy1b");
  chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy2:80");
  chain->set_proxy_b("proxy2");
  chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy3:0");
  chain->set_proxy_b("proxy4:443");
  chain->set_chain_id(3);

  response.mutable_geo_hint()->set_country_code(kGeoHint.country_code);
  response.mutable_geo_hint()->set_iso_region(kGeoHint.iso_region);
  response.mutable_geo_hint()->set_city_name(kGeoHint.city_name);

  IpProtectionProxyConfigDirectFetcher fetcher(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response));

  fetcher.GetProxyConfig(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyConfig did not call back";

  std::vector<net::ProxyChain> exp_proxy_list = {
      IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"proxy1", "proxy1b"})};
  exp_proxy_list.push_back(net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxy2", 80),
       net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxy2", std::nullopt)}));
  exp_proxy_list.push_back(net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxy3", "0"),
       net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxy4", "443")},
      3));

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(), testing::ElementsAreArray(exp_proxy_list));

  ASSERT_TRUE(geo_hint);  // Check that GeoHint is not null.
  EXPECT_TRUE(geo_hint == kGeoHint);
}

TEST_F(IpProtectionProxyConfigDirectFetcherTest, GetProxyConfigProxyInvalid) {
  GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("]INVALID[");
  chain->set_proxy_b("not-invalid");
  chain = response.add_proxy_chain();
  chain->set_proxy_a("valid");
  chain->set_proxy_b("valid");

  response.mutable_geo_hint()->set_country_code(kGeoHint.country_code);
  response.mutable_geo_hint()->set_iso_region(kGeoHint.iso_region);
  response.mutable_geo_hint()->set_city_name(kGeoHint.city_name);

  IpProtectionProxyConfigDirectFetcher fetcher(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response));

  fetcher.GetProxyConfig(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyConfig did not call back";

  std::vector<net::ProxyChain> exp_proxy_list = {
      IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"valid", "valid"})};

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(), testing::ElementsAreArray(exp_proxy_list));

  ASSERT_TRUE(geo_hint);  // Check that GeoHint is not null.
  EXPECT_TRUE(geo_hint == kGeoHint);
}

TEST_F(IpProtectionProxyConfigDirectFetcherTest,
       GetProxyConfigProxyInvalidChainId) {
  GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxya");
  chain->set_proxy_b("proxyb");
  chain->set_chain_id(999);

  response.mutable_geo_hint()->set_country_code(kGeoHint.country_code);
  response.mutable_geo_hint()->set_iso_region(kGeoHint.iso_region);
  response.mutable_geo_hint()->set_city_name(kGeoHint.city_name);

  IpProtectionProxyConfigDirectFetcher fetcher(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response));

  fetcher.GetProxyConfig(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyConfig did not call back";

  // The proxy chain is still used, but the chain ID is set to the default.
  std::vector<net::ProxyChain> exp_proxy_list = {
      IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"proxya", "proxyb"}, net::ProxyChain::kDefaultIpProtectionChainId)};

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(), testing::ElementsAreArray(exp_proxy_list));

  ASSERT_TRUE(geo_hint);  // Check that GeoHint is not null.
  EXPECT_TRUE(geo_hint == kGeoHint);
}

TEST_F(IpProtectionProxyConfigDirectFetcherTest,
       GetProxyConfigProxyCountryLevelGeo) {
  GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy1");
  chain->set_proxy_b("proxy1b");
  chain->set_chain_id(1);
  chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy2");
  chain->set_proxy_b("proxy2b");
  chain->set_chain_id(2);

  // Geo is only country level.
  response.mutable_geo_hint()->set_country_code("US");

  IpProtectionProxyConfigDirectFetcher fetcher(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response));

  fetcher.GetProxyConfig(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyConfig did not call back";

  std::vector<net::ProxyChain> exp_proxy_list = {
      IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"proxy1", "proxy1b"}, 1),
      IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"proxy2", "proxy2b"}, 2)};

  // Country level geo only.
  GeoHint exp_geo_hint;
  exp_geo_hint.country_code = "US";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(), testing::ElementsAreArray(exp_proxy_list));

  ASSERT_TRUE(geo_hint);  // Check that GeoHint is not null.
  EXPECT_TRUE(geo_hint == exp_geo_hint);
}

TEST_F(IpProtectionProxyConfigDirectFetcherTest,
       GetProxyConfigProxyGeoMissingFailure) {
  // The error case in this situation should be a valid response with a missing
  // geo hint and non-empty proxy chain vector.
  GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy1");
  chain->set_proxy_b("proxy1b");
  chain->set_chain_id(1);
  chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy2");
  chain->set_proxy_b("proxy2b");
  chain->set_chain_id(2);

  IpProtectionProxyConfigDirectFetcher fetcher(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response));

  fetcher.GetProxyConfig(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyConfig did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  // A failure means both of these values will be null.
  EXPECT_EQ(proxy_list, std::nullopt);
  EXPECT_FALSE(geo_hint.has_value());
}

}  // namespace ip_protection
