// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/proxy_prefs_utils.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/net/core/features.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "net/base/proxy_chain.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace proxy_config {

class ProxyPrefsUtilsTest : public testing::Test {
 protected:
  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override_{
          policy::PlatformManagementService::GetInstance(),
          policy::EnterpriseManagementAuthority::NONE};
};

TEST_F(ProxyPrefsUtilsTest, ProxyOverrideRuleHostFromString) {
  url::SchemeHostPort scheme_host_port =
      ProxyOverrideRuleHostFromString("https://google.com:123");
  ASSERT_TRUE(scheme_host_port.IsValid());
  ASSERT_EQ(scheme_host_port.scheme(), url::kHttpsScheme);
  ASSERT_EQ(scheme_host_port.host(), "google.com");
  ASSERT_EQ(scheme_host_port.port(), 123);

  scheme_host_port = ProxyOverrideRuleHostFromString("google.com:123");
  ASSERT_TRUE(scheme_host_port.IsValid());
  ASSERT_EQ(scheme_host_port.scheme(), url::kHttpScheme);
  ASSERT_EQ(scheme_host_port.host(), "google.com");
  ASSERT_EQ(scheme_host_port.port(), 123);

  scheme_host_port = ProxyOverrideRuleHostFromString("https://google.com");
  ASSERT_TRUE(scheme_host_port.IsValid());
  ASSERT_EQ(scheme_host_port.scheme(), url::kHttpsScheme);
  ASSERT_EQ(scheme_host_port.host(), "google.com");
  ASSERT_EQ(scheme_host_port.port(), 443);

  scheme_host_port = ProxyOverrideRuleHostFromString("google.com");
  ASSERT_TRUE(scheme_host_port.IsValid());
  ASSERT_EQ(scheme_host_port.scheme(), url::kHttpScheme);
  ASSERT_EQ(scheme_host_port.host(), "google.com");
  ASSERT_EQ(scheme_host_port.port(), 80);

  scheme_host_port = ProxyOverrideRuleHostFromString("192.168.1.1");
  ASSERT_TRUE(scheme_host_port.IsValid());
  ASSERT_EQ(scheme_host_port.scheme(), url::kHttpScheme);
  ASSERT_EQ(scheme_host_port.host(), "192.168.1.1");
  ASSERT_EQ(scheme_host_port.port(), 80);

  scheme_host_port =
      ProxyOverrideRuleHostFromString("https://[3ffe:2a00:100:7031:0:0::1]");
  ASSERT_TRUE(scheme_host_port.IsValid());
  ASSERT_EQ(scheme_host_port.scheme(), url::kHttpsScheme);
  ASSERT_EQ(scheme_host_port.host(), "[3ffe:2a00:100:7031::1]");
  ASSERT_EQ(scheme_host_port.port(), 443);

  ASSERT_FALSE(ProxyOverrideRuleHostFromString("https://").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleHostFromString("http://").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleHostFromString("://").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleHostFromString("123456789").IsValid());
}

TEST_F(ProxyPrefsUtilsTest, ProxyOverrideRuleProxyFromString) {
  net::ProxyChain proxy_chain =
      ProxyOverrideRuleProxyFromString("https://google.com:123");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_HTTPS);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 123);

  proxy_chain = ProxyOverrideRuleProxyFromString("https://google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_HTTPS);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 443);

  proxy_chain = ProxyOverrideRuleProxyFromString("HTTPS google.com:123");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_HTTPS);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 123);

  proxy_chain = ProxyOverrideRuleProxyFromString("HTTPS google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_HTTPS);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 443);

  proxy_chain = ProxyOverrideRuleProxyFromString("socks4://google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_SOCKS4);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 1080);

  proxy_chain = ProxyOverrideRuleProxyFromString("SOCKS4 google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_SOCKS4);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 1080);

  proxy_chain = ProxyOverrideRuleProxyFromString("socks5://google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_SOCKS5);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 1080);

  proxy_chain = ProxyOverrideRuleProxyFromString("SOCKS5 google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_SOCKS5);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 1080);

  proxy_chain = ProxyOverrideRuleProxyFromString("PROXY google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_HTTP);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 80);

  proxy_chain = ProxyOverrideRuleProxyFromString("DIRECT");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_TRUE(proxy_chain.is_direct());

  ASSERT_FALSE(ProxyOverrideRuleProxyFromString("google").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleProxyFromString("google.com").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleProxyFromString("https://").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleProxyFromString("http://").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleProxyFromString("://").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleProxyFromString("123456789").IsValid());
}

namespace {

void SetAffiliation(TestingPrefServiceSimple& prefs,
                    policy::MockPolicyService& policy_service,
                    policy::PolicyMap& policy_map,
                    bool affiliated,
                    int enabled_for_all_users) {
  policy_map.Clear();
  policy_map.SetDeviceAffiliationIds({"device_id"});
  if (affiliated) {
    policy_map.SetUserAffiliationIds({"device_id"});
  } else {
    policy_map.SetUserAffiliationIds({"other_id"});
  }
  EXPECT_CALL(policy_service, GetPolicies(testing::_))
      .WillRepeatedly(testing::ReturnRef(policy_map));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  prefs.SetInteger(prefs::kEnableProxyOverrideRulesForAllUsers,
                   enabled_for_all_users);
#endif
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
void VerifyUnaffiliatedRestriction(TestingPrefServiceSimple& prefs,
                                   policy::MockPolicyService& policy_service,
                                   policy::PolicyMap& policy_map,
                                   bool expected_restricted) {
  // Unaffiliated: restricted if `expected_restricted` is true.
  SetAffiliation(prefs, policy_service, policy_map, /*affiliated=*/false,
                 /*enabled_for_all_users=*/0);
  EXPECT_EQ(ProxyOverrideRulesAllowed(&prefs, &policy_service),
            !expected_restricted);

  // Unaffiliated but enabled by policy: always allowed.
  SetAffiliation(prefs, policy_service, policy_map, /*affiliated=*/false,
                 /*enabled_for_all_users=*/1);
  EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs, &policy_service));

  // Affiliated: always allowed.
  SetAffiliation(prefs, policy_service, policy_map, /*affiliated=*/true,
                 /*enabled_for_all_users=*/0);
  EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs, &policy_service));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

}  // namespace

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
TEST_F(ProxyPrefsUtilsTest, ProxyOverrideRulesAllowed) {
  TestingPrefServiceSimple prefs;
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(prefs.registry());
  testing::NiceMock<policy::MockPolicyService> policy_service;
  policy::PolicyMap policy_map;

  // If `kProxyOverrideRules` is not managed by policy or controlled by an
  // extension, it is always allowed.
  SetAffiliation(prefs, policy_service, policy_map, /*affiliated=*/false,
                 /*enabled_for_all_users=*/0);
  EXPECT_FALSE(IsUserOrExtensionProxyOverrideRules(&prefs));
  EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs, &policy_service));

  // Scenario 1: `kProxyOverrideRules` is controlled by an extension.
  // Affiliation is checked, but scope is ignored.
  prefs.SetExtensionPref(prefs::kProxyOverrideRules,
                         base::Value(base::Value::Type::LIST));

  prefs.SetInteger(prefs::kProxyOverrideRulesScope,
                   policy::POLICY_SCOPE_MACHINE);
  EXPECT_TRUE(IsUserOrExtensionProxyOverrideRules(&prefs));
  VerifyUnaffiliatedRestriction(prefs, policy_service, policy_map,
                                /*expected_restricted=*/true);

  prefs.SetInteger(prefs::kProxyOverrideRulesScope, policy::POLICY_SCOPE_USER);
  EXPECT_TRUE(IsUserOrExtensionProxyOverrideRules(&prefs));
  VerifyUnaffiliatedRestriction(prefs, policy_service, policy_map,
                                /*expected_restricted=*/true);

  // Scenario 2: `kProxyOverrideRules` is managed by policy.
  prefs.SetManagedPref(prefs::kProxyOverrideRules,
                       base::Value(base::Value::Type::LIST));

  // If rules are set at the machine scope, they are always allowed.
  prefs.SetInteger(prefs::kProxyOverrideRulesScope,
                   policy::POLICY_SCOPE_MACHINE);
  EXPECT_FALSE(IsUserOrExtensionProxyOverrideRules(&prefs));
  VerifyUnaffiliatedRestriction(prefs, policy_service, policy_map,
                                /*expected_restricted=*/false);

  // If rules are set at the user scope, they are restricted for unaffiliated
  // users.
  prefs.SetInteger(prefs::kProxyOverrideRulesScope, policy::POLICY_SCOPE_USER);
  EXPECT_TRUE(IsUserOrExtensionProxyOverrideRules(&prefs));
  VerifyUnaffiliatedRestriction(prefs, policy_service, policy_map,
                                /*expected_restricted=*/true);
}

TEST_F(ProxyPrefsUtilsTest,
       ProxyOverrideRulesAllowed_PlatformManagedWithoutCbcm) {
  base::test::ScopedFeatureList feature_list(
      enterprise_net::kEnforceProxyAffiliationOnPlatformManagedDevices);
  policy::ScopedManagementServiceOverrideForTesting platform_managed(
      policy::PlatformManagementService::GetInstance(),
      policy::EnterpriseManagementAuthority::CLOUD);

  TestingPrefServiceSimple prefs;
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(prefs.registry());
  testing::NiceMock<policy::MockPolicyService> policy_service;
  policy::PolicyMap policy_map;
  // No device affiliation IDs (no CBCM), only user affiliation IDs.
  policy_map.SetUserAffiliationIds({"user_id"});
  EXPECT_CALL(policy_service, GetPolicies(testing::_))
      .WillRepeatedly(testing::ReturnRef(policy_map));

  prefs.SetManagedPref(prefs::kProxyOverrideRules,
                       base::Value(base::Value::Type::LIST));
  prefs.SetInteger(prefs::kProxyOverrideRulesScope, policy::POLICY_SCOPE_USER);
  prefs.SetInteger(prefs::kEnableProxyOverrideRulesForAllUsers, 0);

  // Blocked for user-scoped policy on a platform/EMM-managed device without
  // CBCM.
  EXPECT_FALSE(ProxyOverrideRulesAllowed(&prefs, &policy_service));
  EXPECT_FALSE(AreUserProxyOverrideRulesAllowed(&prefs, &policy_service));

  // Allowed when EnableProxyOverrideRulesForAllUsers is set to 1.
  prefs.SetInteger(prefs::kEnableProxyOverrideRulesForAllUsers, 1);
  EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs, &policy_service));
  EXPECT_TRUE(AreUserProxyOverrideRulesAllowed(&prefs, &policy_service));

  // Allowed when ProxyOverrideRules is set at machine scope even if
  // EnableProxyOverrideRulesForAllUsers is 0.
  prefs.SetInteger(prefs::kEnableProxyOverrideRulesForAllUsers, 0);
  prefs.SetInteger(prefs::kProxyOverrideRulesScope,
                   policy::POLICY_SCOPE_MACHINE);
  EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs, &policy_service));
  EXPECT_FALSE(AreUserProxyOverrideRulesAllowed(&prefs, &policy_service));

  // Blocked for extension-controlled rules when
  // EnableProxyOverrideRulesForAllUsers is 0, and allowed when it is 1.
  prefs.RemoveManagedPref(prefs::kProxyOverrideRules);
  prefs.SetExtensionPref(prefs::kProxyOverrideRules,
                         base::Value(base::Value::Type::LIST));
  EXPECT_FALSE(ProxyOverrideRulesAllowed(&prefs, &policy_service));
  prefs.SetInteger(prefs::kEnableProxyOverrideRulesForAllUsers, 1);
  EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs, &policy_service));
}

TEST_F(ProxyPrefsUtilsTest,
       ProxyOverrideRulesAllowed_PlatformManagedWithoutCbcmFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      enterprise_net::kEnforceProxyAffiliationOnPlatformManagedDevices);
  policy::ScopedManagementServiceOverrideForTesting platform_managed(
      policy::PlatformManagementService::GetInstance(),
      policy::EnterpriseManagementAuthority::CLOUD);

  TestingPrefServiceSimple prefs;
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(prefs.registry());
  testing::NiceMock<policy::MockPolicyService> policy_service;
  policy::PolicyMap policy_map;
  // No device affiliation IDs (no CBCM), only user affiliation IDs.
  policy_map.SetUserAffiliationIds({"user_id"});
  EXPECT_CALL(policy_service, GetPolicies(testing::_))
      .WillRepeatedly(testing::ReturnRef(policy_map));

  prefs.SetManagedPref(prefs::kProxyOverrideRules,
                       base::Value(base::Value::Type::LIST));
  prefs.SetInteger(prefs::kProxyOverrideRulesScope, policy::POLICY_SCOPE_USER);
  prefs.SetInteger(prefs::kEnableProxyOverrideRulesForAllUsers, 0);

  // Allowed because platform management alone does not make the device managed
  // without the feature.
  EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs, &policy_service));
  EXPECT_TRUE(AreUserProxyOverrideRulesAllowed(&prefs, &policy_service));

  // Extension-controlled rules are also allowed without the feature.
  prefs.RemoveManagedPref(prefs::kProxyOverrideRules);
  prefs.SetExtensionPref(prefs::kProxyOverrideRules,
                         base::Value(base::Value::Type::LIST));
  EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs, &policy_service));
  EXPECT_TRUE(AreUserProxyOverrideRulesAllowed(&prefs, &policy_service));

  // CBCM device affiliation IDs still require an affiliated user, matching the
  // behavior prior to https://crrev.com/c/8618781.
  policy_map.SetDeviceAffiliationIds({"device_id"});
  EXPECT_FALSE(ProxyOverrideRulesAllowed(&prefs, &policy_service));
  EXPECT_FALSE(AreUserProxyOverrideRulesAllowed(&prefs, &policy_service));

  policy_map.SetDeviceAffiliationIds({"user_id"});
  EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs, &policy_service));
  EXPECT_TRUE(AreUserProxyOverrideRulesAllowed(&prefs, &policy_service));
}

TEST_F(ProxyPrefsUtilsTest,
       ProxyOverrideRulesAllowed_LowTrustPlatformManagementWithoutCbcm) {
  base::test::ScopedFeatureList feature_list(
      enterprise_net::kEnforceProxyAffiliationOnPlatformManagedDevices);
  policy::ScopedManagementServiceOverrideForTesting computer_local(
      policy::PlatformManagementService::GetInstance(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);

  TestingPrefServiceSimple prefs;
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(prefs.registry());
  testing::NiceMock<policy::MockPolicyService> policy_service;
  policy::PolicyMap policy_map;
  policy_map.SetUserAffiliationIds({"user_id"});
  EXPECT_CALL(policy_service, GetPolicies(testing::_))
      .WillRepeatedly(testing::ReturnRef(policy_map));

  prefs.SetManagedPref(prefs::kProxyOverrideRules,
                       base::Value(base::Value::Type::LIST));
  prefs.SetInteger(prefs::kProxyOverrideRulesScope, policy::POLICY_SCOPE_USER);
  prefs.SetInteger(prefs::kEnableProxyOverrideRulesForAllUsers, 0);

  // Allowed when the platform only has `COMPUTER_LOCAL` (`LOW` trustworthiness)
  // management and no CBCM affiliation IDs.
  EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs, &policy_service));
  EXPECT_TRUE(AreUserProxyOverrideRulesAllowed(&prefs, &policy_service));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

TEST_F(ProxyPrefsUtilsTest, AreUserProxyOverrideRulesAllowed) {
  TestingPrefServiceSimple prefs;
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(prefs.registry());
  testing::NiceMock<policy::MockPolicyService> policy_service;
  policy::PolicyMap policy_map;

  // Unlike `ProxyOverrideRulesAllowed`, `AreUserProxyOverrideRulesAllowed`
  // evaluates user/extension eligibility even when `kProxyOverrideRules` has no
  // value set yet. On ChromeOS, user rules are never blocked by unaffiliation
  // and always return true.
  SetAffiliation(prefs, policy_service, policy_map, /*affiliated=*/false,
                 /*enabled_for_all_users=*/0);
  EXPECT_EQ(AreUserProxyOverrideRulesAllowed(&prefs, &policy_service),
            !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
              BUILDFLAG(IS_ANDROID)));

  // Unaffiliated with policy enabled for all users: allowed.
  SetAffiliation(prefs, policy_service, policy_map, /*affiliated=*/false,
                 /*enabled_for_all_users=*/1);
  EXPECT_TRUE(AreUserProxyOverrideRulesAllowed(&prefs, &policy_service));

  // Affiliated: allowed even when `kEnableProxyOverrideRulesForAllUsers` is 0.
  SetAffiliation(prefs, policy_service, policy_map, /*affiliated=*/true,
                 /*enabled_for_all_users=*/0);
  EXPECT_TRUE(AreUserProxyOverrideRulesAllowed(&prefs, &policy_service));

  // Unmanaged device (empty device affiliation IDs): allowed.
  policy_map.Clear();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  prefs.SetInteger(prefs::kEnableProxyOverrideRulesForAllUsers, 0);
#endif
  EXPECT_TRUE(AreUserProxyOverrideRulesAllowed(&prefs, &policy_service));
}

TEST_F(ProxyPrefsUtilsTest, ParseProxyOverrideRules) {
  std::optional<base::Value> parsed = base::JSONReader::Read(
      R"([
        {
          "DestinationMatchers": ["https://*.example.com"],
          "ExcludeDestinationMatchers": ["https://bypass.example.com"],
          "ProxyList": ["HTTPS proxy.example.com:443", "DIRECT"],
          "Conditions": [
            {
              "DnsProbe": {
                "Host": "corp.internal",
                "Result": "resolved"
              }
            }
          ]
        },
        {
          "DestinationMatchers": ["https://invalid-no-proxy-list.example.com"]
        }
      ])",
      base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed && parsed->is_list());

  std::vector<net::ProxyConfig::ProxyOverrideRule> rules =
      ParseProxyOverrideRules(parsed->GetList());
  ASSERT_EQ(rules.size(), 1u);
  EXPECT_TRUE(rules[0].MatchesDestination(GURL("https://app.example.com")));
  EXPECT_FALSE(rules[0].MatchesDestination(GURL("https://bypass.example.com")));
  EXPECT_FALSE(rules[0].MatchesDestination(GURL("http://app.example.com")));
  EXPECT_FALSE(rules[0].MatchesDestination(GURL("https://localhost")));
  EXPECT_EQ(rules[0].proxy_list.ToPacString(),
            "HTTPS proxy.example.com:443;DIRECT");
  ASSERT_EQ(rules[0].dns_conditions.size(), 1u);
  EXPECT_EQ(rules[0].dns_conditions[0].host,
            url::SchemeHostPort(url::kHttpScheme, "corp.internal", 80));
  EXPECT_EQ(rules[0].dns_conditions[0].result,
            net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kResolved);
}

TEST_F(ProxyPrefsUtilsTest, PrefConfigToNetConfig) {
  net::ProxyConfigWithAnnotation config;

  // MODE_SYSTEM returns false so callers fall back to OS proxy settings.
  ProxyConfigDictionary system_dict(ProxyConfigDictionary::CreateSystem());
  EXPECT_FALSE(PrefConfigToNetConfig(system_dict, TRAFFIC_ANNOTATION_FOR_TESTS,
                                     &config));

  // MODE_DIRECT returns true with empty proxy rules.
  ProxyConfigDictionary direct_dict(ProxyConfigDictionary::CreateDirect());
  config = net::ProxyConfigWithAnnotation::CreateDirect();
  EXPECT_TRUE(PrefConfigToNetConfig(direct_dict, TRAFFIC_ANNOTATION_FOR_TESTS,
                                    &config));
  EXPECT_TRUE(config.value().proxy_rules().empty());
  EXPECT_FALSE(config.value().auto_detect());
  EXPECT_FALSE(config.value().has_pac_url());

  // MODE_AUTO_DETECT enables WPAD auto-detection and preserves the annotation.
  ProxyConfigDictionary auto_detect_dict(
      ProxyConfigDictionary::CreateAutoDetect());
  config = net::ProxyConfigWithAnnotation::CreateDirect();
  EXPECT_TRUE(PrefConfigToNetConfig(auto_detect_dict,
                                    TRAFFIC_ANNOTATION_FOR_TESTS, &config));
  EXPECT_TRUE(config.value().auto_detect());
  EXPECT_FALSE(config.value().has_pac_url());
  EXPECT_TRUE(config.value().proxy_rules().empty());
  EXPECT_EQ(config.traffic_annotation(), TRAFFIC_ANNOTATION_FOR_TESTS);

  // MODE_PAC_SCRIPT populates PAC URL, mandatory bit, and traffic annotation.
  ProxyConfigDictionary pac_dict(ProxyConfigDictionary::CreatePacScript(
      "https://pac.example.com/wpad.dat", /*pac_mandatory=*/true));
  config = net::ProxyConfigWithAnnotation::CreateDirect();
  EXPECT_TRUE(
      PrefConfigToNetConfig(pac_dict, TRAFFIC_ANNOTATION_FOR_TESTS, &config));
  EXPECT_EQ(config.value().pac_url(), GURL("https://pac.example.com/wpad.dat"));
  EXPECT_TRUE(config.value().pac_mandatory());
  EXPECT_EQ(config.traffic_annotation(), TRAFFIC_ANNOTATION_FOR_TESTS);

  // MODE_FIXED_SERVERS populates proxy rules, bypass list, and annotation.
  ProxyConfigDictionary fixed_dict(ProxyConfigDictionary::CreateFixedServers(
      "https://proxy.example.com:8443", "*.internal.example.com"));
  config = net::ProxyConfigWithAnnotation::CreateDirect();
  EXPECT_TRUE(
      PrefConfigToNetConfig(fixed_dict, TRAFFIC_ANNOTATION_FOR_TESTS, &config));
  EXPECT_EQ(config.value().proxy_rules().type,
            net::ProxyConfig::ProxyRules::Type::PROXY_LIST);
  EXPECT_EQ(config.value().proxy_rules().single_proxies.ToPacString(),
            "HTTPS proxy.example.com:8443");
  EXPECT_TRUE(config.value().proxy_rules().bypass_rules.Matches(
      GURL("https://foo.internal.example.com")));
  EXPECT_FALSE(config.value().proxy_rules().bypass_rules.Matches(
      GURL("https://external.example.org")));
  EXPECT_EQ(config.traffic_annotation(), TRAFFIC_ANNOTATION_FOR_TESTS);
}

}  // namespace proxy_config
