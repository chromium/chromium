// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/ip_protection_proxy_config_fetcher.h"

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/ip_protection/ip_protection_config_provider_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

namespace {

constexpr char kServiceType[] = "test_service_type";
constexpr char kApiKey[] = "test_api_key";

class MockIpProtectionProxyConfigRetriever
    : public IpProtectionProxyConfigRetriever {
 public:
  using MockGetProxyConfig = base::RepeatingCallback<
      base::expected<ip_protection::GetProxyConfigResponse, std::string>()>;
  // Construct a mock retriever that will call the given closure for each call
  // to GetProxyConfig.
  explicit MockIpProtectionProxyConfigRetriever(
      MockGetProxyConfig get_proxy_config)
      : IpProtectionProxyConfigRetriever(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>(),
            kServiceType,
            kApiKey),
        get_proxy_config_(get_proxy_config) {}

  // Construct a mock retriever that always returns the same response.
  explicit MockIpProtectionProxyConfigRetriever(
      std::optional<ip_protection::GetProxyConfigResponse>
          proxy_config_response)
      : MockIpProtectionProxyConfigRetriever(base::BindLambdaForTesting(
            [proxy_config_response = std::move(proxy_config_response)]()
                -> base::expected<ip_protection::GetProxyConfigResponse,
                                  std::string> {
              if (!proxy_config_response.has_value()) {
                return base::unexpected("uhoh");
              }
              return base::ok(*proxy_config_response);
            })) {}

  void GetProxyConfig(
      std::optional<std::string> oauth_token,
      IpProtectionProxyConfigRetriever::GetProxyConfigCallback callback,
      bool for_testing = false) override {
    std::move(callback).Run(get_proxy_config_.Run());
  }

 private:
  MockGetProxyConfig get_proxy_config_;
};
}  // namespace

class IpProtectionProxyConfigFetcherTest : public testing::Test {
 protected:
  IpProtectionProxyConfigFetcherTest() {}

  void SetUp() override {
    geo_hint_ = network::mojom::GeoHint::New("US", "US-AL", "ALABASTER");

    fetcher_ = std::make_unique<IpProtectionProxyConfigFetcher>(
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(),
        kServiceType, kApiKey);
  }

  void TearDown() override {}

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<IpProtectionProxyConfigFetcher> fetcher_;
  base::test::TestFuture<const std::optional<std::vector<net::ProxyChain>>&,
                         network::mojom::GeoHintPtr>
      proxy_list_future_;

  // A convenient geo hint for fake tokens.
  network::mojom::GeoHintPtr geo_hint_;
};

TEST_F(IpProtectionProxyConfigFetcherTest, CallGetProxyConfigProxyChains) {
  ip_protection::GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy1");
  chain->set_proxy_b("proxy1b");
  chain->set_chain_id(1);
  chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy2");
  chain->set_proxy_b("proxy2b");
  chain->set_chain_id(2);

  response.mutable_geo_hint()->set_country_code(geo_hint_->country_code);
  response.mutable_geo_hint()->set_iso_region(geo_hint_->iso_region);
  response.mutable_geo_hint()->set_city_name(geo_hint_->city_name);

  fetcher_->SetUpForTesting(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response));

  fetcher_->CallGetProxyConfig(proxy_list_future_.GetCallback(),
                               /*oauth_token=*/std::nullopt);
  ASSERT_TRUE(proxy_list_future_.Wait())
      << "CallGetProxyConfig did not call back";

  std::vector<net::ProxyChain> exp_proxy_list = {
      IpProtectionProxyConfigFetcher::MakeChainForTesting({"proxy1", "proxy1b"},
                                                          1),
      IpProtectionProxyConfigFetcher::MakeChainForTesting({"proxy2", "proxy2b"},
                                                          2)};

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(), testing::ElementsAreArray(exp_proxy_list));

  ASSERT_TRUE(geo_hint);  // Check that GeoHintPtr is not null.
  EXPECT_TRUE(geo_hint->Equals(*geo_hint_));
}

TEST_F(IpProtectionProxyConfigFetcherTest,
       CallGetProxyConfigProxyChainsWithPorts) {
  ip_protection::GetProxyConfigResponse response;
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

  response.mutable_geo_hint()->set_country_code(geo_hint_->country_code);
  response.mutable_geo_hint()->set_iso_region(geo_hint_->iso_region);
  response.mutable_geo_hint()->set_city_name(geo_hint_->city_name);

  fetcher_->SetUpForTesting(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response));

  fetcher_->CallGetProxyConfig(proxy_list_future_.GetCallback(),
                               /*oauth_token=*/std::nullopt);
  ASSERT_TRUE(proxy_list_future_.Wait())
      << "CallGetProxyConfig did not call back";

  std::vector<net::ProxyChain> exp_proxy_list = {
      IpProtectionProxyConfigFetcher::MakeChainForTesting(
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

  ASSERT_TRUE(geo_hint);  // Check that GeoHintPtr is not null.
  EXPECT_TRUE(geo_hint->Equals(*geo_hint_));
}

TEST_F(IpProtectionProxyConfigFetcherTest, CallGetProxyConfigProxyInvalid) {
  ip_protection::GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("]INVALID[");
  chain->set_proxy_b("not-invalid");
  chain = response.add_proxy_chain();
  chain->set_proxy_a("valid");
  chain->set_proxy_b("valid");

  response.mutable_geo_hint()->set_country_code(geo_hint_->country_code);
  response.mutable_geo_hint()->set_iso_region(geo_hint_->iso_region);
  response.mutable_geo_hint()->set_city_name(geo_hint_->city_name);

  fetcher_->SetUpForTesting(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response));

  fetcher_->CallGetProxyConfig(proxy_list_future_.GetCallback(),
                               /*oauth_token=*/std::nullopt);
  ASSERT_TRUE(proxy_list_future_.Wait())
      << "CallGetProxyConfig did not call back";

  std::vector<net::ProxyChain> exp_proxy_list = {
      IpProtectionProxyConfigFetcher::MakeChainForTesting({"valid", "valid"})};

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(), testing::ElementsAreArray(exp_proxy_list));

  ASSERT_TRUE(geo_hint);  // Check that GeoHintPtr is not null.
  EXPECT_TRUE(geo_hint->Equals(*geo_hint_));
}

TEST_F(IpProtectionProxyConfigFetcherTest,
       CallGetProxyConfigProxyInvalidChainId) {
  ip_protection::GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxya");
  chain->set_proxy_b("proxyb");
  chain->set_chain_id(999);

  response.mutable_geo_hint()->set_country_code(geo_hint_->country_code);
  response.mutable_geo_hint()->set_iso_region(geo_hint_->iso_region);
  response.mutable_geo_hint()->set_city_name(geo_hint_->city_name);

  fetcher_->SetUpForTesting(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response));

  fetcher_->CallGetProxyConfig(proxy_list_future_.GetCallback(),
                               /*oauth_token=*/std::nullopt);
  ASSERT_TRUE(proxy_list_future_.Wait())
      << "CallGetProxyConfig did not call back";

  // The proxy chain is still used, but the chain ID is set to the default.
  std::vector<net::ProxyChain> exp_proxy_list = {
      IpProtectionProxyConfigFetcher::MakeChainForTesting(
          {"proxya", "proxyb"}, net::ProxyChain::kDefaultIpProtectionChainId)};

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(), testing::ElementsAreArray(exp_proxy_list));

  ASSERT_TRUE(geo_hint);  // Check that GeoHintPtr is not null.
  EXPECT_TRUE(geo_hint->Equals(*geo_hint_));
}

TEST_F(IpProtectionProxyConfigFetcherTest,
       CallGetProxyConfigProxyCountryLevelGeo) {
  ip_protection::GetProxyConfigResponse response;
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

  fetcher_->SetUpForTesting(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response));

  fetcher_->CallGetProxyConfig(proxy_list_future_.GetCallback(),
                               /*oauth_token=*/std::nullopt);
  ASSERT_TRUE(proxy_list_future_.Wait())
      << "CallGetProxyConfig did not call back";

  std::vector<net::ProxyChain> exp_proxy_list = {
      IpProtectionProxyConfigFetcher::MakeChainForTesting({"proxy1", "proxy1b"},
                                                          1),
      IpProtectionProxyConfigFetcher::MakeChainForTesting({"proxy2", "proxy2b"},
                                                          2)};

  // Country level geo only.
  auto exp_geo_hint = network::mojom::GeoHint::New("US", "", "");

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(), testing::ElementsAreArray(exp_proxy_list));

  ASSERT_TRUE(geo_hint);  // Check that GeoHintPtr is not null.
  EXPECT_TRUE(geo_hint->Equals(*exp_geo_hint));
}

TEST_F(IpProtectionProxyConfigFetcherTest,
       CallGetProxyConfigProxyGeoMissingFailure) {
  // The error case in this situation should be a valid response with a missing
  // geo hint and non-empty proxy chain vector.
  ip_protection::GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy1");
  chain->set_proxy_b("proxy1b");
  chain->set_chain_id(1);
  chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy2");
  chain->set_proxy_b("proxy2b");
  chain->set_chain_id(2);

  fetcher_->SetUpForTesting(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response));

  fetcher_->CallGetProxyConfig(proxy_list_future_.GetCallback(),
                               /*oauth_token=*/std::nullopt);
  ASSERT_TRUE(proxy_list_future_.Wait())
      << "CallGetProxyConfig did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  // A failure means both of these values will be null.
  EXPECT_EQ(proxy_list, std::nullopt);
  EXPECT_TRUE(geo_hint.is_null());
}

}  // namespace ip_protection
