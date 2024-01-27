// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/proxy_policy_handler.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/proxy_settings_constants.h"
#include "components/policy/policy_constants.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

using policy::ConfigurationPolicyHandler;
using policy::ConfigurationPolicyPrefStore;
using policy::ConfigurationPolicyPrefStoreTest;
using policy::kProxyPacMandatory;
using policy::POLICY_LEVEL_MANDATORY;
using policy::POLICY_LEVEL_RECOMMENDED;
using policy::POLICY_SCOPE_USER;
using policy::POLICY_SOURCE_CLOUD;
using policy::PolicyMap;
using policy::PolicyServiceImpl;
using policy::key::kProxyBypassList;
using policy::key::kProxyMode;
using policy::key::kProxyPacUrl;
using policy::key::kProxyServer;
using policy::key::kProxyServerMode;
using policy::key::kProxySettings;

namespace proxy_config {

// Test cases for the proxy policy settings.
class ProxyPolicyHandlerTest : public ConfigurationPolicyPrefStoreTest {
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
  void VerifyProxyPrefs(const std::string& expected_proxy_server,
                        const std::string& expected_proxy_pac_url,
                        std::optional<bool> expected_proxy_pac_mandatory,
                        const std::string& expected_proxy_bypass_list,
                        const ProxyPrefs::ProxyMode& expected_proxy_mode) {
    const base::Value* value = nullptr;
    ASSERT_TRUE(store_->GetValue(proxy_config::prefs::kProxy, &value));
    ASSERT_TRUE(value->is_dict());
    ProxyConfigDictionary dict(value->GetDict().Clone());
    std::string s;
    bool b;
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
    if (!expected_proxy_pac_mandatory) {
      EXPECT_FALSE(dict.GetPacMandatory(&b));
    } else {
      ASSERT_TRUE(dict.GetPacMandatory(&b));
      EXPECT_EQ(*expected_proxy_pac_mandatory, b);
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
  policy.Set(kProxyBypassList, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("http://chromium.org/override"),
             nullptr);
  policy.Set(kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("chromium.org"), nullptr);
  policy.Set(
      kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      base::Value(
          ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE),
      nullptr);
  UpdateProviderPolicy(policy);

  VerifyProxyPrefs("chromium.org", std::string(), std::nullopt,
                   "http://chromium.org/override",
                   ProxyPrefs::MODE_FIXED_SERVERS);
}

TEST_F(ProxyPolicyHandlerTest, ManualOptionsReversedApplyOrder) {
  PolicyMap policy;
  policy.Set(
      kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      base::Value(
          ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE),
      nullptr);
  policy.Set(kProxyBypassList, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("http://chromium.org/override"),
             nullptr);
  policy.Set(kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("chromium.org"), nullptr);
  UpdateProviderPolicy(policy);

  VerifyProxyPrefs("chromium.org", std::string(), std::nullopt,
                   "http://chromium.org/override",
                   ProxyPrefs::MODE_FIXED_SERVERS);
}

TEST_F(ProxyPolicyHandlerTest, ManualOptionsInvalid) {
  PolicyMap policy;
  policy.Set(
      kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      base::Value(
          ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE),
      nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(proxy_config::prefs::kProxy, &value));
}

TEST_F(ProxyPolicyHandlerTest, NoProxyServerMode) {
  PolicyMap policy;
  policy.Set(kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             base::Value(ProxyPolicyHandler::PROXY_SERVER_MODE), nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(), std::string(), std::nullopt, std::string(),
                   ProxyPrefs::MODE_DIRECT);
}

TEST_F(ProxyPolicyHandlerTest, NoProxyModeName) {
  PolicyMap policy;
  policy.Set(kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value(ProxyPrefs::kDirectProxyModeName),
             nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(), std::string(), std::nullopt, std::string(),
                   ProxyPrefs::MODE_DIRECT);
}

TEST_F(ProxyPolicyHandlerTest, AutoDetectProxyServerMode) {
  PolicyMap policy;
  policy.Set(
      kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      base::Value(ProxyPolicyHandler::PROXY_AUTO_DETECT_PROXY_SERVER_MODE),
      nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(), std::string(), std::nullopt, std::string(),
                   ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyHandlerTest, AutoDetectProxyModeName) {
  PolicyMap policy;
  policy.Set(kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             base::Value(ProxyPrefs::kAutoDetectProxyModeName), nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(), std::string(), std::nullopt, std::string(),
                   ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyHandlerTest, PacScriptProxyMode) {
  PolicyMap policy;
  policy.Set(kProxyPacUrl, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("http://short.org/proxy.pac"),
             nullptr);
  policy.Set(kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             base::Value(ProxyPrefs::kPacScriptProxyModeName), nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(), "http://short.org/proxy.pac",
                   /* expected_proxy_pac_mandatory */ false, std::string(),
                   ProxyPrefs::MODE_PAC_SCRIPT);
}

// ProxyPacMandatory can be set only via ProxySettings.
TEST_F(ProxyPolicyHandlerTest, PacScriptProxyModeWithPacMandatory) {
  base::Value proxy_settings(base::Value::Type::DICT);
  proxy_settings.GetDict().Set(kProxyPacUrl, "http://short.org/proxy.pac");
  proxy_settings.GetDict().Set(kProxyMode, ProxyPrefs::kPacScriptProxyModeName);
  proxy_settings.GetDict().Set(kProxyPacMandatory, true);

  PolicyMap policy;
  policy.Set(kProxySettings, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::move(proxy_settings), nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(), "http://short.org/proxy.pac",
                   /* expected_proxy_pac_mandatory */ true, std::string(),
                   ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(ProxyPolicyHandlerTest, PacScriptProxyModeInvalid) {
  PolicyMap policy;
  policy.Set(kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             base::Value(ProxyPrefs::kPacScriptProxyModeName), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(proxy_config::prefs::kProxy, &value));
}

// Regression test for http://crbug.com/78016, device management server returns
// empty strings for unset properties.
TEST_F(ProxyPolicyHandlerTest, PacScriptProxyModeBug78016) {
  PolicyMap policy;
  policy.Set(kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value(std::string()), nullptr);
  policy.Set(kProxyPacUrl, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("http://short.org/proxy.pac"),
             nullptr);
  policy.Set(kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             base::Value(ProxyPrefs::kPacScriptProxyModeName), nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(), "http://short.org/proxy.pac", false,
                   std::string(), ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(ProxyPolicyHandlerTest, UseSystemProxyServerMode) {
  PolicyMap policy;
  policy.Set(
      kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      base::Value(ProxyPolicyHandler::PROXY_USE_SYSTEM_PROXY_SERVER_MODE),
      nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(), std::string(), std::nullopt, std::string(),
                   ProxyPrefs::MODE_SYSTEM);
}

TEST_F(ProxyPolicyHandlerTest, UseSystemProxyMode) {
  PolicyMap policy;
  policy.Set(kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value(ProxyPrefs::kSystemProxyModeName),
             nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(), std::string(), std::nullopt, std::string(),
                   ProxyPrefs::MODE_SYSTEM);
}

TEST_F(ProxyPolicyHandlerTest, ProxyModeOverridesProxyServerMode) {
  PolicyMap policy;
  policy.Set(kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             base::Value(ProxyPolicyHandler::PROXY_SERVER_MODE), nullptr);
  policy.Set(kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             base::Value(ProxyPrefs::kAutoDetectProxyModeName), nullptr);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(), std::string(), std::nullopt, std::string(),
                   ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyHandlerTest, ProxyInvalid) {
  // No mode expects all three parameters being set.
  PolicyMap policy;
  policy.Set(kProxyPacUrl, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("http://short.org/proxy.pac"),
             nullptr);
  policy.Set(kProxyBypassList, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("http://chromium.org/override"),
             nullptr);
  policy.Set(kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("chromium.org"), nullptr);
  for (int i = 0; i < ProxyPolicyHandler::MODE_COUNT; ++i) {
    policy.Set(kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(i), nullptr);
    UpdateProviderPolicy(policy);
    const base::Value* value = nullptr;
    EXPECT_FALSE(store_->GetValue(proxy_config::prefs::kProxy, &value));
  }
}

TEST_F(ProxyPolicyHandlerTest, SeparateProxyPoliciesMerging) {
  PolicyMap policy;
  // Individual proxy policy values should be collected into a dictionary.
  policy.Set(
      kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      base::Value(ProxyPolicyHandler::PROXY_USE_SYSTEM_PROXY_SERVER_MODE),
      nullptr);
  // Both these policies should be ignored, since there's a higher priority
  // policy available.
  policy.Set(kProxyMode, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("pac_script"), nullptr);
  policy.Set(kProxyPacUrl, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("http://example.com/wpad.dat"),
             nullptr);

  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(), std::string(), std::nullopt, std::string(),
                   ProxyPrefs::MODE_SYSTEM);
}

}  // namespace proxy_config
