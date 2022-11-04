// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/proxy_policy_provider.h"
#include <memory>
#include "base/callback.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;

namespace policy {

class ProxyPolicyProviderTest : public testing::Test {
 protected:
  ProxyPolicyProviderTest() {
    mock_provider_.Init();
    proxy_provider_.Init(&schema_registry_);
    proxy_provider_.AddObserver(&observer_);
  }
  ProxyPolicyProviderTest(const ProxyPolicyProviderTest&) = delete;
  ProxyPolicyProviderTest& operator=(const ProxyPolicyProviderTest&) = delete;

  ~ProxyPolicyProviderTest() override {
    proxy_provider_.RemoveObserver(&observer_);
    proxy_provider_.Shutdown();
    mock_provider_.Shutdown();
  }

  SchemaRegistry schema_registry_;
  MockConfigurationPolicyObserver observer_;
  MockConfigurationPolicyProvider mock_provider_;
  ProxyPolicyProvider proxy_provider_;
};

TEST_F(ProxyPolicyProviderTest, Init) {
  EXPECT_TRUE(proxy_provider_.IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(proxy_provider_.policies()));
}

TEST_F(ProxyPolicyProviderTest, Delegate) {
  PolicyBundle bundle;
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set("policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);
  mock_provider_.UpdatePolicy(bundle.Clone());

  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_));
  proxy_provider_.SetDelegate(&mock_provider_);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(bundle.Equals(proxy_provider_.policies()));

  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_));
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set("policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_CLOUD, base::Value("new value"), nullptr);
  mock_provider_.UpdatePolicy(bundle.Clone());
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(bundle.Equals(proxy_provider_.policies()));

  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_));
  proxy_provider_.SetDelegate(NULL);
  EXPECT_TRUE(PolicyBundle().Equals(proxy_provider_.policies()));
}

TEST_F(ProxyPolicyProviderTest, RefreshPolicies) {
  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_));
  proxy_provider_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_));
  proxy_provider_.SetDelegate(&mock_provider_);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_)).Times(0);
  EXPECT_CALL(mock_provider_, RefreshPolicies());
  proxy_provider_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&mock_provider_);

  EXPECT_CALL(observer_, OnUpdatePolicy(&proxy_provider_));
  mock_provider_.UpdatePolicy(PolicyBundle());
  Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace policy
