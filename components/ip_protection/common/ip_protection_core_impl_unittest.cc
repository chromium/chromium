// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_core_impl.h"

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager_impl.h"
#include "components/ip_protection/common/ip_protection_token_manager.h"
#include "components/ip_protection/common/masked_domain_list_manager.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/features.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/proxy_config.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ip_protection {

namespace {
using ::masked_domain_list::MaskedDomainList;
using ::masked_domain_list::Resource;
using ::masked_domain_list::ResourceOwner;
using ::network::mojom::IpProtectionProxyBypassPolicy;

constexpr char kEmptyTokenCacheHistogram[] =
    "NetworkService.IpProtection.EmptyTokenCache2";
constexpr char kMdlMatchesTimeHistogram[] =
    "NetworkService.MaskedDomainList.MatchesTime";
constexpr char kQuicProxiesFailedHistogram[] =
    "NetworkService.IpProtection.QuicProxiesFailed";

constexpr char kMountainViewGeoId[] = "US,US-CA,MOUNTAIN VIEW";
constexpr char kSunnyvaleGeoId[] = "US,US-CA,SUNNYVALE";

class MockIpProtectionTokenManager : public IpProtectionTokenManager {
 public:
  bool IsAuthTokenAvailable(const std::string& geo_id) override {
    return auth_tokens_.contains(geo_id);
  }

  bool WasTokenCacheEverFilled() override {
    return was_token_cache_ever_filled_;
  }

  void InvalidateTryAgainAfterTime() override {}

  std::string CurrentGeo() const override { return current_geo_id_; }

  void SetCurrentGeo(const std::string& geo_id) override {
    current_geo_id_ = geo_id;
  }

  std::optional<BlindSignedAuthToken> GetAuthToken(
      const std::string& geo_id) override {
    if (!auth_tokens_.contains(geo_id)) {
      return std::nullopt;
    }

    return auth_tokens_.extract(geo_id).mapped();
  }

  void SetAuthToken(BlindSignedAuthToken auth_token) {
    was_token_cache_ever_filled_ = true;
    auth_tokens_[GetGeoIdFromGeoHint(auth_token.geo_hint)] = auth_token;
  }

  void RecordTokenDemand() override {}

 private:
  std::map<std::string, BlindSignedAuthToken> auth_tokens_;
  std::optional<BlindSignedAuthToken> auth_token_;
  std::string current_geo_id_;
  bool was_token_cache_ever_filled_ = false;
};

class MockIpProtectionProxyConfigManager
    : public IpProtectionProxyConfigManager {
 public:
  bool IsProxyListAvailable() override { return proxy_list_.has_value(); }

  const std::vector<net::ProxyChain>& ProxyList() override {
    return *proxy_list_;
  }

  const std::string& CurrentGeo() override { return geo_id_; }

  void RequestRefreshProxyList() override {
    if (on_force_refresh_proxy_list_) {
      if (!geo_id_to_change_on_refresh_.empty()) {
        geo_id_ = geo_id_to_change_on_refresh_;
      }
      std::move(on_force_refresh_proxy_list_).Run();
    }
  }

  // Set the proxy list returned from `ProxyList()`.
  void SetProxyList(std::vector<net::ProxyChain> proxy_list) {
    proxy_list_ = std::move(proxy_list);
  }

  void SetOnRequestRefreshProxyList(
      base::OnceClosure on_force_refresh_proxy_list,
      std::string geo_id = "") {
    geo_id_to_change_on_refresh_ = geo_id;
    on_force_refresh_proxy_list_ = std::move(on_force_refresh_proxy_list);
  }

  void SetCurrentGeo(const std::string& geo_id) { geo_id_ = geo_id; }

 private:
  std::optional<std::vector<net::ProxyChain>> proxy_list_;
  std::string geo_id_;
  std::string geo_id_to_change_on_refresh_;
  base::OnceClosure on_force_refresh_proxy_list_;
};

class IpProtectionCoreImplTest : public testing::Test {
 protected:
  IpProtectionCoreImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        network::features::kMaskedDomainList,
        {{network::features::kSplitMaskedDomainList.name,
          base::ToString(true)}});
  }

  std::unique_ptr<IpProtectionCoreImpl> MakeCore(
      IpProtectionCoreImpl::ProxyTokenManagerMap ip_protection_token_managers) {
    return std::make_unique<IpProtectionCoreImpl>(
        /*masked_domain_list_manager=*/nullptr,
        /*ip_protection_proxy_config_manager=*/nullptr,
        std::move(ip_protection_token_managers),
        /*is_ip_protection_enabled=*/true, /*ip_protection_incognito=*/true);
  }

  std::unique_ptr<IpProtectionCoreImpl> MakeCore(
      MaskedDomainListManager* masked_domain_list_manager,
      bool ip_protection_incognito = false) {
    return std::make_unique<IpProtectionCoreImpl>(
        masked_domain_list_manager,
        /*ip_protection_proxy_config_manager=*/nullptr,
        /*ip_protection_token_managers=*/
        IpProtectionCoreImpl::ProxyTokenManagerMap(),
        /*is_ip_protection_enabled=*/true,
        /*ip_protection_incognito=*/ip_protection_incognito);
  }

  std::unique_ptr<IpProtectionCoreImpl> MakeCore(
      std::unique_ptr<IpProtectionProxyConfigManager>
          ip_protection_proxy_config_manager,
      IpProtectionCoreImpl::ProxyTokenManagerMap ip_protection_token_managers =
          {}) {
    return std::make_unique<IpProtectionCoreImpl>(
        /*masked_domain_list_manager=*/nullptr,
        std::move(ip_protection_proxy_config_manager),
        std::move(ip_protection_token_managers),
        /*is_ip_protection_enabled=*/true, /*ip_protection_incognito=*/true);
  }

  // Shortcut to create a ProxyChain from hostnames.
  net::ProxyChain MakeChain(std::vector<std::string> hostnames) {
    std::vector<net::ProxyServer> servers;
    for (auto& hostname : hostnames) {
      servers.push_back(net::ProxyServer::FromSchemeHostAndPort(
          net::ProxyServer::SCHEME_HTTPS, hostname, std::nullopt));
    }
    return net::ProxyChain::ForIpProtection(servers);
  }

  ContentSettingsForOneType CreateSetting(const std::string& first_party_url,
                                          ContentSetting setting) {
    content_settings::RuleMetaData metadata;
    metadata.SetExpirationAndLifetime(base::Time(), base::TimeDelta());

    return {ContentSettingPatternSource(
        ContentSettingsPattern::Wildcard(),
        ContentSettingsPattern::FromString(first_party_url),
        base::Value(setting), content_settings::ProviderType::kNone,
        /*incognito=*/true, std::move(metadata))};
  }

  base::HistogramTester histogram_tester_;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Verify that a TRACKING PROTECTION exception is created for a given url.
TEST_F(IpProtectionCoreImplTest, TrackingProtectionExceptionAddedAndRetrieved) {
  const std::string kUrl = "https://example.com";
  auto masked_domain_list_manager =
      MaskedDomainListManager(IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainList mdl = masked_domain_list::MaskedDomainList();
  masked_domain_list_manager.UpdateMaskedDomainListForTesting(mdl);
  auto ip_protection_core =
      MakeCore(&masked_domain_list_manager, /*ip_protection_incognito=*/true);

  EXPECT_FALSE(ip_protection_core->HasTrackingProtectionException(GURL(kUrl)));

  ip_protection_core->SetTrackingProtectionContentSetting(
      CreateSetting(kUrl, CONTENT_SETTING_ALLOW));

  EXPECT_TRUE(ip_protection_core->HasTrackingProtectionException(GURL(kUrl)));
}

// Verify that RequestShouldBeProxied measures the time taken to call Matches().
TEST_F(IpProtectionCoreImplTest, RequestShouldBeProxiedMeasured) {
  auto masked_domain_list_manager =
      MaskedDomainListManager(IpProtectionProxyBypassPolicy::kNone);
  auto ip_protection_core = MakeCore(&masked_domain_list_manager);
  ip_protection_core->RequestShouldBeProxied(GURL(),
                                             net::NetworkAnonymizationKey());
  histogram_tester_.ExpectTotalCount(kMdlMatchesTimeHistogram, 1);
}

TEST_F(IpProtectionCoreImplTest, AreAuthTokensAvailable_NoProxiesConfigured) {
  // A proxy list is available. This should ensure that the only reason tokens
  // are not available is due to a lack of token cache managers.
  auto ipp_proxy_config_manager =
      std::make_unique<MockIpProtectionProxyConfigManager>();
  ipp_proxy_config_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_config_manager->SetCurrentGeo(kMountainViewGeoId);

  auto ip_protection_core = MakeCore(std::move(ipp_proxy_config_manager));

  ASSERT_FALSE(ip_protection_core->WereTokenCachesEverFilled());
  ASSERT_FALSE(ip_protection_core->AreAuthTokensAvailable());
}

TEST_F(IpProtectionCoreImplTest,
       AuthTokensNotAvailableIfProxyListIsNotAvailable) {
  // `IpProtectionProxyConfigManager` has not been set up. This means even if
  // tokens are available, `AreAuthTokensAvailable()` should return false.
  BlindSignedAuthToken exp_token;
  exp_token.token = "a-token";
  exp_token.geo_hint =
      GetGeoHintFromGeoIdForTesting(kMountainViewGeoId).value();
  auto ipp_token_manager = std::make_unique<MockIpProtectionTokenManager>();
  ipp_token_manager->SetAuthToken(std::move(exp_token));

  IpProtectionCoreImpl::ProxyTokenManagerMap managers;
  managers.insert({ProxyLayer::kProxyA, std::move(ipp_token_manager)});
  auto ip_protection_core = MakeCore(std::move(managers));

  ASSERT_FALSE(ip_protection_core->WereTokenCachesEverFilled());
  ASSERT_FALSE(ip_protection_core->AreAuthTokensAvailable());
  // Neither calls will return a token since there is no proxy list available.
  ASSERT_FALSE(ip_protection_core->GetAuthToken(0).has_value());
  ASSERT_FALSE(ip_protection_core->GetAuthToken(1).has_value());
}

// Token cache manager returns available token for proxyA.
TEST_F(IpProtectionCoreImplTest, GetAuthTokenFromManagerForProxyA) {
  auto ipp_proxy_config_manager =
      std::make_unique<MockIpProtectionProxyConfigManager>();
  ipp_proxy_config_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_config_manager->SetCurrentGeo(kMountainViewGeoId);

  auto ipp_token_manager = std::make_unique<MockIpProtectionTokenManager>();
  ipp_token_manager->SetAuthToken(BlindSignedAuthToken{
      .token = "a-token",
      .geo_hint = GetGeoHintFromGeoIdForTesting(kMountainViewGeoId).value()});

  IpProtectionCoreImpl::ProxyTokenManagerMap managers;
  managers.insert({ProxyLayer::kProxyA, std::move(ipp_token_manager)});
  auto ip_protection_core =
      MakeCore(std::move(ipp_proxy_config_manager), std::move(managers));

  ASSERT_TRUE(ip_protection_core->WereTokenCachesEverFilled());
  ASSERT_TRUE(ip_protection_core->AreAuthTokensAvailable());
  ASSERT_FALSE(ip_protection_core->GetAuthToken(1)
                   .has_value());  // ProxyB has no tokens.
  ASSERT_TRUE(ip_protection_core->GetAuthToken(0));
}

// Token cache manager returns available token for proxyB.
TEST_F(IpProtectionCoreImplTest, GetAuthTokenFromManagerForProxyB) {
  auto ipp_proxy_config_manager =
      std::make_unique<MockIpProtectionProxyConfigManager>();
  ipp_proxy_config_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_config_manager->SetCurrentGeo(kMountainViewGeoId);

  BlindSignedAuthToken exp_token;
  exp_token.token = "b-token";
  exp_token.geo_hint =
      GetGeoHintFromGeoIdForTesting(kMountainViewGeoId).value();
  auto ipp_token_manager = std::make_unique<MockIpProtectionTokenManager>();
  ipp_token_manager->SetAuthToken(std::move(exp_token));

  IpProtectionCoreImpl::ProxyTokenManagerMap managers;
  managers.insert({ProxyLayer::kProxyB, std::move(ipp_token_manager)});
  auto ip_protection_core =
      MakeCore(std::move(ipp_proxy_config_manager), std::move(managers));

  ASSERT_TRUE(ip_protection_core->WereTokenCachesEverFilled());
  ASSERT_TRUE(ip_protection_core->AreAuthTokensAvailable());
  ASSERT_FALSE(ip_protection_core->GetAuthToken(0)
                   .has_value());  // ProxyA has no tokens.
  ASSERT_TRUE(ip_protection_core->GetAuthToken(1));
}

TEST_F(IpProtectionCoreImplTest,
       AreAuthTokensAvailable_OneTokenCacheNeverFilled_ReturnsFalse) {
  auto ipp_proxy_config_manager =
      std::make_unique<MockIpProtectionProxyConfigManager>();
  ipp_proxy_config_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_config_manager->SetCurrentGeo(kMountainViewGeoId);

  auto token_manager = std::make_unique<MockIpProtectionTokenManager>();
  token_manager->SetAuthToken(BlindSignedAuthToken{
      .token = "secret-token",
      .geo_hint = GetGeoHintFromGeoIdForTesting(kMountainViewGeoId).value()});

  IpProtectionCoreImpl::ProxyTokenManagerMap managers;
  managers.insert({ProxyLayer::kProxyA, std::move(token_manager)});
  managers.insert(
      {ProxyLayer::kProxyB, std::make_unique<MockIpProtectionTokenManager>()});
  auto ip_protection_core =
      MakeCore(std::move(ipp_proxy_config_manager), std::move(managers));

  ASSERT_FALSE(ip_protection_core->WereTokenCachesEverFilled());
  ASSERT_FALSE(ip_protection_core->AreAuthTokensAvailable());
  // The empty token cache metric should not be emitted since the cache was
  // never filled.
  histogram_tester_.ExpectTotalCount(kEmptyTokenCacheHistogram, 0);
}

TEST_F(IpProtectionCoreImplTest,
       AreAuthTokensAvailable_OneTokenCacheExhausted_ReturnsFalse) {
  auto ipp_proxy_config_manager =
      std::make_unique<MockIpProtectionProxyConfigManager>();
  ipp_proxy_config_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_config_manager->SetCurrentGeo(kMountainViewGeoId);

  // Create two token managers, both with one token.
  IpProtectionCoreImpl::ProxyTokenManagerMap managers;
  for (auto proxy_layer : {ProxyLayer::kProxyA, ProxyLayer::kProxyB}) {
    auto token_manager = std::make_unique<MockIpProtectionTokenManager>();
    token_manager->SetAuthToken(BlindSignedAuthToken{
        .token = "secret-token",
        .geo_hint = GetGeoHintFromGeoIdForTesting(kMountainViewGeoId).value()});
    managers.insert({proxy_layer, std::move(token_manager)});
  }

  auto ip_protection_core =
      MakeCore(std::move(ipp_proxy_config_manager), std::move(managers));

  // Exhaust the token for ProxyA.
  ASSERT_TRUE(ip_protection_core->GetAuthToken(0));

  ASSERT_TRUE(ip_protection_core->WereTokenCachesEverFilled());

  // The token for ProxyA is exhausted, so `AreAuthTokensAvailable()` should
  // return false.
  ASSERT_FALSE(ip_protection_core->AreAuthTokensAvailable());
  histogram_tester_.ExpectTotalCount(kEmptyTokenCacheHistogram, 1);
  histogram_tester_.ExpectBucketCount(kEmptyTokenCacheHistogram,
                                      ProxyLayer::kProxyA, 1);
}

// GetAuthToken for where proxy list manager's geo is different than the current
// geo of the config cache.
TEST_F(IpProtectionCoreImplTest, GetAuthTokenForOldGeo) {
  auto ipp_proxy_config_manager =
      std::make_unique<MockIpProtectionProxyConfigManager>();
  ipp_proxy_config_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_config_manager->SetCurrentGeo(kMountainViewGeoId);

  // The token cache manager will contain a token from the "old" mountain view
  // geo and a new sunnyvale geo.
  auto ipp_token_manager = std::make_unique<MockIpProtectionTokenManager>();
  ipp_token_manager->SetAuthToken(BlindSignedAuthToken{
      .token = "a-token",
      .geo_hint = GetGeoHintFromGeoIdForTesting(kMountainViewGeoId).value()});
  ipp_token_manager->SetAuthToken(BlindSignedAuthToken{
      .token = "a-token",
      .geo_hint = GetGeoHintFromGeoIdForTesting(kSunnyvaleGeoId).value()});

  IpProtectionCoreImpl::ProxyTokenManagerMap managers;
  managers.insert({ProxyLayer::kProxyA, std::move(ipp_token_manager)});
  auto ip_protection_core =
      MakeCore(std::move(ipp_proxy_config_manager), std::move(managers));

  // The following calls will be based on the proxy list manager's geo (Mountain
  // View).
  ASSERT_TRUE(ip_protection_core->WereTokenCachesEverFilled());
  ASSERT_TRUE(ip_protection_core->AreAuthTokensAvailable());
  std::optional<BlindSignedAuthToken> token =
      ip_protection_core->GetAuthToken(0);
  ASSERT_TRUE(token);
  ASSERT_EQ(token->geo_hint, GetGeoHintFromGeoIdForTesting(kMountainViewGeoId));
}

// Proxy list manager returns currently cached proxy hostnames.
TEST_F(IpProtectionCoreImplTest, GetProxyListFromManager) {
  std::string proxy = "a-proxy";
  auto ip_protection_proxy_chain =
      net::ProxyChain::ForIpProtection({net::ProxyServer::FromSchemeHostAndPort(
          net::ProxyServer::SCHEME_HTTPS, proxy, std::nullopt)});
  const std::vector<net::ProxyChain> proxy_chain_list = {
      std::move(ip_protection_proxy_chain)};
  auto ipp_proxy_config_manager =
      std::make_unique<MockIpProtectionProxyConfigManager>();
  ipp_proxy_config_manager->SetProxyList({MakeChain({proxy})});
  auto ip_protection_core = MakeCore(std::move(ipp_proxy_config_manager));

  ASSERT_TRUE(ip_protection_core->IsProxyListAvailable());
  EXPECT_EQ(ip_protection_core->GetProxyChainList(), proxy_chain_list);
}

// When QUIC proxies are enabled, the proxy list has both QUIC and HTTPS
// proxies, and falls back properly when a QUIC proxy fails.
TEST_F(IpProtectionCoreImplTest, GetProxyListFromManagerWithQuic) {
  std::map<std::string, std::string> parameters;
  parameters[net::features::kIpPrivacyUseQuicProxies.name] = "true";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier =
      net::NetworkChangeNotifier::CreateMockIfNeeded();

  auto ipp_proxy_config_manager =
      std::make_unique<MockIpProtectionProxyConfigManager>();
  ipp_proxy_config_manager->SetProxyList({MakeChain({"a-proxy1", "b-proxy1"}),
                                          MakeChain({"a-proxy2", "b-proxy2"})});
  auto ip_protection_core = MakeCore(std::move(ipp_proxy_config_manager));

  const std::vector<net::ProxyChain> proxy_chain_list_with_quic = {
      net::ProxyChain::ForIpProtection({
          net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_QUIC,
                                                  "a-proxy1", std::nullopt),
          net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_QUIC,
                                                  "b-proxy1", std::nullopt),
      }),
      net::ProxyChain::ForIpProtection({
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "a-proxy1", std::nullopt),
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "b-proxy1", std::nullopt),
      }),
      net::ProxyChain::ForIpProtection({
          net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_QUIC,
                                                  "a-proxy2", std::nullopt),
          net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_QUIC,
                                                  "b-proxy2", std::nullopt),
      })};

  const std::vector<net::ProxyChain> proxy_chain_list_without_quic = {
      net::ProxyChain::ForIpProtection({
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "a-proxy1", std::nullopt),
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "b-proxy1", std::nullopt),
      }),
      net::ProxyChain::ForIpProtection({
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "a-proxy2", std::nullopt),
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "b-proxy2", std::nullopt),
      })};
  ASSERT_TRUE(ip_protection_core->IsProxyListAvailable());

  // Call GetProxyChainList three times to test counting requests before
  // failure.
  EXPECT_EQ(ip_protection_core->GetProxyChainList(),
            proxy_chain_list_with_quic);
  EXPECT_EQ(ip_protection_core->GetProxyChainList(),
            proxy_chain_list_with_quic);
  EXPECT_EQ(ip_protection_core->GetProxyChainList(),
            proxy_chain_list_with_quic);

  ip_protection_core->QuicProxiesFailed();
  histogram_tester_.ExpectBucketCount(kQuicProxiesFailedHistogram, 3, 1);

  EXPECT_EQ(ip_protection_core->GetProxyChainList(),
            proxy_chain_list_without_quic);

  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ip_protection_core->GetProxyChainList(),
            proxy_chain_list_with_quic);
}

// When the network changes, a new proxy list is requested.
TEST_F(IpProtectionCoreImplTest, RefreshProxyListOnNetworkChange) {
  std::map<std::string, std::string> parameters;
  parameters[net::features::kIpPrivacyUseQuicProxies.name] = "true";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier =
      net::NetworkChangeNotifier::CreateMockIfNeeded();

  auto ipp_proxy_config_manager =
      std::make_unique<MockIpProtectionProxyConfigManager>();
  bool refresh_requested = false;
  ipp_proxy_config_manager->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { refresh_requested = true; }));
  auto ip_protection_core = MakeCore(std::move(ipp_proxy_config_manager));

  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(refresh_requested);
}

// Simulates a geo change detected in the IppProxyConfigManager.
TEST_F(IpProtectionCoreImplTest, GeoChangeObservedInIppProxyConfigManager) {
  // Set up IppProxyConfigManager to have a "new" geo.
  auto ipp_proxy_config_manager =
      std::make_unique<MockIpProtectionProxyConfigManager>();
  bool refresh_requested = false;
  std::string new_geo_signal = "US,US-NY,NEW YORK CITY";
  ipp_proxy_config_manager->SetCurrentGeo(new_geo_signal);
  ipp_proxy_config_manager->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { refresh_requested = true; }));
  ipp_proxy_config_manager->SetProxyList({MakeChain({"a-proxy"})});

  // Set up `IppTokenManager` to have an "old geo"
  auto ipp_token_manager = std::make_unique<MockIpProtectionTokenManager>();
  ipp_token_manager->SetCurrentGeo("US,US-MA,BOSTON");

  IpProtectionCoreImpl::ProxyTokenManagerMap managers;
  managers.insert({ProxyLayer::kProxyA, std::move(ipp_token_manager)});
  auto ip_protection_core =
      MakeCore(std::move(ipp_proxy_config_manager), std::move(managers));

  // Simulate that the new geo signal in the Proxy List Manager resulted in a
  // call to observe a geo change.
  ip_protection_core->GeoObserved(new_geo_signal);

  EXPECT_EQ(ip_protection_core->GetIpProtectionProxyConfigManagerForTesting()
                ->CurrentGeo(),
            new_geo_signal);
  EXPECT_EQ(ip_protection_core
                ->GetIpProtectionTokenManagerForTesting(ProxyLayer::kProxyA)
                ->CurrentGeo(),
            new_geo_signal);

  // Since the new geo matches the geo of the proxy list manager, it should not
  // refresh the proxy list.
  EXPECT_FALSE(refresh_requested);
}

// Current geo for all token cache managers should reflect this empty geo.
TEST_F(IpProtectionCoreImplTest,
       GeoChangeObservedEmptyGeoIdInProxyConfigManager) {
  // Set up IppProxyConfigManager to have a "new" geo.
  auto ipp_proxy_config_manager =
      std::make_unique<MockIpProtectionProxyConfigManager>();
  bool refresh_requested = false;
  std::string empty_geo_signal = "";
  ipp_proxy_config_manager->SetCurrentGeo(empty_geo_signal);
  ipp_proxy_config_manager->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { refresh_requested = true; }));
  ipp_proxy_config_manager->SetProxyList({MakeChain({"a-proxy"})});

  // Set up `IppTokenManager` to have an "old geo"
  std::string boston_geo_id = "US,US-MA,BOSTON";
  auto ipp_token_manager = std::make_unique<MockIpProtectionTokenManager>();
  ipp_token_manager->SetCurrentGeo(boston_geo_id);

  IpProtectionCoreImpl::ProxyTokenManagerMap managers;
  managers.insert({ProxyLayer::kProxyA, std::move(ipp_token_manager)});
  auto ip_protection_core =
      MakeCore(std::move(ipp_proxy_config_manager), std::move(managers));

  // Simulate the empty geo change in the proxy list manager caused a call such
  // as this.
  ip_protection_core->GeoObserved(empty_geo_signal);

  EXPECT_EQ(ip_protection_core->GetIpProtectionProxyConfigManagerForTesting()
                ->CurrentGeo(),
            empty_geo_signal);
  EXPECT_EQ(ip_protection_core
                ->GetIpProtectionTokenManagerForTesting(ProxyLayer::kProxyA)
                ->CurrentGeo(),
            empty_geo_signal);

  // Since the new geo matches the geo of the proxy list manager, it should not
  // refresh the proxy list.
  EXPECT_FALSE(refresh_requested);
}

// Simulates a geo change detected in the IppTokenManager.
TEST_F(IpProtectionCoreImplTest, GeoChangeObservedInIppTokenManager) {
  // Set up `IppTokenManager` to have an "new geo"
  std::string new_geo_signal = "US,US-MA,BOSTON";
  auto ipp_token_manager = std::make_unique<MockIpProtectionTokenManager>();
  ipp_token_manager->SetCurrentGeo(new_geo_signal);

  // Set up IppProxyConfigManager to have a "old" geo.
  auto ipp_proxy_config_manager =
      std::make_unique<MockIpProtectionProxyConfigManager>();
  bool refresh_requested = false;
  ipp_proxy_config_manager->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { refresh_requested = true; }),
      new_geo_signal);
  ipp_proxy_config_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_config_manager->SetCurrentGeo("US,US-NY,NEW YORK CITY");

  IpProtectionCoreImpl::ProxyTokenManagerMap managers;
  managers.insert({ProxyLayer::kProxyA, std::move(ipp_token_manager)});
  auto ip_protection_core =
      MakeCore(std::move(ipp_proxy_config_manager), std::move(managers));

  // Simulate that the new geo signal in the token cache manager resulted in a
  // call to observe a geo change.
  ip_protection_core->GeoObserved(new_geo_signal);

  EXPECT_EQ(ip_protection_core
                ->GetIpProtectionTokenManagerForTesting(ProxyLayer::kProxyA)
                ->CurrentGeo(),
            new_geo_signal);

  EXPECT_EQ(ip_protection_core->GetIpProtectionProxyConfigManagerForTesting()
                ->CurrentGeo(),
            new_geo_signal);

  // Since the new geo matches the geo of the proxy list manager, it should not
  // refresh the proxy list.
  EXPECT_TRUE(refresh_requested);
}

TEST_F(IpProtectionCoreImplTest,
       RequestShouldBeProxied_MdlMatchesForDefaultMdlType) {
  // Create a MDL manager w/ a single entry that matches the default MDL type.
  std::string example_com = "example.com";
  auto masked_domain_list_manager =
      MaskedDomainListManager(IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainList mdl = masked_domain_list::MaskedDomainList();
  ResourceOwner* resource_owner = mdl.add_resource_owners();
  // By not setting an `Experiments` value, the entry is considered 'default'.
  Resource* resource = resource_owner->add_owned_resources();
  resource->set_domain(example_com);
  masked_domain_list_manager.UpdateMaskedDomainListForTesting(mdl);

  // The core should be constructed with the default MDL type (i.e. incognito),
  // so we set `ip_protection_incognito` to true.
  auto ip_protection_core =
      MakeCore(&masked_domain_list_manager, /*ip_protection_incognito=*/true);

  EXPECT_FALSE(ip_protection_core->RequestShouldBeProxied(
      GURL(base::StrCat({"http://", "irrelevant.com"})),
      net::NetworkAnonymizationKey()));

  EXPECT_TRUE(ip_protection_core->RequestShouldBeProxied(
      GURL(base::StrCat({"http://", example_com})),
      net::NetworkAnonymizationKey()));
}

TEST_F(IpProtectionCoreImplTest,
       RequestShouldBeProxied_MdlMatchesForNonDefaultMdlType) {
  // Create a MDL manager w/ a single non-default entry.
  std::string example_com = "example.com";
  auto masked_domain_list_manager =
      MaskedDomainListManager(IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainList mdl = masked_domain_list::MaskedDomainList();
  ResourceOwner* resource_owner = mdl.add_resource_owners();
  // The following resource should only match when the MDL type is
  // `MdlType::kRegularBrowsing`.
  Resource* resource = resource_owner->add_owned_resources();
  resource->set_domain(example_com);
  resource->add_experiments(
      Resource::Experiment::Resource_Experiment_EXPERIMENT_EXTERNAL_REGULAR);
  resource->set_exclude_default_group(true);
  masked_domain_list_manager.UpdateMaskedDomainListForTesting(mdl);

  // The core should be constructed with the regular browsing MDL type, so we
  // set `ip_protection_incognito` to false.
  auto ip_protection_core =
      MakeCore(&masked_domain_list_manager, /*ip_protection_incognito=*/false);

  EXPECT_FALSE(ip_protection_core->RequestShouldBeProxied(
      GURL(base::StrCat({"http://", "irrelevant.com"})),
      net::NetworkAnonymizationKey()));

  EXPECT_TRUE(ip_protection_core->RequestShouldBeProxied(
      GURL(base::StrCat({"http://", example_com})),
      net::NetworkAnonymizationKey()));
}

TEST_F(
    IpProtectionCoreImplTest,
    RequestShouldBeProxied_SplitMdlDisabled_RegularAndIncognitoMatchDefault) {
  // Disable the split MDL feature.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      network::features::kMaskedDomainList,
      {{network::features::kSplitMaskedDomainList.name,
        base::ToString(false)}});

  // Create a MDL manager w/ a single entry that matches the default MDL type.
  std::string example_com = "example.com";
  auto masked_domain_list_manager =
      MaskedDomainListManager(IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainList mdl = masked_domain_list::MaskedDomainList();
  ResourceOwner* resource_owner = mdl.add_resource_owners();
  // By not setting an `Experiments` value, the entry is considered 'default'.
  Resource* resource = resource_owner->add_owned_resources();
  resource->set_domain(example_com);
  masked_domain_list_manager.UpdateMaskedDomainListForTesting(mdl);

  // The core should be constructed with the regular browsing MDL type, so we
  // set `ip_protection_incognito` to false.
  auto ip_protection_core =
      MakeCore(&masked_domain_list_manager, /*ip_protection_incognito=*/false);

  EXPECT_FALSE(ip_protection_core->RequestShouldBeProxied(
      GURL(base::StrCat({"http://", "irrelevant.com"})),
      net::NetworkAnonymizationKey()));

  // The default MDL type should match for regular browsing since the split MDL
  // feature is disabled.
  EXPECT_TRUE(ip_protection_core->RequestShouldBeProxied(
      GURL(base::StrCat({"http://", example_com})),
      net::NetworkAnonymizationKey()));

  // A IP Protection core should also match during an incognito session since
  // the split MDL feature is disabled.
  ip_protection_core =
      MakeCore(&masked_domain_list_manager, /*ip_protection_incognito=*/true);
  EXPECT_TRUE(ip_protection_core->RequestShouldBeProxied(
      GURL(base::StrCat({"http://", example_com})),
      net::NetworkAnonymizationKey()));
}

}  // namespace
}  // namespace ip_protection
