// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_service.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
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

  MOCK_METHOD(void,
              OnAllDynamicProxyConfigsResolved,
              (const std::vector<ProvisioningDomainProxyConfig>& configs),
              (override));
  MOCK_METHOD(void, OnDynamicProxyConfigsUpdateInProgress, (), (override));
};

constexpr char kTestDomain1[] = "domain1.example.com";
constexpr char kTestDomain2[] = "domain2.example.com";
constexpr char kTestFailDomain[] = "fail.example.com";

const char kValidPvdJson1[] = R"({
  "identifier": "domain1.example.com",
  "proxies": [
    {
      "protocol": "https-connect",
      "identity": "proxy1",
      "proxy": "https://proxy1.example.com:443"
    }
  ],
  "proxy-match": [
    {
      "proxies": ["proxy1"],
      "domains": ["*.example.com"]
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
      "proxy": "https://proxy2.example.com:443"
    }
  ],
  "proxy-match": [
    {
      "proxies": ["proxy2"],
      "domains": ["*.domain2.com"]
    }
  ]
})";

base::DictValue CreateDomainPolicyEntry(const std::string& pvd_id,
                                        bool use_oauth = false) {
  base::DictValue entry;
  entry.Set("pvd_id", pvd_id);
  base::DictValue auth;
  if (use_oauth) {
    auth.Set("type", "profile_bearer_token");
    auth.Set("scope", "cloud_secure_gateway");
  } else {
    auth.Set("type", "none");
  }
  entry.Set("auth_config", std::move(auth));
  return entry;
}

class EnterpriseProxyServiceTest : public testing::Test {
 protected:
  EnterpriseProxyServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(kEnableDynamicRouteFetching);
    RegisterProfilePrefs(pref_service_.registry());
  }

  void SetUp() override {
    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        "user@managed.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        "managed.com", "Full Name", "Given Name", "en-US", "picture_url");
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    auth_service_ = std::make_unique<EnterpriseNetworkAuthService>(
        identity_test_env_.identity_manager(), &pref_service_,
        &profile_id_service_);
  }

  void CreateService() {
    auto callback = base::BindRepeating(
        [](network::TestURLLoaderFactory* factory)
            -> scoped_refptr<network::SharedURLLoaderFactory> {
          return factory->GetSafeWeakWrapper();
        },
        base::Unretained(&test_url_loader_factory_));
    service_ = std::make_unique<EnterpriseProxyService>(
        &pref_service_, auth_service_.get(), std::move(callback),
        &profile_id_service_);
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
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  enterprise::ProfileIdService profile_id_service_{"test_profile_id"};
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<EnterpriseNetworkAuthService> auth_service_;
  std::unique_ptr<EnterpriseProxyService> service_;
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
  EXPECT_CALL(observer, OnDynamicProxyConfigsUpdateInProgress()).Times(1);
  EXPECT_CALL(observer, OnAllDynamicProxyConfigsResolved(testing::_))
      .WillOnce([&](const std::vector<ProvisioningDomainProxyConfig>& configs) {
        ASSERT_EQ(3u, configs.size());
        EXPECT_EQ(kTestDomain1, configs[0].pvd_id);
        EXPECT_EQ(ProvisioningDomainProxyConfig::State::kValid,
                  configs[0].state);

        EXPECT_EQ(kTestDomain2, configs[1].pvd_id);
        EXPECT_EQ(ProvisioningDomainProxyConfig::State::kValid,
                  configs[1].state);

        EXPECT_EQ(kTestFailDomain, configs[2].pvd_id);
        EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedTransient,
                  configs[2].state);
      });

  // Phase 2 Expectations: Policy update reducing to 1 domain.
  EXPECT_CALL(observer, OnDynamicProxyConfigsUpdateInProgress()).Times(1);
  EXPECT_CALL(observer, OnAllDynamicProxyConfigsResolved(testing::_)).Times(1);

  // Set 3 policy domains and run pending tasks to execute initial fetch calls.
  SetPolicyDomains({kTestDomain1, kTestDomain2, kTestFailDomain});
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

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

  // All 3 fetches completed -> counter hits 0.
  EXPECT_FALSE(service_->IsRefreshInProgress());

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

  // A successful fetch following the initial policy value set.
  EXPECT_CALL(observer, OnDynamicProxyConfigsUpdateInProgress()).Times(1);
  EXPECT_CALL(observer, OnAllDynamicProxyConfigsResolved(testing::_)).Times(1);

  SetPolicyDomains({kTestDomain1}, /*use_oauth=*/true);
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
  EXPECT_CALL(observer, OnDynamicProxyConfigsUpdateInProgress()).Times(1);
  EXPECT_CALL(observer, OnAllDynamicProxyConfigsResolved(testing::_)).Times(1);

  service_->ForceRefreshAllConfigs();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsRefreshInProgress(); }));

  EXPECT_TRUE(service_->IsRefreshInProgress());
  EXPECT_EQ(1, *service_->GetDebugInfo().FindInt("refreshing_configs_count"));

  // Call ForceRefreshAllConfigs again while fetch is already in-progress.
  service_->ForceRefreshAllConfigs();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return test_url_loader_factory_.NumPending() == 1; }));

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

  // A successful force-fetch following the transient failure, which is
  // successful and recovered configs.
  EXPECT_CALL(observer, OnDynamicProxyConfigsUpdateInProgress()).Times(1);
  EXPECT_CALL(observer, OnAllDynamicProxyConfigsResolved(testing::_)).Times(1);
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

}  // namespace
}  // namespace enterprise_net
