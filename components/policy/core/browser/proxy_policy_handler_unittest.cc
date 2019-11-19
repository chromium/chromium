// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/proxy_policy_handler.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Test cases for the proxy policy settings.
class ProxyPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
 public:
  void SetUp() override {
    ConfigurationPolicyPrefStoreTest::SetUp();
    handler_list_.AddHandler(
        base::WrapUnique<ConfigurationPolicyHandler>(new ProxyPolicyHandler));
    // Reset the PolicyServiceImpl to one that has the policy fixup
    // preprocessor. The previous store must be nulled out first so that it
    // removes itself from the service's observer list.
    store_ = nullptr;
    policy_service_ = std::make_unique<PolicyServiceImpl>(providers_);
    store_ = new ConfigurationPolicyPrefStore(
        nullptr, policy_service_.get(), &handler_list_, POLICY_LEVEL_MANDATORY);
  }

 protected:
  // Verify that all the proxy prefs are set to the specified expected values.
  void VerifyProxyPrefs(
      const std::string& expected_proxy_server,
      const std::string& expected_proxy_pac_url,
      const std::string& expected_proxy_bypass_list,
      const ProxyPrefs::ProxyMode& expected_proxy_mode) {
    const base::Value* value = nullptr;
    ASSERT_TRUE(store_->GetValue(proxy_config::prefs::kProxy, &value));
    ASSERT_TRUE(value->is_dict());
    ProxyConfigDictionary dict(value->Clone());
    std::string s;
    if (expected_proxy_server.empty()) {
      EXPECT_FALSE(dict.GetProxyServer(&s));
    } else {
      ASSERT_TRUE(dict.GetProxyServer(&s));
      EXPECT_EQ(expected_proxy_server, s);
    }
    if (expected_proxy_pac_url.empty()) {
      EXPECT_FALSE(dict.GetPacUrl(&s));
    } else {
      ASSERT_TRUE(dict.GetPacUrl(&s));
      EXPECT_EQ(expected_proxy_pac_url, s);
    }
    if (expected_proxy_bypass_list.empty()) {
      EXPECT_FALSE(dict.GetBypassList(&s));
    } else {
      ASSERT_TRUE(dict.GetBypassList(&s));
      EXPECT_EQ(expected_proxy_bypass_list, s);
    }
    ProxyPrefs::ProxyMode mode;
    ASSERT_TRUE(dict.GetMode(&mode));
    EXPECT_EQ(expected_proxy_mode, mode);
  }
};

TEST_F(ProxyPolicyHandlerTest, ManualOptions) {
  PolicyMap policy;
  policy.Set(key::kProxyBypassList, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>("http://chromium.org/override"),
             nullptr);
  policy.Set(key::kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("chromium.org"),
             nullptr);
  policy.Set(
      key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(
          ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE),
      nullptr);
  UpdateProviderPolicy(policy);

  VerifyProxyPrefs("chromium.org",
                   std::string(),
                   "http://chromium.org/override",
                   ProxyPrefs::MODE_FIXED_SERVERS);
}

TEST_F(ProxyPolicyHandlerTest, ManualOptionsReversedApplyOrder) {
  PolicyMap policy;
  policy.Set(
      key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(
          ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE),
      nullptr);
  policy.Set(key::kProxyBypassList, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>("http://chromium.org/override"),
             nullptr);
  policy.Set(key::kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("chromium.org"),
             nullptr);
  UpdateProviderPolicy(policy);

  VerifyProxyPrefs("chromium.org",
                   std::string(),
                   "http://chromium.org/override",
                   ProxyPrefs::MODE_FIXED_SERVERS);
}

TEST_F(ProxyPolicyHandlerTest, ManualOptionsInvalid) {
  PolicyMap policy;
  policy.Set(
      key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(
          ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE),
      nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(proxy_config::prefs::kProxy, &value));
}

TEST_F(ProxyPolicyHandlerTest, NoProxyServerMode) {
  PolicyMap policy;
  policy.Set(
      key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(ProxyPolicyHandler::PROXY_SERVER_MODE),
      nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(
      std::string(), std::string(), std::string(), ProxyPrefs::MODE_DIRECT);
}

TEST_F(ProxyPolicyHandlerTest, NoProxyModeName) {
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(ProxyPrefs::kDirectProxyModeName),
             nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(
      std::string(), std::string(), std::string(), ProxyPrefs::MODE_DIRECT);
}

TEST_F(ProxyPolicyHandlerTest, AutoDetectProxyServerMode) {
  PolicyMap policy;
  policy.Set(key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(
                 ProxyPolicyHandler::PROXY_AUTO_DETECT_PROXY_SERVER_MODE),
             nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(),
                   std::string(),
                   std::string(),
                   ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyHandlerTest, AutoDetectProxyModeName) {
  PolicyMap policy;
  policy.Set(
      key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(ProxyPrefs::kAutoDetectProxyModeName),
      nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(),
                   std::string(),
                   std::string(),
                   ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyHandlerTest, PacScriptProxyMode) {
  PolicyMap policy;
  policy.Set(key::kProxyPacUrl, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>("http://short.org/proxy.pac"),
             nullptr);
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(ProxyPrefs::kPacScriptProxyModeName),
             nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(),
                   "http://short.org/proxy.pac",
                   std::string(),
                   ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(ProxyPolicyHandlerTest, PacScriptProxyModeInvalid) {
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(ProxyPrefs::kPacScriptProxyModeName),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(proxy_config::prefs::kProxy, &value));
}

// Regression test for http://crbug.com/78016, CPanel returns empty strings
// for unset properties.
TEST_F(ProxyPolicyHandlerTest, PacScriptProxyModeBug78016) {
  PolicyMap policy;
  policy.Set(key::kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(std::string()),
             nullptr);
  policy.Set(key::kProxyPacUrl, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>("http://short.org/proxy.pac"),
             nullptr);
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(ProxyPrefs::kPacScriptProxyModeName),
             nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(),
                   "http://short.org/proxy.pac",
                   std::string(),
                   ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(ProxyPolicyHandlerTest, UseSystemProxyServerMode) {
  PolicyMap policy;
  policy.Set(key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(
                 ProxyPolicyHandler::PROXY_USE_SYSTEM_PROXY_SERVER_MODE),
             nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(
      std::string(), std::string(), std::string(), ProxyPrefs::MODE_SYSTEM);
}

TEST_F(ProxyPolicyHandlerTest, UseSystemProxyMode) {
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(ProxyPrefs::kSystemProxyModeName),
             nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(
      std::string(), std::string(), std::string(), ProxyPrefs::MODE_SYSTEM);
}

TEST_F(ProxyPolicyHandlerTest,
       ProxyModeOverridesProxyServerMode) {
  PolicyMap policy;
  policy.Set(
      key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(ProxyPolicyHandler::PROXY_SERVER_MODE),
      nullptr);
  policy.Set(
      key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(ProxyPrefs::kAutoDetectProxyModeName),
      nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(),
                   std::string(),
                   std::string(),
                   ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyHandlerTest, ProxyInvalid) {
  // No mode expects all three parameters being set.
  PolicyMap policy;
  policy.Set(key::kProxyPacUrl, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>("http://short.org/proxy.pac"),
             nullptr);
  policy.Set(key::kProxyBypassList, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>("http://chromium.org/override"),
             nullptr);
  policy.Set(key::kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("chromium.org"),
             nullptr);
  for (int i = 0; i < ProxyPolicyHandler::MODE_COUNT; ++i) {
    policy.Set(key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(i), nullptr);
    UpdateProviderPolicy(policy);
    const base::Value* value = nullptr;
    EXPECT_FALSE(store_->GetValue(proxy_config::prefs::kProxy, &value));
  }
}

}  // namespace policy
