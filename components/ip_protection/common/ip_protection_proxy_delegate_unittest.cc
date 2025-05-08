// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_delegate.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/ip_protection/common/ip_protection_core.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_direct_fetcher.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_manager.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_manager.h"
#include "components/ip_protection/common/masked_domain_list_manager.h"
#include "components/ip_protection/common/probabilistic_reveal_token_registry.h"
#include "components/ip_protection/common/probabilistic_reveal_token_test_consumer.h"
#include "components/ip_protection/common/probabilistic_reveal_token_test_issuer.h"
#include "components/ip_protection/get_probabilistic_reveal_token.pb.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/request_priority.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_response_headers.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/proxy_config.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace ip_protection {
namespace {
using ::masked_domain_list::MaskedDomainList;
using ::masked_domain_list::Resource;
using ::masked_domain_list::ResourceOwner;
using ::network::mojom::IpProtectionProxyBypassPolicy;

constexpr char kHttpsUrl[] = "https://example.com";
constexpr char kHttpUrl[] = "http://example.com";
constexpr char kLocalhost[] = "http://localhost";

constexpr char kProxyResolutionHistogram[] =
    "NetworkService.IpProtection.ProxyResolution";
constexpr char kEligibilityHistogram[] =
    "NetworkService.IpProtection.RequestIsEligibleForProtection";
constexpr char kAreAuthTokensAvailableHistogram[] =
    "NetworkService.IpProtection.AreAuthTokensAvailable";
constexpr char kIsProxyListAvailableHistogram[] =
    "NetworkService.IpProtection.IsProxyListAvailable";
constexpr char kAvailabilityHistogram[] =
    "NetworkService.IpProtection.ProtectionIsAvailableForRequest";
constexpr size_t kPRTPlaintextSize = 29;

class MockIpProtectionCore : public IpProtectionCore {
 public:
  explicit MockIpProtectionCore(
      MaskedDomainListManager* masked_domain_list_manager,
      ProbabilisticRevealTokenRegistry* prt_registry = nullptr,
      // Default is set to true which is needed for the default MDL type.
      bool ip_protection_incognito = true,
      IpProtectionProbabilisticRevealTokenManager* prt_manager = nullptr)
      : masked_domain_list_manager_(masked_domain_list_manager),
        prt_registry_(prt_registry),
        prt_manager_(prt_manager) {
    mdl_type_ = ip_protection_incognito ? MdlType::kIncognito
                                        : MdlType::kRegularBrowsing;
    if (ip_protection_incognito && prt_manager_) {
      prt_manager_->RequestTokens();
    }
  }

  bool IsMdlPopulated() override {
    return masked_domain_list_manager_->IsPopulated();
  }

  bool RequestShouldBeProxied(
      const GURL& request_url,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {
    return masked_domain_list_manager_->Matches(
        request_url, network_anonymization_key, mdl_type_);
  }

  bool IsIpProtectionEnabled() override { return is_ip_protection_enabled_; }

  bool AreAuthTokensAvailable() override { return auth_token_.has_value(); }

  bool IsProbabilisticRevealTokenAvailable() override {
    if (prt_) {
      return true;
    }
    return (prt_manager_ && prt_manager_->IsTokenAvailable());
  }

  bool WereTokenCachesEverFilled() override {
    return were_token_caches_ever_filled_;
  }

  std::optional<BlindSignedAuthToken> GetAuthToken(
      size_t chain_index) override {
    return std::move(auth_token_);
  }

  std::optional<std::string> GetProbabilisticRevealToken(
      const std::string& top_level,
      const std::string& third_party) override {
    if (prt_) {
      return prt_;
    }
    return prt_manager_ ? prt_manager_->GetToken(top_level, third_party)
                        : std::nullopt;
  }

  // Set the auth token that will be returned from the next call to
  // `GetAuthToken()`.
  void SetNextAuthToken(std::optional<BlindSignedAuthToken> auth_token) {
    auth_token_ = std::move(auth_token);
    were_token_caches_ever_filled_ = true;
  }

  // Set the serialized PRT that will be returned for the
  // `GetProbabilisticRevealToken()` call. This is used to mock
  // core's PRT query behavior. For a more realistic PRT manager
  // behavior, construct MockCore with a non-null PRT manager.
  void SetPRT(std::optional<std::string> prt) { prt_ = std::move(prt); }

  std::vector<net::ProxyChain> GetProxyChainList() override {
    return *proxy_list_;
  }

  void QuicProxiesFailed() override {
    if (on_proxies_failed_) {
      std::move(on_proxies_failed_).Run();
    }
  }

  bool IsProxyListAvailable() override { return proxy_list_.has_value(); }

  void RequestRefreshProxyList() override {
    if (on_force_refresh_proxy_list_) {
      std::move(on_force_refresh_proxy_list_).Run();
    }
  }

  void GeoObserved(const std::string& geo_id) override {}

  bool HasTrackingProtectionException(
      const GURL& first_party_url) const override {
    for (const content_settings::HostIndexedContentSettings& index :
         tp_content_settings_) {
      if (const content_settings::RuleEntry* result =
              index.Find(GURL(), first_party_url);
          result != nullptr) {
        return content_settings::ValueToContentSetting(result->second.value) ==
               CONTENT_SETTING_ALLOW;
      }
    }
    return false;
  }

  void SetTrackingProtectionContentSetting(
      const ContentSettingsForOneType& settings) override {
    tp_content_settings_ =
        content_settings::HostIndexedContentSettings::Create(settings);
  }

  bool ShouldRequestIncludeProbabilisticRevealToken(
      const GURL& request_url) override {
    return (prt_registry_ && prt_registry_->IsRegistered(request_url));
  }

  void SetIpProtectionEnabled(bool value) { is_ip_protection_enabled_ = value; }

  // Set the proxy list returned from `ProxyList()`.
  void SetProxyList(std::vector<net::ProxyChain> proxy_list) {
    proxy_list_ = std::move(proxy_list);
  }

  void SetOnRequestRefreshProxyList(
      base::OnceClosure on_force_refresh_proxy_list) {
    on_force_refresh_proxy_list_ = std::move(on_force_refresh_proxy_list);
  }

  void SetOnProxiesFailed(base::OnceClosure on_proxies_failed) {
    on_proxies_failed_ = std::move(on_proxies_failed);
  }

  void ExhaustTokenCache() { auth_token_ = std::nullopt; }

 private:
  bool is_ip_protection_enabled_ = true;
  bool were_token_caches_ever_filled_ = false;
  MdlType mdl_type_;
  std::optional<BlindSignedAuthToken> auth_token_;
  std::optional<std::vector<net::ProxyChain>> proxy_list_;
  std::optional<std::string> prt_;
  std::vector<net::ProxyChain> proxy_chain_list_;
  base::OnceClosure on_force_refresh_proxy_list_;
  base::OnceClosure on_proxies_failed_;
  raw_ptr<MaskedDomainListManager> masked_domain_list_manager_;
  raw_ptr<ProbabilisticRevealTokenRegistry> prt_registry_;
  std::vector<content_settings::HostIndexedContentSettings>
      tp_content_settings_;
  raw_ptr<IpProtectionProbabilisticRevealTokenManager> prt_manager_;
};

MaskedDomainListManager CreateMdlManager(
    const std::map<std::string, std::set<std::string>>& first_party_map) {
  auto allow_list = MaskedDomainListManager(
      IpProtectionProxyBypassPolicy::kFirstPartyToTopLevelFrame);

  MaskedDomainList mdl = masked_domain_list::MaskedDomainList();

  for (auto const& [domain, properties] : first_party_map) {
    ResourceOwner& resourceOwner = *mdl.add_resource_owners();
    for (auto property : properties) {
      resourceOwner.add_owned_properties(property);
    }
    Resource& resource = *resourceOwner.add_owned_resources();
    resource.set_domain(domain);
  }

  allow_list.UpdateMaskedDomainList(
      mdl,
      /*exclusion_list=*/std::vector<std::string>());
  return allow_list;
}

base::Value::Dict CreateRegistryFromJson(const std::string& json_content) {
  std::optional<base::Value> json =
      base::JSONReader::Read(json_content, base::JSON_ALLOW_TRAILING_COMMAS);
  CHECK(json.has_value());
  return (*std::move(json)).TakeDict();
}

}  // namespace

MATCHER_P2(Contain,
           expected_name,
           expected_value,
           std::string("headers ") + (negation ? "don't " : "") + "contain '" +
               expected_name + ": " + expected_value + "'") {
  std::optional<std::string> value = arg.GetHeader(expected_name);
  return value && value == expected_value;
}

struct HeadersReceived {
  net::ProxyChain proxy_chain;
  uint64_t chain_index;
  scoped_refptr<net::HttpResponseHeaders> response_headers;
};

class TestCustomProxyConnectionObserver
    : public network::mojom::CustomProxyConnectionObserver {
 public:
  TestCustomProxyConnectionObserver() = default;
  ~TestCustomProxyConnectionObserver() override = default;

  const std::optional<std::pair<net::ProxyChain, int>>& FallbackArgs() const {
    return fallback_;
  }

  const std::optional<HeadersReceived>& HeadersReceivedArgs() const {
    return headers_received_;
  }

  // mojom::CustomProxyConnectionObserver:
  void OnFallback(const net::ProxyChain& bad_chain, int net_error) override {
    fallback_ = std::make_pair(bad_chain, net_error);
  }
  void OnTunnelHeadersReceived(const net::ProxyChain& proxy_chain,
                               uint64_t chain_index,
                               const scoped_refptr<net::HttpResponseHeaders>&
                                   response_headers) override {
    headers_received_ =
        HeadersReceived{proxy_chain, chain_index, response_headers};
  }

 private:
  std::optional<std::pair<net::ProxyChain, int>> fallback_;
  std::optional<HeadersReceived> headers_received_;
};

class IpProtectionProxyDelegateTest : public testing::Test {
 public:
  IpProtectionProxyDelegateTest() = default;

  void SetUp() override {
    context_ = net::CreateTestURLRequestContextBuilder()->Build();
    scoped_feature_list_.InitWithFeatures(
        {net::features::kEnableIpProtectionProxy,
         network::features::kMaskedDomainList},
        {});
    // Advance to an arbitrary time.
    task_environment_.AdvanceClock(base::Time::UnixEpoch() + base::Days(4242) -
                                   base::Time::Now());
  }

 protected:
  std::unique_ptr<IpProtectionProxyDelegate> CreateDelegate(
      IpProtectionCore* ipp_core) {
    return std::make_unique<IpProtectionProxyDelegate>(ipp_core);
  }

  std::unique_ptr<net::URLRequest> CreateRequest(const GURL& url) {
    return context_->CreateRequest(url, net::DEFAULT_PRIORITY, nullptr,
                                   TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  // Shortcut to create a ProxyChain from hostnames.
  net::ProxyChain MakeChain(std::vector<std::string> hostnames,
                            int chain_id = 0) {
    std::vector<net::ProxyServer> servers;
    for (auto& hostname : hostnames) {
      servers.push_back(net::ProxyServer::FromSchemeHostAndPort(
          net::ProxyServer::SCHEME_HTTPS, hostname, std::nullopt));
    }
    return net::ProxyChain::ForIpProtection(servers, chain_id);
  }

  BlindSignedAuthToken MakeAuthToken(std::string content) {
    BlindSignedAuthToken token;
    token.token = std::move(content);
    return token;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<net::URLRequestContext> context_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(IpProtectionProxyDelegateTest, AddsTokenToTunnelRequest) {
  MaskedDomainListManager mdl_manager = CreateMdlManager(
      /*first_party_map=*/{});
  auto ipp_core = std::make_unique<MockIpProtectionCore>(&mdl_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});
  auto delegate = CreateDelegate(ipp_core.get());

  net::HttpRequestHeaders headers;
  auto ip_protection_proxy_chain = net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxya", std::nullopt),
       net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxyb", std::nullopt)});
  EXPECT_THAT(delegate->OnBeforeTunnelRequest(ip_protection_proxy_chain,
                                              /*chain_index=*/0, &headers),
              IsOk());

  EXPECT_THAT(headers, Contain("Authorization", "Bearer: a-token"));
}

TEST_F(IpProtectionProxyDelegateTest, ErrorIfConnectionWithNoTokens) {
  auto masked_domain_list_manager = CreateMdlManager(
      /*first_party_map=*/{});
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});
  auto delegate = CreateDelegate(ipp_core.get());

  net::HttpRequestHeaders headers;
  auto ip_protection_proxy_chain = net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxya", std::nullopt),
       net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxyb", std::nullopt)});
  EXPECT_THAT(delegate->OnBeforeTunnelRequest(ip_protection_proxy_chain,
                                              /*chain_index=*/0, &headers),
              IsError(net::ERR_TUNNEL_CONNECTION_FAILED));
  EXPECT_THAT(delegate->OnBeforeTunnelRequest(ip_protection_proxy_chain,
                                              /*chain_index=*/1, &headers),
              IsError(net::ERR_TUNNEL_CONNECTION_FAILED));
}

TEST_F(IpProtectionProxyDelegateTest, AddsDebugExperimentArm) {
  std::map<std::string, std::string> parameters;
  parameters[net::features::kIpPrivacyDebugExperimentArm.name] = "13";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));
  for (int chain_index : {0, 1}) {
    auto masked_domain_list_manager = CreateMdlManager(
        /*first_party_map=*/{});
    auto ipp_core =
        std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
    ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
    ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});
    auto delegate = CreateDelegate(ipp_core.get());

    net::HttpRequestHeaders headers;
    auto ip_protection_proxy_chain = net::ProxyChain::ForIpProtection(
        {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                                 "proxya", std::nullopt),
         net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                                 "proxyb", std::nullopt)});
    EXPECT_THAT(delegate->OnBeforeTunnelRequest(ip_protection_proxy_chain,
                                                chain_index, &headers),
                IsOk());
    EXPECT_THAT(headers, Contain("Ip-Protection-Debug-Experiment-Arm", "13"));
  }
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxyDeprioritizesBadProxies) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"}),
                          MakeChain({"backup-proxya", "backup-proxyb"})});
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo& info = retry_map[net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxya", std::nullopt),
       net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxyb", std::nullopt)})];
  info.try_while_bad = false;
  info.bad_until = base::TimeTicks::Now() + base::Days(2);

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", std::move(retry_map), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyChain(net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "backup-proxya", std::nullopt),
       net::ProxyServer::FromSchemeHostAndPort(
           net::ProxyServer::SCHEME_HTTPS, "backup-proxyb", std::nullopt)}));
  expected_proxy_list.AddProxyChain(net::ProxyChain::ForIpProtection({}));

  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list))
      << "Got: " << result.proxy_list().ToDebugString();
  EXPECT_TRUE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(kProxyResolutionHistogram,
                                       ProxyResolutionResult::kAttemptProxy, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxyAllProxiesBad) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo& info = retry_map[net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxya", std::nullopt),
       net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxyb", std::nullopt)})];
  info.try_while_bad = false;
  info.bad_until = base::TimeTicks::Now() + base::Days(2);

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", std::move(retry_map), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_TRUE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(kProxyResolutionHistogram,
                                       ProxyResolutionResult::kAttemptProxy, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

TEST_F(IpProtectionProxyDelegateTest,
       OnResolveProxyMaskedDomainListManagerMatch) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList(
      {MakeChain({"ippro-1", "ippro-2"}), MakeChain({"ippro-2", "ippro-2"})});
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  // Verify that the IP Protection proxy list is correctly merged with the
  // existing proxy list.
  result.UsePacString("PROXY bar; DIRECT; PROXY weird");
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));

  const net::ProxyServer kProxyServer1{net::ProxyServer::SCHEME_HTTPS,
                                       net::HostPortPair("ippro-1", 443)};
  const net::ProxyServer kProxyServer2{net::ProxyServer::SCHEME_HTTPS,
                                       net::HostPortPair("ippro-2", 443)};
  const net::ProxyChain kIpProtectionChain1 =
      net::ProxyChain::ForIpProtection({kProxyServer1, kProxyServer2});
  const net::ProxyChain kIpProtectionChain2 =
      net::ProxyChain::ForIpProtection({kProxyServer2, kProxyServer2});

  expected_proxy_list.AddProxyChain(std::move(kIpProtectionChain1));
  expected_proxy_list.AddProxyChain(std::move(kIpProtectionChain2));
  expected_proxy_list.AddProxyChain(net::ProxyChain::ForIpProtection({}));
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY weird"));

  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list))
      << "Got: " << result.proxy_list().ToDebugString();
  EXPECT_FALSE(result.is_for_ip_protection());

  // After a fallback, the first IP Protection proxy chain should be used.
  EXPECT_TRUE(result.Fallback(net::ERR_PROXY_CONNECTION_FAILED,
                              net::NetLogWithSource()));
  EXPECT_TRUE(result.is_for_ip_protection());

  histogram_tester_.ExpectUniqueSample(kProxyResolutionHistogram,
                                       ProxyResolutionResult::kAttemptProxy, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

TEST_F(IpProtectionProxyDelegateTest,
       OnResolveProxyMaskedDomainListManagerMatch_DirectOnly) {
  std::map<std::string, std::string> parameters;
  parameters[net::features::kIpPrivacyDirectOnly.name] = "true";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"foo"})});
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  auto ip_protection_proxy_chain = net::ProxyChain::ForIpProtection({});
  expected_proxy_list.AddProxyChain(std::move(ip_protection_proxy_chain));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list))
      << "Got: " << result.proxy_list().ToDebugString();
  EXPECT_TRUE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(kProxyResolutionHistogram,
                                       ProxyResolutionResult::kAttemptProxy, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

TEST_F(IpProtectionProxyDelegateTest,
       OnResolveProxyMaskedDomainListManagerDoesNotMatch_FirstPartyException) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {"top.com"};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"ippro-1"}), MakeChain({"ippro-2"})});
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(kProxyResolutionHistogram,
                                       ProxyResolutionResult::kNoMdlMatch, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kIneligible, 1);
  histogram_tester_.ExpectTotalCount(kAreAuthTokensAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kIsProxyListAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAvailabilityHistogram, 0);
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxy_NoAuthTokenEver) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetProxyList({MakeChain({"proxy"})});
  // No token is added to the cache, so the result will be direct.
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());

  histogram_tester_.ExpectUniqueSample(
      kProxyResolutionHistogram, ProxyResolutionResult::kTokensNeverAvailable,
      1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, false,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, false, 1);
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxy_NoAuthToken_Exhausted) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetProxyList({MakeChain({"proxy"})});

  // Token is added but will be removed to simulate exhaustion.
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->ExhaustTokenCache();

  // Tokens in cache are exhausted, so the result will be direct.
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());

  histogram_tester_.ExpectUniqueSample(
      kProxyResolutionHistogram, ProxyResolutionResult::kTokensExhausted, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, false,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, false, 1);
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxy_NoProxyList) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  // No proxy list is added to the cache, so the result will be direct.
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kProxyResolutionHistogram, ProxyResolutionResult::kProxyListNotAvailable,
      1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, false,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, false,
                                       1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, false, 1);
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxy_IpProtectionDisabled) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxy"})});
  ipp_core->SetIpProtectionEnabled(false);
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kProxyResolutionHistogram, ProxyResolutionResult::kSettingDisabled, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectTotalCount(kAreAuthTokensAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kIsProxyListAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAvailabilityHistogram, 0);
}

// When URLs do not match the allow list, the result is direct and not flagged
// as for IP protection.
TEST_F(IpProtectionProxyDelegateTest, OnResolveProxyIpProtectionNoMatch) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["not.example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"ippro-1"}), MakeChain({"ippro-2"})});
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kLocalhost),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("http://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(kProxyResolutionHistogram,
                                       ProxyResolutionResult::kNoMdlMatch, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kIneligible, 1);
  histogram_tester_.ExpectTotalCount(kAreAuthTokensAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kIsProxyListAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAvailabilityHistogram, 0);
}

// If the allowlist is empty, this suggests it hasn't yet been populated and
// thus we don't really know whether the request is supposed to be eligible or
// not.
TEST_F(IpProtectionProxyDelegateTest,
       OnResolveProxyIpProtectionNoMatch_UnpopulatedAllowList) {
  std::map<std::string, std::set<std::string>> first_party_map;
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"ippro-1"}), MakeChain({"ippro-2"})});
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kLocalhost),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("http://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kProxyResolutionHistogram, ProxyResolutionResult::kMdlNotPopulated, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kUnknown, 1);
  histogram_tester_.ExpectTotalCount(kAreAuthTokensAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kIsProxyListAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAvailabilityHistogram, 0);
}

// When the top frame url has a User Bypass exception, do not attempt to proxy.
TEST_F(IpProtectionProxyDelegateTest, OnResolveProxy_HasSiteException) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{net::features::kEnableIpProtectionProxy,
        {{"IpPrivacyEnableUserBypass", "true"}}},
       {network::features::kMaskedDomainList, {}}},
      {});
  std::map<std::string, std::set<std::string>> first_party_map;
  std::string top_frame_url = "https://top.com";
  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});

  content_settings::RuleMetaData metadata;
  metadata.SetExpirationAndLifetime(base::Time(), base::TimeDelta());

  ipp_core->SetTrackingProtectionContentSetting({ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString(top_frame_url),
      base::Value(CONTENT_SETTING_ALLOW), content_settings::ProviderType::kNone,
      /*incognito=*/true, std::move(metadata))});

  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL(top_frame_url))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());

  histogram_tester_.ExpectUniqueSample(
      kProxyResolutionHistogram, ProxyResolutionResult::kHasSiteException, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
}

// When the top frame url has a User Bypass exception and the user has navigated
// to a subdomain of the top frame url, do not attempt to proxy.
TEST_F(IpProtectionProxyDelegateTest,
       OnResolveProxy_HasSiteExceptionForSubdomain) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{net::features::kEnableIpProtectionProxy,
        {{"IpPrivacyEnableUserBypass", "true"}}},
       {network::features::kMaskedDomainList, {}}},
      {});
  std::map<std::string, std::set<std::string>> first_party_map;
  std::string top_frame_url = "https://top.com";
  std::string subdomain_url = "https://sub.top.com";

  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});

  content_settings::RuleMetaData metadata;
  metadata.SetExpirationAndLifetime(base::Time(), base::TimeDelta());

  ipp_core->SetTrackingProtectionContentSetting({ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString(top_frame_url),
      base::Value(CONTENT_SETTING_ALLOW), content_settings::ProviderType::kNone,
      /*incognito=*/true, std::move(metadata))});

  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL(subdomain_url))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());

  histogram_tester_.ExpectUniqueSample(
      kProxyResolutionHistogram, ProxyResolutionResult::kHasSiteException, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
}

// When the top frame url has a User Bypass exception but the experiment to
// enable the proxying logic is not enabled, still proxy successfully.
TEST_F(
    IpProtectionProxyDelegateTest,
    OnResolveProxy_HasSiteExceptionWithExperimentDisabledWillProxySucessfully) {
  std::map<std::string, std::set<std::string>> first_party_map;
  std::string top_frame_url = "https://top.com";

  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});

  content_settings::RuleMetaData metadata;
  metadata.SetExpirationAndLifetime(base::Time(), base::TimeDelta());

  ipp_core->SetTrackingProtectionContentSetting({ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString(top_frame_url),
      base::Value(CONTENT_SETTING_ALLOW), content_settings::ProviderType::kNone,
      /*incognito=*/true, std::move(metadata))});

  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL(top_frame_url))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_FALSE(result.is_direct());
  EXPECT_TRUE(result.is_for_ip_protection());

  histogram_tester_.ExpectUniqueSample(kProxyResolutionHistogram,
                                       ProxyResolutionResult::kAttemptProxy, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
}

// When the URL is HTTP and multi-proxy chains are used, the result is flagged
// as for IP protection and is not direct.
TEST_F(IpProtectionProxyDelegateTest,
       OnResolveProxyIpProtectionMultiProxyHttpSuccess) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxy1", "proxy2"})});
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("http://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_FALSE(result.is_direct());
  EXPECT_TRUE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

// When URLs match the allow list, and a token is available, the result is
// flagged as for IP protection and is not direct.
TEST_F(IpProtectionProxyDelegateTest, OnResolveProxyIpProtectionSuccess) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(first_party_map);
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});
  auto delegate = CreateDelegate(ipp_core.get());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_FALSE(result.is_direct());
  EXPECT_TRUE(result.is_for_ip_protection());
  EXPECT_FALSE(result.prt_header_value().has_value());
  histogram_tester_.ExpectUniqueSample(kProxyResolutionHistogram,
                                       ProxyResolutionResult::kAttemptProxy, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxyPRTSuccess) {
  const GURL top_level_url =
      GURL("https://sub.top.com:27272/another/path/arbitrary");
  const GURL destination_url =
      GURL("https://foo.example.com:1234/some/arbitrary/path/");
  std::map<std::string, std::set<std::string>> mdl_map;
  mdl_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(mdl_map);
  ProbabilisticRevealTokenRegistry registry;
  registry.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": [
      "example.com",
    ]
  })json"));
  auto ipp_core = std::make_unique<MockIpProtectionCore>(
      &masked_domain_list_manager, &registry);
  ipp_core->SetPRT("serialized-prt");
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});
  auto delegate = CreateDelegate(ipp_core.get());
  net::ProxyInfo result;
  delegate->OnResolveProxy(destination_url,
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(top_level_url)),
                           "GET", net::ProxyRetryInfoMap(), &result);
  std::optional<std::string> maybe_header_value = result.prt_header_value();
  ASSERT_TRUE(maybe_header_value.has_value());
  EXPECT_EQ(maybe_header_value.value(),
            ":" + base::Base64Encode("serialized-prt") + ":");
  const auto maybe_item =
      net::structured_headers::ParseBareItem(maybe_header_value.value());
  ASSERT_TRUE(maybe_item.has_value());
  EXPECT_EQ(maybe_item.value(),
            net::structured_headers::Item(
                "serialized-prt",
                net::structured_headers::Item::ItemType::kByteSequenceType));
}

TEST_F(IpProtectionProxyDelegateTest, NoPRTHeaderWhenFetchOnlyFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{net::features::kEnableProbabilisticRevealTokens,
        {{"ProbabilisticRevealTokenFetchOnly", "true"}}}},
      {});

  const GURL top_level_url =
      GURL("https://sub.top.com:27272/another/path/arbitrary");
  const GURL destination_url =
      GURL("https://foo.example.com:1234/some/arbitrary/path/");
  std::map<std::string, std::set<std::string>> mdl_map;
  mdl_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(mdl_map);
  ProbabilisticRevealTokenRegistry registry;
  registry.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": [
      "example.com",
    ]
  })json"));
  auto ipp_core = std::make_unique<MockIpProtectionCore>(
      &masked_domain_list_manager, &registry);
  ipp_core->SetPRT("serialized-prt");
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});
  auto delegate = CreateDelegate(ipp_core.get());
  net::ProxyInfo result;
  delegate->OnResolveProxy(destination_url,
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(top_level_url)),
                           "GET", net::ProxyRetryInfoMap(), &result);
  std::optional<std::string> maybe_header_value = result.prt_header_value();
  ASSERT_FALSE(maybe_header_value.has_value());
}

TEST_F(IpProtectionProxyDelegateTest,
       PRTHeaderNotAddedToNonProxiedRequestsByDefault) {
  const GURL top_level_url =
      GURL("https://sub.top.com:27272/another/path/arbitrary");
  const GURL destination_url =
      GURL("https://foo.example.com:1234/some/arbitrary/path/");
  // Empty MDL.
  std::map<std::string, std::set<std::string>> mdl_map;
  auto masked_domain_list_manager = CreateMdlManager(mdl_map);
  ProbabilisticRevealTokenRegistry registry;
  registry.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": [
      "example.com",
    ]
  })json"));
  auto ipp_core = std::make_unique<MockIpProtectionCore>(
      &masked_domain_list_manager, &registry);
  ipp_core->SetPRT("serialized-prt");
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});
  auto delegate = CreateDelegate(ipp_core.get());
  net::ProxyInfo result;
  delegate->OnResolveProxy(destination_url,
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(top_level_url)),
                           "GET", net::ProxyRetryInfoMap(), &result);
  std::optional<std::string> maybe_header_value = result.prt_header_value();
  ASSERT_FALSE(maybe_header_value.has_value());
}

TEST_F(IpProtectionProxyDelegateTest,
       PRTHeaderNotAddedToNonProxiedRequestsWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{net::features::kEnableProbabilisticRevealTokens,
        {{"EnableProbabilisticRevealTokensForNonProxiedRequests", "false"}}}},
      {});

  const GURL top_level_url =
      GURL("https://sub.top.com:27272/another/path/arbitrary");
  const GURL destination_url =
      GURL("https://foo.example.com:1234/some/arbitrary/path/");
  // Empty MDL.
  std::map<std::string, std::set<std::string>> mdl_map;
  auto masked_domain_list_manager = CreateMdlManager(mdl_map);
  ProbabilisticRevealTokenRegistry registry;
  registry.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": [
      "example.com",
    ]
  })json"));
  auto ipp_core = std::make_unique<MockIpProtectionCore>(
      &masked_domain_list_manager, &registry);
  ipp_core->SetPRT("serialized-prt");
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});
  auto delegate = CreateDelegate(ipp_core.get());
  net::ProxyInfo result;
  delegate->OnResolveProxy(destination_url,
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(top_level_url)),
                           "GET", net::ProxyRetryInfoMap(), &result);
  std::optional<std::string> maybe_header_value = result.prt_header_value();
  ASSERT_FALSE(maybe_header_value.has_value());
}

TEST_F(IpProtectionProxyDelegateTest,
       PRTHeaderAddedToNonProxiedRequestsWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{net::features::kEnableProbabilisticRevealTokens,
        {{"EnableProbabilisticRevealTokensForNonProxiedRequests", "true"}}}},
      {});

  const GURL top_level_url =
      GURL("https://sub.top.com:27272/another/path/arbitrary");
  const GURL destination_url =
      GURL("https://foo.example.com:1234/some/arbitrary/path/");
  // Empty MDL.
  std::map<std::string, std::set<std::string>> mdl_map;
  auto masked_domain_list_manager = CreateMdlManager(mdl_map);
  ProbabilisticRevealTokenRegistry registry;
  registry.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": [
      "example.com",
    ]
  })json"));
  auto ipp_core = std::make_unique<MockIpProtectionCore>(
      &masked_domain_list_manager, &registry);
  ipp_core->SetPRT("serialized-prt");
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});
  auto delegate = CreateDelegate(ipp_core.get());
  net::ProxyInfo result;
  delegate->OnResolveProxy(destination_url,
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(top_level_url)),
                           "GET", net::ProxyRetryInfoMap(), &result);
  std::optional<std::string> maybe_header_value = result.prt_header_value();
  ASSERT_TRUE(maybe_header_value.has_value());
  EXPECT_EQ(maybe_header_value.value(),
            ":" + base::Base64Encode("serialized-prt") + ":");
  const auto maybe_item =
      net::structured_headers::ParseBareItem(maybe_header_value.value());
  ASSERT_TRUE(maybe_item.has_value());
  EXPECT_EQ(maybe_item.value(),
            net::structured_headers::Item(
                "serialized-prt",
                net::structured_headers::Item::ItemType::kByteSequenceType));
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxyPRTNoToken) {
  const GURL top_level_url =
      GURL("https://sub.top.com:27272/another/path/arbitrary");
  const GURL destination_url =
      GURL("https://foo.example.com:1234/some/arbitrary/path/");
  std::map<std::string, std::set<std::string>> mdl_map;
  mdl_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(mdl_map);
  ProbabilisticRevealTokenRegistry registry;
  registry.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": [
      "example.com",
    ]
  })json"));
  auto ipp_core = std::make_unique<MockIpProtectionCore>(
      &masked_domain_list_manager, &registry);
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});
  // `ipp_core` does not have any PRTs.
  auto delegate = CreateDelegate(ipp_core.get());
  net::ProxyInfo result;
  delegate->OnResolveProxy(destination_url,
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(top_level_url)),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_FALSE(result.prt_header_value().has_value());
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxyPRTNotInRegList) {
  const GURL top_level_url =
      GURL("https://sub.top.com:27272/another/path/arbitrary");
  const GURL destination_url =
      GURL("https://foo.example.com:1234/some/arbitrary/path/");
  std::map<std::string, std::set<std::string>> mdl_map;
  mdl_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(mdl_map);
  ProbabilisticRevealTokenRegistry registry;
  // Pass empty registration list to the core.
  auto ipp_core = std::make_unique<MockIpProtectionCore>(
      &masked_domain_list_manager, &registry);
  ipp_core->SetPRT("prt-serialized");
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});
  auto delegate = CreateDelegate(ipp_core.get());
  net::ProxyInfo result;
  delegate->OnResolveProxy(destination_url,
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(top_level_url)),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_FALSE(result.prt_header_value().has_value());
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxyPRTIntegration) {
  const GURL top_level_url =
      GURL("https://sub.top.com:27272/another/path/arbitrary");
  const GURL destination_url =
      GURL("https://foo.example.com:1234/some/arbitrary/path/");
  std::map<std::string, std::set<std::string>> mdl_map;
  mdl_map["example.com"] = {};
  auto masked_domain_list_manager = CreateMdlManager(mdl_map);
  ProbabilisticRevealTokenRegistry registry;
  registry.UpdateRegistry(CreateRegistryFromJson(R"json({
    "domains": [
      "example.com",
    ]
  })json"));

  // Mock a response proto type that is received from the PRT issuer server.
  base::expected<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>,
                 absl::Status>
      maybe_issuer =
          ProbabilisticRevealTokenTestIssuer::Create(/*private_key=*/2468);
  ASSERT_TRUE(maybe_issuer.has_value());
  auto& issuer = maybe_issuer.value();
  const size_t num_tokens = 100;
  std::vector<std::string> plaintexts(num_tokens, "");
  for (std::size_t i = 0; i < num_tokens; ++i) {
    std::string p = "prt-for-testing-" + base::NumberToString(i);
    plaintexts[i] = p + std::string(kPRTPlaintextSize - p.size(), '-');
  }
  const std::string epoch_id = "epoch-id";
  base::expected<GetProbabilisticRevealTokenResponse, absl::Status>
      maybe_response = issuer->Issue(
          plaintexts,
          /*expiration_time=*/base::Time::Now() + base::Hours(10),
          /*next_epoch_start_time=*/base::Time::Now() + base::Hours(8),
          /*num_tokens_with_signal=*/10, epoch_id);
  ASSERT_TRUE(maybe_response.has_value())
      << "Issue() returned error " << maybe_response.error();
  const std::string response_str = maybe_response->SerializeAsString();

  // Set interceptor to return `response_str`, i.e., the mocked PRT issuer
  // server response.
  network::TestURLLoaderFactory test_url_loader_factory;
  GURL prt_server_url =
      GURL(net::features::kProbabilisticRevealTokenServer.Get() +
           net::features::kProbabilisticRevealTokenServerPath.Get());
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        GetProbabilisticRevealTokenRequest request_proto;
        ASSERT_TRUE(request_proto.ParseFromString(GetUploadData(request)));
        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory.AddResponse(
            prt_server_url, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::OK));
      }));

  // Create a PRT manager.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath data_dir =
      temp_dir.GetPath().AppendASCII("DataDirectory");
  auto fetcher =
      std::make_unique<IpProtectionProbabilisticRevealTokenDirectFetcher>(
          test_url_loader_factory.GetSafeWeakWrapper()->Clone(),
          version_info::Channel::DEFAULT);
  auto manager = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher), data_dir);

  auto ipp_core = std::make_unique<MockIpProtectionCore>(
      &masked_domain_list_manager, &registry,
      /*ip_protection_incognito=*/true, manager.get());
  ipp_core->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_core->SetProxyList({MakeChain({"proxya", "proxyb"})});

  // Advance time for PRT manager to fetch PRTs.
  RunUntilIdle();

  ASSERT_TRUE(manager->IsTokenAvailable())
      << "PRT manager is expected to have tokens to proceed";

  auto delegate = CreateDelegate(ipp_core.get());
  net::ProxyInfo result;
  delegate->OnResolveProxy(destination_url,
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(top_level_url)),
                           "GET", net::ProxyRetryInfoMap(), &result);

  std::optional<std::string> maybe_header_value = result.prt_header_value();
  ASSERT_TRUE(maybe_header_value.has_value());

  auto const get_etld_plus_one = [](const GURL& url) -> std::string {
    return net::registry_controlled_domains::GetDomainAndRegistry(
        url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  };
  std::optional<std::string> maybe_serialized_token =
      ipp_core->GetProbabilisticRevealToken(get_etld_plus_one(top_level_url),
                                            get_etld_plus_one(destination_url));
  ASSERT_TRUE(maybe_serialized_token)
      << "core is expected to return the token in the header";

  // Verify header value is :Base64Encode(serialized_token):
  const std::string serialized_token =
      std::move(maybe_serialized_token).value();
  EXPECT_EQ(maybe_header_value.value(),
            ":" + base::Base64Encode(serialized_token) + ":");

  // Verify header value yields same `Item(serialized_token)` when parsed.
  std::optional<net::structured_headers::Item> maybe_item =
      net::structured_headers::ParseBareItem(maybe_header_value.value());
  ASSERT_TRUE(maybe_item.has_value());
  EXPECT_EQ(maybe_item.value(),
            net::structured_headers::Item(
                serialized_token,
                net::structured_headers::Item::ItemType::kByteSequenceType));

  // `consumer` mocks a party that receives a PRT, de-serializes the PRT.
  std::optional<ProbabilisticRevealTokenTestConsumer> consumer =
      ProbabilisticRevealTokenTestConsumer::MaybeCreate(
          maybe_item->GetString());
  ASSERT_TRUE(consumer) << "de-serializing PRT failed";

  // Verify header yields to right epoch id once parsed.
  EXPECT_THAT(consumer->EpochId(), epoch_id);

  // Verify the PRT from the header yields to an expected plaintext once
  // revealed.
  base::expected<std::string, absl::Status> maybe_revealed_token =
      issuer->RevealToken(consumer->Token());
  ASSERT_TRUE(maybe_revealed_token.has_value())
      << "decrypting obtained PRT failed";
  EXPECT_THAT(plaintexts, testing::Contains(maybe_revealed_token.value()))
      << "revealed token value is not in starting plaintexts";
}

TEST_F(IpProtectionProxyDelegateTest, OnSuccessfulRequestAfterFailures) {
  auto check = [this](std::string_view name,
                      const net::ProxyRetryInfoMap& proxy_retry_info_map,
                      bool expected_call) {
    SCOPED_TRACE(name);
    bool on_proxies_failed_called = false;
    auto masked_domain_list_manager = CreateMdlManager(
        /*first_party_map=*/{});
    auto ipp_core =
        std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
    ipp_core->SetOnProxiesFailed(
        base::BindLambdaForTesting([&]() { on_proxies_failed_called = true; }));
    auto delegate = CreateDelegate(ipp_core.get());
    delegate->OnSuccessfulRequestAfterFailures(proxy_retry_info_map);
    EXPECT_EQ(expected_call, on_proxies_failed_called);
  };

  auto quic_chain1 = net::ProxyChain::ForIpProtection({
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_QUIC,
                                              "proxy.com", std::nullopt),
  });
  auto quic_chain2 = net::ProxyChain::ForIpProtection({
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_QUIC,
                                              "proxy2.com", std::nullopt),
  });
  auto https_chain1 = net::ProxyChain::ForIpProtection({
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "proxy.com", std::nullopt),
  });

  check("Only QUIC proxies",
        {
            {quic_chain1, net::ProxyRetryInfo()},
            {quic_chain2, net::ProxyRetryInfo()},
        },
        true);

  check("Only HTTPS proxies",
        {
            {https_chain1, net::ProxyRetryInfo()},
        },
        false);

  check("Mixed QUIC and HTTPS proxies",
        {
            {quic_chain1, net::ProxyRetryInfo()},
            {https_chain1, net::ProxyRetryInfo()},
            {quic_chain2, net::ProxyRetryInfo()},
        },
        false);
}

TEST_F(IpProtectionProxyDelegateTest, OnFallback) {
  constexpr int kChainId = 2;
  auto ip_protection_proxy_chain = net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxy.com", std::nullopt)},
      kChainId);
  bool force_refresh_called = false;

  auto masked_domain_list_manager = CreateMdlManager(
      /*first_party_map=*/{});
  auto ipp_core =
      std::make_unique<MockIpProtectionCore>(&masked_domain_list_manager);
  ipp_core->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { force_refresh_called = true; }));
  auto delegate = CreateDelegate(ipp_core.get());

  delegate->OnFallback(ip_protection_proxy_chain, net::ERR_FAILED);
  EXPECT_TRUE(force_refresh_called);
  histogram_tester_.ExpectBucketCount(
      "NetworkService.IpProtection.ProxyChainFallback", kChainId, 1);
}

// TODO(crbug.com/365771838): Add tests for non-ip protection nested proxy
// chains if support is enabled for all builds.
TEST_F(IpProtectionProxyDelegateTest, MergeProxyRules) {
  net::ProxyChain chain1 = net::ProxyChain::ForIpProtection({
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "proxy2a.com", 80),
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "proxy2b.com", 80),
  });
  net::ProxyChain chain2(net::ProxyChain::Direct());
  net::ProxyChain chain3(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTPS, "proxy1.com", 80));
  net::ProxyList existing_proxy_list;
  existing_proxy_list.AddProxyChain(chain1);
  existing_proxy_list.AddProxyChain(chain2);
  existing_proxy_list.AddProxyChain(chain3);

  net::ProxyChain custom1 = net::ProxyChain::ForIpProtection({
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "custom-a.com", 80),
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "custom-b.com", 80),
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "custom-c.com", 80),
  });
  net::ProxyChain custom2(net::ProxyChain::Direct());
  net::ProxyList custom_proxy_list;
  custom_proxy_list.AddProxyChain(custom1);
  custom_proxy_list.AddProxyChain(custom2);

  auto result = IpProtectionProxyDelegate::MergeProxyRules(existing_proxy_list,
                                                           custom_proxy_list);

  // Custom chains replace `chain2`.
  std::vector<net::ProxyChain> expected = {
      chain1,
      custom1,
      custom2,
      chain3,
  };
  EXPECT_EQ(result.AllChains(), expected);
}

}  // namespace ip_protection
