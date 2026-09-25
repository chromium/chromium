// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_service.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/net/core/auth_scope_metadata.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/enterprise/net/core/scoped_extra_allowed_domains_for_testing.h"
#include "components/enterprise/net/core/utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_net {

namespace {

net::ProxyChain MakeHttpsProxyChain(const std::string& host_and_port) {
  return net::ProxyChain(net::ProxyUriToProxyServer(
      host_and_port, net::ProxyServer::SCHEME_HTTPS));
}

class MockObserver : public EnterpriseProxyService::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void, OnDynamicProxyConfigsStatusChanged, (), (override));
};

constexpr char kTestDomain1[] = "domain1.example.com";
constexpr char kTestDomain2[] = "domain2.example.com";
constexpr char kTestFailDomain[] = "fail.example.com";

const char kValidPvdJson1[] = R"({
  "identifier": "domain1.example.com",
  "expires": "Wed, 21 Oct 2026 07:28:00 GMT",
  "proxies": [
    {
      "protocol": "https-connect",
      "identity": "proxy1",
      "proxy": "proxy1.example.com:443",
      "google_chrome": {
        "auth": {
          "type": "profile_bearer_token",
          "scope": "cloud_secure_gateway"
        },
        "extra_headers": [
          {
            "key": "X-Client-ID",
            "value": "test_client",
            "type": "constant"
          },
          {
            "key": "X-Profile-ID",
            "value": "${profile_id}",
            "type": "variable"
          }
        ]
      }
    }
  ],
  "proxy-match": [
    {
      "proxies": ["proxy1"],
      "domains": ["*.example.com"],
      "subnets": ["192.168.1.0/24"],
      "ports": [443]
    }
  ]
})";

// An extra version of above response to test updating config works.
const char kValidPvdJson1Updated[] = R"({
  "identifier": "domain1.example.com",
  "proxies": [
    {
      "protocol": "https-connect",
      "identity": "proxy1_updated",
      "proxy": "https://proxy1-updated.example.com:443"
    }
  ],
  "proxy-match": [
    {
      "proxies": ["proxy1_updated"],
      "domains": ["*.example.com"]
    }
  ]
})";

const char kValidPvdJson2[] = R"({
  "identifier": "domain2.example.com",
  "proxies": [
    {
      "protocol": "https-connect",
      "identity": "proxy2",
      "proxy": "proxy2.example.com:443"
    }
  ],
  "proxy-match": [
    {
      "proxies": ["proxy2"],
      "domains": ["*.domain2.com"]
    }
  ]
})";

base::DictValue CreateDomainPolicyEntry(
    const std::string& pvd_id,
    const std::string& auth_type = "none",
    const std::string& auth_scope = "",
    base::ListValue extra_headers = base::ListValue()) {
  base::DictValue entry;
  entry.Set("pvd_id", pvd_id);
  base::DictValue auth;
  auth.Set("type", auth_type);
  if (!auth_scope.empty()) {
    auth.Set("scope", auth_scope);
  }
  entry.Set("auth_config", std::move(auth));
  if (!extra_headers.empty()) {
    entry.Set("extra_headers", std::move(extra_headers));
  }
  return entry;
}

base::DictValue CreateDomainPolicyEntry(const std::string& pvd_id,
                                        bool use_oauth) {
  return CreateDomainPolicyEntry(pvd_id,
                                 use_oauth ? "profile_bearer_token" : "none",
                                 use_oauth ? "cloud_secure_gateway" : "");
}

class EnterpriseProxyServiceTest : public testing::Test {
 protected:
  EnterpriseProxyServiceTest() {
    RegisterProfilePrefs(pref_service_.registry());
  }

  void SetUp() override {
    scoped_allowed_domains_ =
        std::make_unique<ScopedExtraAllowedDomainsForTesting>(
            std::vector<std::string>{"example.com"});
    auth_service_ = std::make_unique<EnterpriseNetworkAuthService>(
        identity_test_env_.identity_manager(), &pref_service_,
        &profile_id_service_);
  }

  void TearDown() override {
    service_.reset();
    auth_service_.reset();
    scoped_allowed_domains_.reset();
  }

  void SetUpPrimaryAccount(const std::string& email = "user@managed.com") {
    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
    identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
        account_info.GetAccountId(), account_info.GetEmail(),
        account_info.GetGaiaId(), "managed.com", "Full Name", "Given Name",
        "en-US", "picture_url");
  }

  void CreateService(enterprise::ProfileIdService* profile_id_service = nullptr,
                     net::NetLog* net_log = nullptr) {
    if (!profile_id_service) {
      profile_id_service = &profile_id_service_;
    }
    if (!net_log) {
      net_log = net::NetLog::Get();
    }
    auto callback = base::BindRepeating(
        [](network::TestURLLoaderFactory* factory)
            -> scoped_refptr<network::SharedURLLoaderFactory> {
          return factory->GetSafeWeakWrapper();
        },
        base::Unretained(&test_url_loader_factory_));
    service_ = std::make_unique<EnterpriseProxyService>(
        &pref_service_, auth_service_.get(), std::move(callback),
        profile_id_service, net_log);
  }

  void SetPolicyDomains(const std::vector<std::string>& domain_ids,
                        bool use_oauth = false) {
    base::ListValue policy_domains;
    for (const auto& domain_id : domain_ids) {
      policy_domains.Append(CreateDomainPolicyEntry(domain_id, use_oauth));
    }
    pref_service_.SetList(kProxyProvisioningDomains, std::move(policy_domains));
  }

  base::test::TaskEnvironment task_environment_;
  net::RecordingNetLogObserver net_log_observer_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      mock_network_change_notifier_ =
          net::test::MockNetworkChangeNotifier::Create();
  TestingPrefServiceSimple pref_service_;
  enterprise::ProfileIdService profile_id_service_{"test_profile_id"};
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<EnterpriseNetworkAuthService> auth_service_;
  std::unique_ptr<EnterpriseProxyService> service_;
  std::unique_ptr<ScopedExtraAllowedDomainsForTesting> scoped_allowed_domains_;
};

TEST_F(EnterpriseProxyServiceTest, InitializesAndRegistersPrefs) {
  CreateService();
  EXPECT_TRUE(service_->GetProvisioningDomainConfigs().empty());
  EXPECT_FALSE(service_->IsRefreshInProgress());
  EXPECT_EQ(0, *service_->GetDebugInfo().FindInt("refreshing_configs_count"));
}

// Tests service width: Covers the complete lifecycle of EnterpriseProxyService
// when configured with multiple domain policies:
// - adding multiple domains
// - fetch orchestration counter transitions
// - merged dynamic routing
// - endpoint resolution
// - policy updates
// - service shutdown
TEST_F(EnterpriseProxyServiceTest, MultiDomainServiceLifecycle) {
  CreateService();
  MockObserver observer;
  service_->AddObserver(&observer);

  testing::InSequence seq;

  // Phase 1 Expectations: Initial 3 domains policy set.
  EXPECT_CALL(observer, OnDynamicProxyConfigsStatusChanged())
      .WillOnce([&]() {
        EXPECT_TRUE(service_->GetDynamicRoutingConfig().is_update_in_progress);
      })
      .WillOnce([&]() {
        EXPECT_FALSE(service_->GetDynamicRoutingConfig().is_update_in_progress);
        EXPECT_EQ(2u, service_->GetDynamicRoutingConfig().routing_rules.size());
      });

  // Phase 2 Expectations: Policy update reducing to 1 domain.
  EXPECT_CALL(observer, OnDynamicProxyConfigsStatusChanged())
      .WillOnce([&]() {
        EXPECT_TRUE(service_->GetDynamicRoutingConfig().is_update_in_progress);
      })
      .WillOnce([&]() {
        EXPECT_FALSE(service_->GetDynamicRoutingConfig().is_update_in_progress);
        EXPECT_EQ(1u, service_->GetDynamicRoutingConfig().routing_rules.size());
      });

  // Set 3 policy domains and run pending tasks to execute initial fetch calls.
  SetPolicyDomains({kTestDomain1, kTestDomain2, kTestFailDomain});
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  auto pause_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_NETWORK_PAUSE);
  ASSERT_EQ(1u, pause_entries.size());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, pause_entries[0].phase);

  EXPECT_TRUE(service_->IsRefreshInProgress());
  EXPECT_EQ(3, test_url_loader_factory_.NumPending());

  // Check initial debug info while 3 fetches are in progress.
  base::DictValue debug_info_initial = service_->GetDebugInfo();
  EXPECT_TRUE(*debug_info_initial.FindBool("is_refresh_in_progress"));
  EXPECT_EQ(3, *debug_info_initial.FindInt("refreshing_configs_count"));
  const base::ListValue* domains_list = debug_info_initial.FindList("domains");
  ASSERT_NE(nullptr, domains_list);
  EXPECT_EQ(3u, domains_list->size());

  // Fulfill fetches for domain1 and domain2, and fail domain3 (across 5xx
  // retries).
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return *service_->GetDebugInfo().FindInt("refreshing_configs_count") == 2;
  }));
  EXPECT_TRUE(service_->IsRefreshInProgress());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain2.example.com/.well-known/pvd", kValidPvdJson2);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return *service_->GetDebugInfo().FindInt("refreshing_configs_count") == 1;
  }));
  EXPECT_TRUE(service_->IsRefreshInProgress());

  while (test_url_loader_factory_.NumPending() > 0) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        "https://fail.example.com/.well-known/pvd", "",
        net::HTTP_INTERNAL_SERVER_ERROR);
  }
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));

  // All 3 fetches completed -> counter hits 0 and network pause ends.
  EXPECT_FALSE(service_->IsRefreshInProgress());

  pause_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_NETWORK_PAUSE);
  ASSERT_EQ(2u, pause_entries.size());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, pause_entries[0].phase);
  EXPECT_EQ(net::NetLogEventPhase::END, pause_entries[1].phase);

  base::DictValue debug_info_complete = service_->GetDebugInfo();
  EXPECT_FALSE(*debug_info_complete.FindBool("is_refresh_in_progress"));
  EXPECT_EQ(0, *debug_info_complete.FindInt("refreshing_configs_count"));

  // Verify GetMergedDynamicRoutingConfig returns merged network stack rules
  // from valid PvD configs in order.
  net::ProxyConfig::DynamicRoutingConfig merged_config =
      service_->GetDynamicRoutingConfig();
  EXPECT_EQ(2u, merged_config.routing_rules.size());

  // Verify FindMatchingProxyEndpoint resolves endpoints across domains.
  auto endpoint1 = service_->FindMatchingProxyEndpoint(
      GURL("https://foo.example.com/test"),
      MakeHttpsProxyChain("proxy1.example.com:443"));
  ASSERT_TRUE(endpoint1.has_value());
  EXPECT_EQ(MakeHttpsProxyChain("proxy1.example.com:443"),
            endpoint1->proxy_chain);

  auto endpoint2 = service_->FindMatchingProxyEndpoint(
      GURL("https://foo.domain2.com/test"),
      MakeHttpsProxyChain("proxy2.example.com:443"));
  ASSERT_TRUE(endpoint2.has_value());
  EXPECT_EQ(MakeHttpsProxyChain("proxy2.example.com:443"),
            endpoint2->proxy_chain);

  // Policy update: Reduce to only kTestDomain1.
  SetPolicyDomains({kTestDomain1});
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  EXPECT_TRUE(service_->IsRefreshInProgress());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));
  EXPECT_FALSE(service_->IsRefreshInProgress());

  EXPECT_EQ(1u, service_->GetProvisioningDomainConfigs().size());

  // Verify FindMatchingProxyEndpoint returns nullopt for domain2 after its
  // removal.
  auto removed_endpoint = service_->FindMatchingProxyEndpoint(
      GURL("https://foo.domain2.com/test"),
      MakeHttpsProxyChain("proxy2.example.com:443"));
  EXPECT_FALSE(removed_endpoint.has_value());

  // Service Shutdown: Verifies Shutdown clears all state and managers.
  service_->RemoveObserver(&observer);
  static_cast<KeyedService*>(service_.get())->Shutdown();
  EXPECT_FALSE(service_->IsRefreshInProgress());
  EXPECT_TRUE(service_->GetProvisioningDomainConfigs().empty());
  EXPECT_EQ(0, *service_->GetDebugInfo().FindInt("refreshing_configs_count"));
}

// Tests service depth: Covers the state lifecycle of a single domain config
// across:
// - success
// - forced refresh while in-flight
// - transient failure
// - recovery via another success
TEST_F(EnterpriseProxyServiceTest, SingleDomainLifecycleAndStateTransitions) {
  CreateService();
  MockObserver observer;
  service_->AddObserver(&observer);

  testing::InSequence seq;

  auto expect_refresh_cycle = [&](size_t expected_rules) {
    EXPECT_CALL(observer, OnDynamicProxyConfigsStatusChanged())
        .WillOnce([&]() {
          EXPECT_TRUE(service_->IsRefreshInProgress());
          EXPECT_TRUE(
              service_->GetDynamicRoutingConfig().is_update_in_progress);
        })
        .WillOnce([&, expected_rules]() {
          EXPECT_FALSE(service_->IsRefreshInProgress());
          EXPECT_FALSE(
              service_->GetDynamicRoutingConfig().is_update_in_progress);
          EXPECT_EQ(expected_rules,
                    service_->GetDynamicRoutingConfig().routing_rules.size());
        });
  };

  // A successful fetch following the initial policy value set.
  expect_refresh_cycle(1u);

  SetPolicyDomains({kTestDomain1});
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  EXPECT_TRUE(service_->IsRefreshInProgress());
  EXPECT_EQ(1, *service_->GetDebugInfo().FindInt("refreshing_configs_count"));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));
  EXPECT_FALSE(service_->IsRefreshInProgress());
  EXPECT_EQ(0, *service_->GetDebugInfo().FindInt("refreshing_configs_count"));

  EXPECT_EQ(1u, service_->GetProvisioningDomainConfigs().size());
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kValid,
            service_->GetProvisioningDomainConfigs()[0].state);

  // Two force refreshes (to test that inflight-fetches are cancelled), which
  // results in a transient failure.
  expect_refresh_cycle(1u);

  service_->ForceRefreshAllConfigs();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  EXPECT_TRUE(service_->IsRefreshInProgress());
  EXPECT_EQ(1, *service_->GetDebugInfo().FindInt("refreshing_configs_count"));

  // Call ForceRefreshAllConfigs again while fetch is already in-progress.
  service_->ForceRefreshAllConfigs();
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  EXPECT_TRUE(service_->IsRefreshInProgress());
  EXPECT_EQ(1, *service_->GetDebugInfo().FindInt("refreshing_configs_count"));

  // Simulate transient failure (HTTP 500) across 5xx retries.
  while (test_url_loader_factory_.NumPending() > 0) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        "https://domain1.example.com/.well-known/pvd", "",
        net::HTTP_INTERNAL_SERVER_ERROR);
  }
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));

  EXPECT_FALSE(service_->IsRefreshInProgress());
  EXPECT_EQ(0, *service_->GetDebugInfo().FindInt("refreshing_configs_count"));
  EXPECT_EQ(1u, service_->GetProvisioningDomainConfigs().size());
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedTransient,
            service_->GetProvisioningDomainConfigs()[0].state);

  // Verify that existing fetched config and endpoint remain accessible
  // despite kFailedTransient state.
  auto endpoint = service_->FindMatchingProxyEndpoint(
      GURL("https://foo.example.com/test"),
      MakeHttpsProxyChain("proxy1.example.com:443"));
  ASSERT_TRUE(endpoint.has_value());

  net::ProxyConfig::DynamicRoutingConfig merged_config =
      service_->GetDynamicRoutingConfig();
  EXPECT_EQ(1u, merged_config.routing_rules.size());

  // A successful force-fetch following the initial transient failure, which is
  // successful and recovered configs.
  expect_refresh_cycle(1u);
  service_->ForceRefreshAllConfigs();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  EXPECT_TRUE(service_->IsRefreshInProgress());
  EXPECT_EQ(1, *service_->GetDebugInfo().FindInt("refreshing_configs_count"));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1Updated);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));

  EXPECT_FALSE(service_->IsRefreshInProgress());
  EXPECT_EQ(0, *service_->GetDebugInfo().FindInt("refreshing_configs_count"));
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kValid,
            service_->GetProvisioningDomainConfigs()[0].state);

  // Verify that the new successful fetch overrides the old cached config.
  auto updated_endpoint = service_->FindMatchingProxyEndpoint(
      GURL("https://foo.example.com/test"),
      MakeHttpsProxyChain("proxy1-updated.example.com:443"));
  ASSERT_TRUE(updated_endpoint.has_value());
  EXPECT_EQ(MakeHttpsProxyChain("proxy1-updated.example.com:443"),
            updated_endpoint->proxy_chain);

  auto old_endpoint = service_->FindMatchingProxyEndpoint(
      GURL("https://foo.example.com/test"),
      MakeHttpsProxyChain("proxy1.example.com:443"));
  EXPECT_FALSE(old_endpoint.has_value());

  service_->RemoveObserver(&observer);
}

TEST_F(EnterpriseProxyServiceTest, OAuthAuthenticationFetch) {
  SetUpPrimaryAccount();
  CreateService();
  SetPolicyDomains({kTestDomain1}, /*use_oauth=*/true);

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "bearer_token_abc", base::Time::Max());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  // Verify authorization header contains bearer token.
  const network::ResourceRequest* request =
      &test_url_loader_factory_.GetPendingRequest(0)->request;
  std::optional<std::string> auth_header =
      request->headers.GetHeader(net::HttpRequestHeaders::kAuthorization);
  EXPECT_EQ("Bearer bearer_token_abc", auth_header.value_or(""));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));
  EXPECT_EQ(1u, service_->GetProvisioningDomainConfigs().size());
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kValid,
            service_->GetProvisioningDomainConfigs()[0].state);
}

TEST_F(EnterpriseProxyServiceTest,
       CachesResponsesIncrementallyAndPrunesOnPolicyChange) {
  CreateService();

  base::ListValue policy_domains;
  policy_domains.Append(CreateDomainPolicyEntry(kTestDomain1));
  policy_domains.Append(CreateDomainPolicyEntry(kTestDomain2));
  pref_service_.SetList(kProxyProvisioningDomains, std::move(policy_domains));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  // Simulate response for domain1 ONLY.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);

  // Verify pref cache dict is updated incrementally after domain1 completes:
  // domain1's cached state becomes "Valid" while domain2 remains
  // "RefreshNeeded".
  const base::DictValue& partial_cache =
      pref_service_.GetDict(kProvisioningDomainProxyConfigs);
  EXPECT_EQ(2u, partial_cache.size());

  for (const auto [key, value] : partial_cache) {
    ASSERT_TRUE(value.is_dict());
    const std::string* pvd_id =
        value.GetDict().FindStringByDottedPath("policy.pvd_id");
    ASSERT_NE(nullptr, pvd_id);
    const std::string* state =
        value.GetDict().FindStringByDottedPath("fetched_config.state");
    ASSERT_NE(nullptr, state);
    if (*pvd_id == kTestDomain1) {
      EXPECT_EQ("Valid", *state);
    } else if (*pvd_id == kTestDomain2) {
      EXPECT_EQ("RefreshNeeded", *state);
    }
  }

  // Simulate response for domain2.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain2.example.com/.well-known/pvd", kValidPvdJson2);

  // Verify pref cache dict contains entries for both domains.
  const base::DictValue& full_cache =
      pref_service_.GetDict(kProvisioningDomainProxyConfigs);
  EXPECT_EQ(2u, full_cache.size());

  // Remove domain2 from policy.
  base::ListValue updated_domains;
  updated_domains.Append(CreateDomainPolicyEntry(kTestDomain1));
  pref_service_.SetList(kProxyProvisioningDomains, std::move(updated_domains));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  // Finish background refresh for domain1.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);

  // Verify updated cache contains only 1 entry for domain1.
  const base::DictValue& updated_cache =
      pref_service_.GetDict(kProvisioningDomainProxyConfigs);
  EXPECT_EQ(1u, updated_cache.size());
}

TEST_F(EnterpriseProxyServiceTest,
       RestoresActiveRoutesFromCachedConfigOnStartup) {
  CreateService();

  // Populate initial cache.
  base::ListValue policy_domains;
  policy_domains.Append(CreateDomainPolicyEntry(kTestDomain1));
  pref_service_.SetList(kProxyProvisioningDomains, std::move(policy_domains));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);

  ASSERT_FALSE(service_->IsRefreshInProgress());

  // Re-create service (simulating Chrome startup with populated cache).
  service_.reset();
  CreateService();

  // Verify active routes are immediately available from cache BEFORE
  // simulating network response.
  const auto endpoint = service_->FindMatchingProxyEndpoint(
      GURL("https://foo.example.com/test"),
      MakeHttpsProxyChain("proxy1.example.com:443"));
  ASSERT_TRUE(endpoint.has_value());
  ASSERT_TRUE(endpoint->auth.has_value());
  EXPECT_EQ(AuthType::kProfileBearerToken, endpoint->auth->type);
  EXPECT_EQ(AuthScope::kCloudSecureGateway, endpoint->auth->scope);
  EXPECT_EQ(2u, endpoint->extra_headers.size());
}

TEST_F(EnterpriseProxyServiceTest, NetworkChangeTriggersRefresh) {
  CreateService();
  MockObserver observer;
  service_->AddObserver(&observer);

  // Initial policy set triggers fetch start and completion.
  EXPECT_CALL(observer, OnDynamicProxyConfigsStatusChanged()).Times(2);
  SetPolicyDomains({kTestDomain1, kTestDomain2});

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain2.example.com/.well-known/pvd", kValidPvdJson2);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // CONNECTION_NONE should NOT trigger a refresh.
  EXPECT_CALL(observer, OnDynamicProxyConfigsStatusChanged()).Times(0);
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_FALSE(service_->IsRefreshInProgress());
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Active connection change (e.g. CONNECTION_WIFI) SHOULD trigger a refresh
  // for all domains.
  EXPECT_CALL(observer, OnDynamicProxyConfigsStatusChanged()).Times(2);
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1Updated);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain2.example.com/.well-known/pvd", kValidPvdJson2);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));
  testing::Mock::VerifyAndClearExpectations(&observer);

  service_->RemoveObserver(&observer);
}

TEST_F(EnterpriseProxyServiceTest, AccountChangeTriggersRefresh) {
  CreateService();
  SetPolicyDomains({kTestDomain1}, /*use_oauth=*/true);

  // Initial fetch fails with blocked transient error because there is no
  // primary account.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));
  EXPECT_EQ(0u, service_->GetDynamicRoutingConfig().routing_rules.size());
  std::vector<ProvisioningDomainProxyConfig> configs =
      service_->GetProvisioningDomainConfigs();
  ASSERT_EQ(1u, configs.size());
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedBlocked,
            configs[0].state);

  MockObserver observer;
  service_->AddObserver(&observer);

  // Sign in primary account automatically triggers refresh (start + finish).
  EXPECT_CALL(observer, OnDynamicProxyConfigsStatusChanged())
      .Times(testing::AtLeast(2));
  SetUpPrimaryAccount();

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "test_token", base::Time::Max());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));
  testing::Mock::VerifyAndClearExpectations(&observer);

  configs = service_->GetProvisioningDomainConfigs();
  ASSERT_EQ(1u, configs.size());
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kValid, configs[0].state);
  EXPECT_EQ(1u, service_->GetDynamicRoutingConfig().routing_rules.size());

  service_->RemoveObserver(&observer);
}

TEST_F(EnterpriseProxyServiceTest, RouteFlushingOnAuthFailureAndRecovery) {
  CreateService();
  SetUpPrimaryAccount();
  SetPolicyDomains({kTestDomain1}, /*use_oauth=*/true);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "test_token", base::Time::Max());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));
  EXPECT_EQ(1u, service_->GetDynamicRoutingConfig().routing_rules.size());
  std::vector<ProvisioningDomainProxyConfig> configs =
      service_->GetProvisioningDomainConfigs();
  ASSERT_EQ(1u, configs.size());
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kValid, configs[0].state);

  // Invalidate refresh token with auth error.
  CoreAccountId primary_account_id =
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin);

  // Trigger refresh. Token fetch fails with auth error -> transitions to
  // kFailedBlocked.
  service_->ForceRefreshAllConfigs();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));

  // Verify that active routes are PRESERVED on blocked state for maximal
  // availability.
  EXPECT_EQ(1u, service_->GetDynamicRoutingConfig().routing_rules.size());
  configs = service_->GetProvisioningDomainConfigs();
  ASSERT_EQ(1u, configs.size());
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedBlocked,
            configs[0].state);

  // Recovery: Credentials resolved / new token issued.
  identity_test_env_.SetRefreshTokenForAccount(primary_account_id);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "new_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));

  // Routes are restored!
  EXPECT_EQ(1u, service_->GetDynamicRoutingConfig().routing_rules.size());
  configs = service_->GetProvisioningDomainConfigs();
  ASSERT_EQ(1u, configs.size());
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kValid, configs[0].state);
}

TEST_F(EnterpriseProxyServiceTest, MalformedPolicyDoesNotRefresh) {
  CreateService();
  MockObserver observer;
  service_->AddObserver(&observer);

  // Set policy with one malformed entry and one valid entry.
  EXPECT_CALL(observer, OnDynamicProxyConfigsStatusChanged()).Times(2);
  base::ListValue policy_domains;
  base::DictValue malformed_entry;
  malformed_entry.Set("invalid_field", "value");
  policy_domains.Append(std::move(malformed_entry));
  policy_domains.Append(
      CreateDomainPolicyEntry(kTestDomain1, /*use_oauth=*/false));
  pref_service_.SetList(kProxyProvisioningDomains, std::move(policy_domains));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));
  // Only the valid domain produces a request.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));
  testing::Mock::VerifyAndClearExpectations(&observer);

  std::vector<ProvisioningDomainProxyConfig> configs =
      service_->GetProvisioningDomainConfigs();
  ASSERT_EQ(2u, configs.size());
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedPermanent,
            configs[0].state);
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kValid, configs[1].state);

  // Trigger network change.
  EXPECT_CALL(observer, OnDynamicProxyConfigsStatusChanged()).Times(2);
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));
  // Only the valid domain produces a request; the malformed one does not.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://domain1.example.com/.well-known/pvd", kValidPvdJson1);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service_->IsRefreshInProgress(); }));
  testing::Mock::VerifyAndClearExpectations(&observer);

  configs = service_->GetProvisioningDomainConfigs();
  ASSERT_EQ(2u, configs.size());
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedPermanent,
            configs[0].state);
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kValid, configs[1].state);

  service_->RemoveObserver(&observer);
}

TEST_F(EnterpriseProxyServiceTest, NetLogEmittedOnShutdownDuringNetworkPause) {
  CreateService();

  SetPolicyDomains({kTestDomain1});
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  auto pause_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_NETWORK_PAUSE);
  ASSERT_EQ(1u, pause_entries.size());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, pause_entries[0].phase);

  service_->Shutdown();

  pause_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_NETWORK_PAUSE);
  ASSERT_EQ(2u, pause_entries.size());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, pause_entries[0].phase);
  EXPECT_EQ(net::NetLogEventPhase::END, pause_entries[1].phase);
}

class EnterpriseProxyServiceAuthChallengeTest
    : public EnterpriseProxyServiceTest {
 protected:
  using Decision = EnterpriseProxyService::ProxyAuthChallengeMatch::Decision;
  using Outcome = EnterpriseProxyService::CredentialFetchOutcome;

  void SetUp() override {
    EnterpriseProxyServiceTest::SetUp();
    SetUpPrimaryAccount();
    CreateService();
    // Configure default managed domain using existing kValidPvdJson1.
    SetUpDomainAndSimulateResponse(kTestDomain1, kValidPvdJson1);
  }

  void SetUpDomainAndSimulateResponse(std::string_view pvd_id,
                                      std::string_view json_response) {
    base::ListValue policy_domains;
    policy_domains.Append(CreateDomainPolicyEntry(std::string(pvd_id), false));
    pref_service_.SetList(kProxyProvisioningDomains, std::move(policy_domains));
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL("https://" + std::string(pvd_id) + "/.well-known/pvd").spec(),
        std::string(json_response));
  }

  net::AuthChallengeInfo CreateProxyAuthChallengeInfo(
      std::string_view host = "proxy1.example.com",
      uint16_t port = 443,
      std::string_view realm = "") {
    net::AuthChallengeInfo auth_info;
    auth_info.is_proxy = true;
    auth_info.challenger =
        url::SchemeHostPort("https", std::string(host), port);
    if (!realm.empty()) {
      auth_info.realm = std::string(realm);
    }
    return auth_info;
  }

  EnterpriseProxyService::ProxyAuthChallengeMatch Classify(
      const net::AuthChallengeInfo& auth_info,
      const GURL& destination_url) {
    return service_->ClassifyProxyAuthChallenge(auth_info, destination_url,
                                                nullptr);
  }

  void ExpectDecisionHistogram(
      const base::HistogramTester& histogram_tester,
      EnterpriseProxyService::ProxyAuthChallengeMatch::Decision expected) {
    histogram_tester.ExpectUniqueSample(
        "Enterprise.SecureGateway.ProxyAuthChallengeDecision", expected, 1);
  }

  void ExpectFetchOutcomeHistogram(
      const base::HistogramTester& histogram_tester,
      EnterpriseProxyService::CredentialFetchOutcome expected) {
    histogram_tester.ExpectUniqueSample(
        "Enterprise.SecureGateway.ProxyAuthCredentialFetchOutcome", expected,
        1);
  }

  using AuthChallengeFuture =
      base::test::TestFuture<EnterpriseProxyService::CredentialFetchOutcome,
                             const std::optional<net::AuthCredentials>&,
                             const net::NetLogWithSource&>;
};

TEST_F(EnterpriseProxyServiceAuthChallengeTest, NotProxyChallenge) {
  base::HistogramTester histogram_tester;
  net::AuthChallengeInfo auth_info = CreateProxyAuthChallengeInfo();
  auth_info.is_proxy = false;

  EXPECT_EQ(Decision::kNotApplicable,
            Classify(auth_info, GURL("https://foo.example.com/test")).decision);
  ExpectDecisionHistogram(histogram_tester, Decision::kNotApplicable);
}

TEST_F(EnterpriseProxyServiceAuthChallengeTest, NotManagedProxy) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(Decision::kNotApplicable,
            Classify(CreateProxyAuthChallengeInfo("unmanaged-proxy.com"),
                     GURL("https://foo.example.com/test"))
                .decision);
  ExpectDecisionHistogram(histogram_tester, Decision::kNotApplicable);
}

TEST_F(EnterpriseProxyServiceAuthChallengeTest, InvalidDestinationUrl) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(Decision::kNotApplicable,
            Classify(CreateProxyAuthChallengeInfo("proxy1.example.com"), GURL())
                .decision);
  ExpectDecisionHistogram(histogram_tester, Decision::kNotApplicable);
}

TEST_F(EnterpriseProxyServiceAuthChallengeTest, NoCredentialsNeeded) {
  base::HistogramTester histogram_tester;
  SetUpDomainAndSimulateResponse(kTestDomain2, kValidPvdJson2);

  EXPECT_EQ(Decision::kNoCredentialsNeeded,
            Classify(CreateProxyAuthChallengeInfo("proxy2.example.com"),
                     GURL("https://foo.domain2.com/test"))
                .decision);
  ExpectDecisionHistogram(histogram_tester, Decision::kNoCredentialsNeeded);

  // kValidPvdJson2 advertises no `google_chrome.auth` block at all. That is a
  // different misconfiguration from a route whose auth block asks for
  // something we cannot supply, and the two share a histogram bucket, so the
  // NetLog reason is the only way to tell them apart.
  auto resolved_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_EQ(1u, resolved_entries.size());
  EXPECT_EQ("no_auth_config_advertised",
            *resolved_entries[0].params.FindString("failure_reason"));
}

TEST_F(EnterpriseProxyServiceAuthChallengeTest, DisguisedErrorRealm) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(
      Decision::kDisguisedError,
      Classify(CreateProxyAuthChallengeInfo("proxy1.example.com", 443, "403"),
               GURL("https://foo.example.com/test"))
          .decision);
  ExpectDecisionHistogram(histogram_tester, Decision::kDisguisedError);
}

// The forcing param stands in for the identity layer rejecting the account, so
// it must surface where a real rejection does: out of the credential fetch,
// leaving classification untouched.
TEST_F(EnterpriseProxyServiceAuthChallengeTest, ForcedSignInRequired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnterpriseProxyErrorHandling, {{kForceSignInRequiredParamName, "true"}});

  base::HistogramTester histogram_tester;

  auto match = Classify(CreateProxyAuthChallengeInfo("proxy1.example.com"),
                        GURL("https://foo.example.com/test"));
  ASSERT_EQ(Decision::kNeedsCredentials, match.decision);

  AuthChallengeFuture future;
  service_->FetchProxyAuthCredentials(std::move(match), future.GetCallback());

  // Resolved without consulting the identity layer at all.
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(Outcome::kSignInRequired, future.Get<0>());
  EXPECT_FALSE(future.Get<1>().has_value());
  ExpectFetchOutcomeHistogram(histogram_tester, Outcome::kSignInRequired);

  auto resolved_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_EQ(1u, resolved_entries.size());
  EXPECT_EQ("forced_sign_in_required",
            *resolved_entries[0].params.FindString("failure_reason"));
}

// Classification must be fully synchronous. The original bug reported
// `kNotApplicable` by writing through a pointer to the caller's stack local,
// which is only reachable if classification can answer after the caller has
// returned. Nothing here may start a fetch or queue a task.
TEST_F(EnterpriseProxyServiceAuthChallengeTest, ClassifyDoesNotStartAsyncWork) {
  base::HistogramTester histogram_tester;
  size_t pending_tasks_before =
      task_environment_.GetPendingMainThreadTaskCount();

  // A terminal decision must be fully resolved, and its result already
  // recorded, by the time Classify() returns -- with no run loop in between.
  EXPECT_EQ(Decision::kNotApplicable,
            Classify(CreateProxyAuthChallengeInfo("unmanaged-proxy.com"),
                     GURL("https://foo.example.com/test"))
                .decision);
  histogram_tester.ExpectBucketCount(
      "Enterprise.SecureGateway.ProxyAuthChallengeDecision",
      Decision::kNotApplicable, 1);

  // The non-terminal decision must not start the token fetch either; that
  // belongs to FetchProxyAuthCredentials(), which this test never calls.
  EXPECT_EQ(Decision::kNeedsCredentials,
            Classify(CreateProxyAuthChallengeInfo("proxy1.example.com"),
                     GURL("https://foo.example.com/test"))
                .decision);
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());
  histogram_tester.ExpectBucketCount(
      "Enterprise.SecureGateway.ProxyAuthChallengeDecision",
      Decision::kNeedsCredentials, 1);

  // Classification records the decision stage only. The fetch stage belongs to
  // FetchProxyAuthCredentials(), so its histogram must still be empty.
  histogram_tester.ExpectTotalCount(
      "Enterprise.SecureGateway.ProxyAuthCredentialFetchOutcome", 0);

  // Neither classification may have queued anything, so dropping the
  // `kNeedsCredentials` match above is inert rather than merely survivable.
  EXPECT_EQ(pending_tasks_before,
            task_environment_.GetPendingMainThreadTaskCount());
}

TEST_F(EnterpriseProxyServiceAuthChallengeTest, CredentialFetchSuccess) {
  base::HistogramTester histogram_tester;
  pref_service_.registry()->RegisterStringPref(
      language::prefs::kAcceptLanguages, std::string());
  pref_service_.SetString(language::prefs::kAcceptLanguages, "en-US,en;q=0.9");

  auto match = Classify(CreateProxyAuthChallengeInfo("proxy1.example.com"),
                        GURL("https://foo.example.com/test"));
  ASSERT_EQ(Decision::kNeedsCredentials, match.decision);

  AuthChallengeFuture future;
  service_->FetchProxyAuthCredentials(std::move(match), future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());

  EXPECT_EQ(Outcome::kSuccess, future.Get<0>());
  ExpectFetchOutcomeHistogram(histogram_tester, Outcome::kSuccess);

  ASSERT_TRUE(future.Get<1>().has_value());
  EXPECT_EQ(u"access_token", future.Get<1>()->password());

  std::string expected_username =
      "X-Client-ID=test_client"
      "&X-Profile-ID=test_profile_id";
  EXPECT_EQ(base::UTF8ToUTF16(expected_username), future.Get<1>()->username());

  auto received_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RECEIVED);
  ASSERT_EQ(1u, received_entries.size());
  EXPECT_EQ("https://foo.example.com/test",
            *received_entries[0].params.FindString("destination_url"));

  auto resolved_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_EQ(1u, resolved_entries.size());
  EXPECT_EQ("token_acquired",
            *resolved_entries[0].params.FindString("decision"));
  EXPECT_FALSE(resolved_entries[0].params.FindString("failure_reason"));
  // The classification and the terminal result must share a NetLog source.
  EXPECT_EQ(received_entries[0].source.id, resolved_entries[0].source.id);
}

TEST_F(EnterpriseProxyServiceAuthChallengeTest, CredentialFetchFailure) {
  base::HistogramTester histogram_tester;
  auto match = Classify(CreateProxyAuthChallengeInfo("proxy1.example.com"),
                        GURL("https://foo.example.com/test"));
  ASSERT_EQ(Decision::kNeedsCredentials, match.decision);

  AuthChallengeFuture future;
  service_->FetchProxyAuthCredentials(std::move(match), future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromServiceUnavailable("error"));

  EXPECT_EQ(Outcome::kFailure, future.Get<0>());
  EXPECT_FALSE(future.Get<1>().has_value());
  ExpectFetchOutcomeHistogram(histogram_tester, Outcome::kFailure);

  auto resolved_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_EQ(1u, resolved_entries.size());
  EXPECT_EQ("failed", *resolved_entries[0].params.FindString("decision"));
  EXPECT_EQ("transient_error",
            *resolved_entries[0].params.FindString("failure_reason"));
}

TEST_F(EnterpriseProxyServiceAuthChallengeTest,
       SignInRequired_InvalidCredentials) {
  base::HistogramTester histogram_tester;
  auto match = Classify(CreateProxyAuthChallengeInfo("proxy1.example.com"),
                        GURL("https://foo.example.com/test"));
  ASSERT_EQ(Decision::kNeedsCredentials, match.decision);

  AuthChallengeFuture future;
  service_->FetchProxyAuthCredentials(std::move(match), future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  EXPECT_EQ(Outcome::kSignInRequired, future.Get<0>());
  EXPECT_FALSE(future.Get<1>().has_value());
  ExpectFetchOutcomeHistogram(histogram_tester, Outcome::kSignInRequired);

  auto resolved_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_EQ(1u, resolved_entries.size());
  EXPECT_EQ("sign_in_required",
            *resolved_entries[0].params.FindString("decision"));
  EXPECT_EQ("invalid_credentials",
            *resolved_entries[0].params.FindString("failure_reason"));
}

TEST_F(EnterpriseProxyServiceAuthChallengeTest,
       ShutdownDuringPendingAuthRequest) {
  auto match = Classify(CreateProxyAuthChallengeInfo("proxy1.example.com"),
                        GURL("https://foo.example.com/test"));
  ASSERT_EQ(Decision::kNeedsCredentials, match.decision);

  AuthChallengeFuture future;
  service_->FetchProxyAuthCredentials(std::move(match), future.GetCallback());

  service_->Shutdown();

  EXPECT_EQ(Outcome::kFailure, future.Get<0>());
  EXPECT_FALSE(future.Get<1>().has_value());

  auto resolved_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_EQ(1u, resolved_entries.size());
  EXPECT_EQ("failed", *resolved_entries[0].params.FindString("decision"));
  EXPECT_EQ("service_shutdown",
            *resolved_entries[0].params.FindString("failure_reason"));
}

TEST_F(EnterpriseProxyServiceAuthChallengeTest, InapplicableProxyServer) {
  base::HistogramTester histogram_tester;

  // Configure a domain with an untrusted proxy server host not in the allowed
  // domain list.
  constexpr char kUntrustedProxyPvdJson[] = R"({
    "identifier": "domain2.example.com",
    "proxies": [
      {
        "protocol": "https-connect",
        "identity": "untrusted_proxy",
        "proxy": "https://untrusted-proxy.com:443",
        "google_chrome": {
          "auth": {
            "type": "profile_bearer_token",
            "scope": "cloud_secure_gateway"
          }
        }
      }
    ],
    "proxy-match": [
      {
        "proxies": ["untrusted_proxy"],
        "domains": ["*.untrusted-match.com"]
      }
    ]
  })";
  SetUpDomainAndSimulateResponse(kTestDomain2, kUntrustedProxyPvdJson);

  auto match = Classify(CreateProxyAuthChallengeInfo("untrusted-proxy.com"),
                        GURL("https://foo.untrusted-match.com/test"));
  ASSERT_EQ(Decision::kNeedsCredentials, match.decision);

  AuthChallengeFuture future;
  service_->FetchProxyAuthCredentials(std::move(match), future.GetCallback());

  EXPECT_EQ(Outcome::kFailure, future.Get<0>());
  EXPECT_FALSE(future.Get<1>().has_value());
  ExpectFetchOutcomeHistogram(histogram_tester, Outcome::kFailure);

  auto resolved_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_EQ(1u, resolved_entries.size());
  EXPECT_EQ("inapplicable_server",
            *resolved_entries[0].params.FindString("failure_reason"));
}

// Regression test: destroying the service without calling Shutdown() used to
// silently drop every pending callback. Callers such as iOS treat a dropped
// callback as fatal, so the callback must still run.
TEST_F(EnterpriseProxyServiceAuthChallengeTest,
       DestructionWithoutShutdownRunsPendingCallbacks) {
  auto match = Classify(CreateProxyAuthChallengeInfo("proxy1.example.com"),
                        GURL("https://foo.example.com/test"));
  ASSERT_EQ(Decision::kNeedsCredentials, match.decision);

  AuthChallengeFuture future;
  service_->FetchProxyAuthCredentials(std::move(match), future.GetCallback());
  ASSERT_FALSE(future.IsReady());

  service_.reset();

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(Outcome::kFailure, future.Get<0>());
  EXPECT_FALSE(future.Get<1>().has_value());

  auto resolved_entries = net_log_observer_.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED);
  ASSERT_EQ(1u, resolved_entries.size());
  EXPECT_EQ("request_destroyed",
            *resolved_entries[0].params.FindString("failure_reason"));
}

// Coalesced challenges share a single token fetch; every one of their
// callbacks must still run exactly once when the request is torn down.
TEST_F(EnterpriseProxyServiceAuthChallengeTest,
       CoalescedRequestsAllRunOnDestruction) {
  AuthChallengeFuture first_future;
  AuthChallengeFuture second_future;

  auto first = Classify(CreateProxyAuthChallengeInfo("proxy1.example.com"),
                        GURL("https://foo.example.com/one"));
  ASSERT_EQ(Decision::kNeedsCredentials, first.decision);
  service_->FetchProxyAuthCredentials(std::move(first),
                                      first_future.GetCallback());

  auto second = Classify(CreateProxyAuthChallengeInfo("proxy1.example.com"),
                         GURL("https://foo.example.com/two"));
  ASSERT_EQ(Decision::kNeedsCredentials, second.decision);
  service_->FetchProxyAuthCredentials(std::move(second),
                                      second_future.GetCallback());

  ASSERT_FALSE(first_future.IsReady());
  ASSERT_FALSE(second_future.IsReady());

  service_.reset();

  EXPECT_TRUE(first_future.IsReady());
  EXPECT_TRUE(second_future.IsReady());
  EXPECT_EQ(Outcome::kFailure, first_future.Get<0>());
  EXPECT_EQ(Outcome::kFailure, second_future.Get<0>());
}

}  // namespace
}  // namespace enterprise_net
