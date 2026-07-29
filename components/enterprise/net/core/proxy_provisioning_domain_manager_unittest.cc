// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/proxy_provisioning_domain_manager.h"

#include <memory>
#include <vector>

#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/provisioning_domain_fetcher.h"
#include "components/enterprise/net/core/types.h"
#include "components/enterprise/net/core/utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_net {
namespace {

constexpr char kTestDomain[] = "api.example.com";
constexpr char kTestUrl[] = "https://api.example.com/.well-known/pvd";

constexpr char kTestPvdJson[] = R"({
  "identifier": "api.example.com",
  "expires": "Wed, 21 Oct 2026 07:28:00 GMT",
  "proxies": [
    {
      "identifier": "proxy1",
      "protocol": "https-connect",
      "proxy": "proxy1.example.com:443"
    },
    {
      "identifier": "proxy2",
      "protocol": "https-connect",
      "proxy": "proxy2.example.com:443"
    }
  ],
  "proxy-match": [
    {
      "proxies": ["proxy1"],
      "domains": ["*.secure.com"]
    },
    {
      "proxies": ["DIRECT"],
      "domains": ["internal.direct.com"]
    },
    {
      "proxies": ["proxy2"],
      "subnets": ["10.0.0.0/8"]
    }
  ]
})";

class MockDomainObserver : public ProxyProvisioningDomainManager::Observer {
 public:
  MOCK_METHOD(void,
              OnProvisioningDomainStateChanged,
              (ProxyProvisioningDomainManager * domain_manager),
              (override));
};

class ProxyProvisioningDomainManagerTest : public testing::Test {
 public:
  ProxyProvisioningDomainManagerTest() {
    pref_service_.registry()->RegisterStringPref("intl.accept_languages",
                                                 "en-US,en;q=0.9");
  }

 protected:
  std::unique_ptr<EnterpriseNetworkAuthService> CreateAuthService() {
    return std::make_unique<EnterpriseNetworkAuthService>(
        identity_test_env_.identity_manager(), &pref_service_,
        &profile_id_service_);
  }

  std::unique_ptr<ProxyProvisioningDomainManager> CreateManager(
      const ProvisioningDomainConfig& policy,
      EnterpriseNetworkAuthService* auth_service) {
    auto url_loader_factory_callback = base::BindRepeating(
        [](network::TestURLLoaderFactory* test_url_loader_factory)
            -> scoped_refptr<network::SharedURLLoaderFactory> {
          return test_url_loader_factory->GetSafeWeakWrapper();
        },
        &test_url_loader_factory_);
    return std::make_unique<ProxyProvisioningDomainManager>(
        base::Value(ProvisioningDomainConfigToDict(policy)), auth_service,
        std::move(url_loader_factory_callback));
  }

  ProvisioningDomainConfig CreateTestPolicyConfig() {
    ProvisioningDomainConfig policy;
    policy.pvd_id = kTestDomain;
    return policy;
  }

  ProvisioningDomainConfig CreateTestPolicyConfigWithAuthAndHeaders() {
    ProvisioningDomainConfig policy;
    policy.pvd_id = kTestDomain;
    policy.auth_config = ProxyAuthConfig{
        .type = AuthType::kProfileBearerToken,
        .scope = AuthScope::kCloudSecureGateway,
    };
    policy.extra_headers = {
        ProxyExtraHeader("X-Client-ID", "test_client",
                         ProxyExtraHeader::HeaderType::kConstant),
        ProxyExtraHeader("X-Profile-ID", "${profile_id}",
                         ProxyExtraHeader::HeaderType::kVariable),
    };
    return policy;
  }

  void ExpectStateTransitions(
      MockDomainObserver& observer,
      ProxyProvisioningDomainManager* manager,
      const std::vector<ProvisioningDomainProxyConfig::State>& states) {
    auto& call =
        EXPECT_CALL(observer, OnProvisioningDomainStateChanged(manager));
    for (auto expected_state : states) {
      call.WillOnce(
          [manager, expected_state](ProxyProvisioningDomainManager* m) {
            EXPECT_EQ(manager, m);
            EXPECT_EQ(expected_state, m->state());
          });
    }
  }

  void ExpectStateTransitionTo(
      MockDomainObserver& observer,
      ProxyProvisioningDomainManager* manager,
      ProvisioningDomainProxyConfig::State final_state) {
    ExpectStateTransitions(
        observer, manager,
        {ProvisioningDomainProxyConfig::State::kFetching, final_state});
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple pref_service_;
  enterprise::ProfileIdService profile_id_service_{"test_profile_id"};
};

TEST_F(ProxyProvisioningDomainManagerTest,
       StateTransitionsAndRoutePreservation) {
  auto auth_service = CreateAuthService();
  MockDomainObserver observer;

  // On creation, manager automatically posts an initial casual refresh.
  auto manager = CreateManager(CreateTestPolicyConfig(), auth_service.get());
  manager->AddObserver(&observer);

  // Simulate successful HTTP response with diverse rules.
  ExpectStateTransitionTo(observer, manager.get(),
                          ProvisioningDomainProxyConfig::State::kValid);

  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestUrl, kTestPvdJson));

  EXPECT_FALSE(manager->is_refresh_in_progress());
  EXPECT_EQ(2u, manager->fetched_config().proxy_endpoints.size());
  EXPECT_EQ(3u, manager->fetched_config().routing_rules.size());
  EXPECT_EQ(std::vector<std::string>{"DIRECT"},
            manager->fetched_config().routing_rules[1].proxies);

  // Trigger another refresh using ForceRefresh that fails with transient HTTP
  // 500 error.
  ExpectStateTransitionTo(
      observer, manager.get(),
      ProvisioningDomainProxyConfig::State::kFailedTransient);

  manager->ForceRefresh();
  for (int i = 0; i < 3; ++i) {
    ASSERT_EQ(1, test_url_loader_factory_.NumPending());
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        kTestUrl, "", net::HTTP_INTERNAL_SERVER_ERROR);
  }
  EXPECT_FALSE(manager->is_refresh_in_progress());
  // Verify previous valid routes were PRESERVED!
  EXPECT_EQ(2u, manager->fetched_config().proxy_endpoints.size());
  EXPECT_EQ(3u, manager->fetched_config().routing_rules.size());

  // Trigger another refresh that fails with permanent JSON parse error.
  ExpectStateTransitionTo(
      observer, manager.get(),
      ProvisioningDomainProxyConfig::State::kFailedPermanent);

  manager->ForceRefresh();
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestUrl,
                                                             "{invalid_json");
  EXPECT_FALSE(manager->is_refresh_in_progress());
  // Verify previous valid routes were PRESERVED!
  EXPECT_EQ(2u, manager->fetched_config().proxy_endpoints.size());
  EXPECT_EQ(3u, manager->fetched_config().routing_rules.size());

  manager->RemoveObserver(&observer);
}

TEST_F(ProxyProvisioningDomainManagerTest, ForceRefreshCancelsInFlightFetch) {
  auto auth_service = CreateAuthService();
  auto manager = CreateManager(CreateTestPolicyConfig(), auth_service.get());

  MockDomainObserver observer;
  manager->AddObserver(&observer);

  ExpectStateTransitionTo(observer, manager.get(),
                          ProvisioningDomainProxyConfig::State::kValid);

  // ForceRefresh should cancel the in-flight fetch and re-trigger.
  manager->ForceRefresh();
  EXPECT_TRUE(manager->is_refresh_in_progress());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestUrl,
                                                             kTestPvdJson);
  EXPECT_FALSE(manager->is_refresh_in_progress());

  manager->RemoveObserver(&observer);
}

TEST_F(ProxyProvisioningDomainManagerTest, CancelRefreshAbortsInFlightRequest) {
  auto auth_service = CreateAuthService();
  auto manager = CreateManager(CreateTestPolicyConfig(), auth_service.get());

  MockDomainObserver observer;
  manager->AddObserver(&observer);

  // Wait for initial task to start refresh.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager->is_refresh_in_progress(); }));
  EXPECT_TRUE(manager->is_refresh_in_progress());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  manager->CancelRefresh();
  EXPECT_FALSE(manager->is_refresh_in_progress());

  manager->RemoveObserver(&observer);
}

TEST_F(ProxyProvisioningDomainManagerTest,
       GetDebugInfoContainsDetailedContent) {
  auto auth_service = CreateAuthService();
  auto manager = CreateManager(CreateTestPolicyConfigWithAuthAndHeaders(),
                               auth_service.get());

  // Verify debug info before fetch runs.
  base::DictValue debug_info_initial = manager->GetDebugInfo();
  const base::DictValue* policy_dict = debug_info_initial.FindDict("policy");
  ASSERT_NE(nullptr, policy_dict);
  EXPECT_EQ(kTestDomain, *policy_dict->FindString("pvd_id"));
  ASSERT_NE(nullptr, policy_dict->FindDict("auth_config"));
  EXPECT_EQ("profile_bearer_token",
            *policy_dict->FindDict("auth_config")->FindString("type"));

  const base::ListValue* extra_headers_list =
      policy_dict->FindList("extra_headers");
  ASSERT_NE(nullptr, extra_headers_list);

  base::ListValue expected_headers;
  {
    base::DictValue h1;
    h1.Set("key", "X-Client-ID");
    h1.Set("value", "test_client");
    h1.Set("type", "constant");
    expected_headers.Append(std::move(h1));

    base::DictValue h2;
    h2.Set("key", "X-Profile-ID");
    h2.Set("value", "${profile_id}");
    h2.Set("type", "variable");
    expected_headers.Append(std::move(h2));
  }
  EXPECT_EQ(*extra_headers_list, expected_headers);

  const base::DictValue* initial_config_dict =
      debug_info_initial.FindDict("fetched_config");
  ASSERT_NE(nullptr, initial_config_dict);
  EXPECT_EQ("RefreshNeeded", *initial_config_dict->FindString("state"));

  // Start refresh and complete successfully.
  auto manager_http =
      CreateManager(CreateTestPolicyConfig(), auth_service.get());

  MockDomainObserver observer;
  manager_http->AddObserver(&observer);

  ExpectStateTransitionTo(observer, manager_http.get(),
                          ProvisioningDomainProxyConfig::State::kValid);

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestUrl, kTestPvdJson));

  // Verify detailed debug info after successful refresh.
  base::DictValue debug_info = manager_http->GetDebugInfo();
  const base::DictValue* fetched_config_dict =
      debug_info.FindDict("fetched_config");
  ASSERT_NE(nullptr, fetched_config_dict);
  EXPECT_EQ(kTestDomain, *fetched_config_dict->FindString("pvd_id"));
  EXPECT_EQ("Valid", *fetched_config_dict->FindString("state"));

  const base::ListValue* proxies = fetched_config_dict->FindList("proxies");
  ASSERT_NE(nullptr, proxies);
  EXPECT_EQ(2u, proxies->size());

  const base::ListValue* proxy_match =
      fetched_config_dict->FindList("proxy-match");
  ASSERT_NE(nullptr, proxy_match);
  EXPECT_EQ(3u, proxy_match->size());

  manager_http->RemoveObserver(&observer);
}

TEST_F(ProxyProvisioningDomainManagerTest,
       ErrorClassificationTransientVsPermanent) {
  auto auth_service = CreateAuthService();

  // Test permanent error case with a 404.
  {
    auto manager = CreateManager(CreateTestPolicyConfig(), auth_service.get());
    ASSERT_EQ(1, test_url_loader_factory_.NumPending());
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        kTestUrl, "", net::HTTP_NOT_FOUND);
    EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedPermanent,
              manager->state());
  }

  // Test transient error case with network disconnection error.
  {
    auto manager = CreateManager(CreateTestPolicyConfig(), auth_service.get());
    ASSERT_EQ(1, test_url_loader_factory_.NumPending());
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kTestUrl),
        network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED),
        network::mojom::URLResponseHead::New(), "");
    EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedTransient,
              manager->state());
  }

  // Test permanent error case with auth token fetch without primary account.
  {
    auto manager = CreateManager(CreateTestPolicyConfigWithAuthAndHeaders(),
                                 auth_service.get());
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return manager->state() ==
             ProvisioningDomainProxyConfig::State::kFailedPermanent;
    }));
    EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedPermanent,
              manager->state());
  }

  // Test permanent error case with certificate error.
  {
    auto manager = CreateManager(CreateTestPolicyConfig(), auth_service.get());
    ASSERT_EQ(1, test_url_loader_factory_.NumPending());
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kTestUrl),
        network::URLLoaderCompletionStatus(net::ERR_CERT_COMMON_NAME_INVALID),
        network::mojom::URLResponseHead::New(), "");
    EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedPermanent,
              manager->state());
  }

  // Test transient error case with HTTP 429 Too Many Requests.
  {
    auto manager = CreateManager(CreateTestPolicyConfig(), auth_service.get());
    ASSERT_EQ(1, test_url_loader_factory_.NumPending());
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        kTestUrl, "", net::HTTP_TOO_MANY_REQUESTS);
    EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedTransient,
              manager->state());
  }
}

TEST_F(ProxyProvisioningDomainManagerTest, NullURLLoaderFactoryRecovery) {
  auto auth_service = CreateAuthService();
  bool return_valid_factory = false;

  auto url_loader_factory_callback = base::BindRepeating(
      [](network::TestURLLoaderFactory* test_url_loader_factory,
         bool* return_valid_factory)
          -> scoped_refptr<network::SharedURLLoaderFactory> {
        if (!*return_valid_factory) {
          return nullptr;
        }
        return test_url_loader_factory->GetSafeWeakWrapper();
      },
      &test_url_loader_factory_, &return_valid_factory);

  auto manager = std::make_unique<ProxyProvisioningDomainManager>(
      base::Value(ProvisioningDomainConfigToDict(CreateTestPolicyConfig())),
      auth_service.get(), std::move(url_loader_factory_callback));

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !manager->is_refresh_in_progress(); }));
  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedTransient,
            manager->state());

  // Make factory available and force refresh.
  return_valid_factory = true;
  manager->ForceRefresh();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager->is_refresh_in_progress(); }));

  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestUrl,
                                                             kTestPvdJson);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !manager->is_refresh_in_progress(); }));

  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kValid, manager->state());
}

TEST_F(ProxyProvisioningDomainManagerTest, HandlesMalformedPolicyDict) {
  auto auth_service = CreateAuthService();
  base::DictValue malformed_policy;
  malformed_policy.Set("invalid_key", "invalid_value");

  auto url_loader_factory_callback = base::BindRepeating(
      [](network::TestURLLoaderFactory* test_url_loader_factory)
          -> scoped_refptr<network::SharedURLLoaderFactory> {
        return test_url_loader_factory->GetSafeWeakWrapper();
      },
      &test_url_loader_factory_);

  auto manager = std::make_unique<ProxyProvisioningDomainManager>(
      base::Value(std::move(malformed_policy)), auth_service.get(),
      std::move(url_loader_factory_callback));

  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedPermanent,
            manager->state());
  EXPECT_FALSE(manager->is_refresh_in_progress());
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  base::DictValue debug_info = manager->GetDebugInfo();
  const std::string* state_str =
      debug_info.FindStringByDottedPath("fetched_config.state");
  ASSERT_NE(nullptr, state_str);
  EXPECT_EQ("FailedPermanent", *state_str);
}

TEST_F(ProxyProvisioningDomainManagerTest, HandlesNonDictPolicyValue) {
  auto auth_service = CreateAuthService();
  base::Value non_dict_policy("not_a_dictionary");

  auto url_loader_factory_callback = base::BindRepeating(
      [](network::TestURLLoaderFactory* test_url_loader_factory)
          -> scoped_refptr<network::SharedURLLoaderFactory> {
        return test_url_loader_factory->GetSafeWeakWrapper();
      },
      &test_url_loader_factory_);

  auto manager = std::make_unique<ProxyProvisioningDomainManager>(
      non_dict_policy, auth_service.get(),
      std::move(url_loader_factory_callback));

  EXPECT_EQ(ProvisioningDomainProxyConfig::State::kFailedPermanent,
            manager->state());
  EXPECT_FALSE(manager->is_refresh_in_progress());
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  base::DictValue debug_info = manager->GetDebugInfo();
  const std::string* state_str =
      debug_info.FindStringByDottedPath("fetched_config.state");
  ASSERT_NE(nullptr, state_str);
  EXPECT_EQ("FailedPermanent", *state_str);
}

}  // namespace
}  // namespace enterprise_net
