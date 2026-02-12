// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/proxy_override_rules_policy_handler.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/types/expected_macros.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/prefs/pref_value_map.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proxy_config {
namespace {

constexpr char kPolicyName[] = "ProxyOverrideRules";

// Schema copied from out/Default/gen/chrome/app/policy/policy_templates.json
constexpr char kSchema[] = R"(
{
  "type": "object",
  "properties": {
    "ProxyOverrideRules": {
      "items": {
        "properties": {
          "Conditions": {
            "items": {
              "properties": {
                "DnsProbe": {
                  "properties": {
                    "Host": {
                      "type": "string"
                    },
                    "Result": {
                      "enum": [
                        "resolved",
                        "not_found"
                      ],
                      "type": "string"
                    }
                  },
                  "type": "object"
                }
              },
              "type": "object"
            },
            "type": "array"
          },
          "DestinationMatchers": {
            "items": {
              "type": "string"
            },
            "type": "array"
          },
          "ExcludeDestinationMatchers": {
            "items": {
              "type": "string"
            },
            "type": "array"
          },
          "ProxyList": {
            "items": {
              "type": "string"
            },
            "type": "array"
          }
        },
        "type": "object"
      },
      "type": "array"
    },
    "EnableProxyOverrideRulesForAllUsers": {
      "type": "integer"
    }
  }
})";

struct TestCase {
  const char* policy_value;
  const char16_t* expected_messages;
  bool affiliated = true;
};

constexpr TestCase kInvalidTestCases[] = {
    {
        R"([
             {
               "ProxyList": ["DIRECT"]
             }
           ])",
        u"Error at ProxyOverrideRules[0].DestinationMatchers: Must be "
        u"specified.",
    },
    {
        R"([
             {
               "DestinationMatchers": ["://"],
               "ProxyList": ["DIRECT"]
             }
           ])",
        u"Error at ProxyOverrideRules[0].DestinationMatchers[0]: \"://\" is not"
        u" a valid destination pattern.",
    },
    {
        R"([
             {
               "DestinationMatchers": ["https://*"],
               "ExcludeDestinationMatchers": ["://"],
               "ProxyList": ["DIRECT"]
             }
           ])",
        u"Error at ProxyOverrideRules[0].ExcludeDestinationMatchers[0]: "
        u"\"://\" is not a valid destination pattern.",
    },
    {
        R"([
             {
               "DestinationMatchers": ["https://*"]
             }
           ])",
        u"Error at ProxyOverrideRules[0].ProxyList: Must be specified.",
    },
    {
        R"([
             {
               "DestinationMatchers": ["https://*"],
               "ProxyList": ["foo"]
             }
           ])",
        u"Error at ProxyOverrideRules[0].ProxyList[0]: \"foo\" is not a valid "
        u"proxy.",
    },
    {
        R"([
             {
               "DestinationMatchers": ["https://*"],
               "ProxyList": ["DIRECT"],
               "Conditions": [
                 {
                   "FakeCondition": {}
                 }
               ]
             }
           ])",
        u"Error at ProxyOverrideRules[0].Conditions[0]: \"FakeCondition\" is "
        u"not a known condition type.",
    },
    {
        R"([
             {
               "DestinationMatchers": ["https://*"],
               "ProxyList": ["DIRECT"],
               "Conditions": [
                 {
                   "DnsProbe": {
                   }
                 }
               ]
             }
           ])",
        u"Error at ProxyOverrideRules[0].Conditions[0].Host: Must be "
        u"specified.\nError at ProxyOverrideRules[0].Conditions[0].Result: Must"
        u" be specified.",
    },
    {
        R"([
             {
               "DestinationMatchers": ["https://*"],
               "ProxyList": ["DIRECT"],
               "Conditions": [
                 {
                   "DnsProbe": {
                     "Host": "foo.com"
                   }
                 }
               ]
             }
           ])",
        u"Error at ProxyOverrideRules[0].Conditions[0].Result: Must be "
        u"specified.",
    },
    {
        R"([
             {
               "DestinationMatchers": ["https://*"],
               "ProxyList": ["DIRECT"],
               "Conditions": [
                 {
                   "DnsProbe": {
                     "Result": "resolved"
                   }
                 }
               ]
             }
           ])",
        u"Error at ProxyOverrideRules[0].Conditions[0].Host: Must be "
        u"specified.",
    },
    {
        R"([
             {
               "DestinationMatchers": ["https://*"],
               "ProxyList": ["DIRECT"],
               "Conditions": [
                 {
                   "DnsProbe": {
                     "Host": "://",
                     "Result": "not_found"
                   }
                 }
               ]
             }
           ])",
        u"Error at ProxyOverrideRules[0].Conditions[0].Host: \"://\" is not a "
        u"valid value.",
    },
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    {
        R"([
             {
               "DestinationMatchers": ["https://*"],
               "ProxyList": ["DIRECT"],
               "Conditions": [
                 {
                   "DnsProbe": {
                     "Host": "foo.com",
                     "Result": "not_found"
                   }
                 }
               ]
             }
           ])",
        u"This policy value is ignored since the user is not affiliated.",
        /*affiliated=*/false,
    },
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
};

class ProxyOverrideRulesPolicyHandlerTest
    : public testing::TestWithParam<TestCase> {
 public:
  policy::Schema schema() {
    ASSIGN_OR_RETURN(const auto validation_schema,
                     policy::Schema::Parse(kSchema), [](const auto& e) {
                       ADD_FAILURE() << e;
                       return policy::Schema();
                     });
    return validation_schema;
  }

  policy::PolicyMap CreatePolicyMap(const std::string& policy,
                                    policy::PolicySource policy_source) {
    policy::PolicyMap policy_map;
    policy_map.SetDeviceAffiliationIds({"same_id"});
    if (GetParam().affiliated) {
      policy_map.SetUserAffiliationIds({"same_id"});
    } else {
      policy_map.SetUserAffiliationIds({"different_id"});
    }
    policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                   policy::PolicyScope::POLICY_SCOPE_USER, policy_source,
                   base::JSONReader::Read(GetParam().policy_value,
                                          base::JSON_ALLOW_TRAILING_COMMAS),
                   nullptr);

    return policy_map;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ProxyOverrideRulesPolicyHandlerTest,
                         testing::ValuesIn(kInvalidTestCases));

TEST_P(ProxyOverrideRulesPolicyHandlerTest, Test) {
  policy::PolicyMap map = CreatePolicyMap(
      GetParam().policy_value, policy::PolicySource::POLICY_SOURCE_CLOUD);
  auto handler = std::make_unique<ProxyOverrideRulesPolicyHandler>(schema());

  policy::PolicyErrorMap errors;

  // Invalid list entries are tolerated, so in that case `CheckPolicySettings`
  // will still return true.
  ASSERT_TRUE(handler->CheckPolicySettings(map, &errors));

  ASSERT_FALSE(errors.empty());
  ASSERT_TRUE(errors.HasError(kPolicyName));

  std::u16string messages = errors.GetErrorMessages(kPolicyName);
  ASSERT_EQ(messages, GetParam().expected_messages);
}

TEST_F(ProxyOverrideRulesPolicyHandlerTest, AffiliationUpdatedWhenPolicyUnset) {
  auto handler = std::make_unique<ProxyOverrideRulesPolicyHandler>(schema());

  policy::PolicyMap policy_map;
  // Scenario 1: Affiliated (empty device IDs)
  policy_map.SetDeviceAffiliationIds({});
  policy_map.SetUserAffiliationIds({"user_id"});

  PrefValueMap prefs;
  policy::PolicyErrorMap errors;

  ASSERT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  handler->ApplyPolicySettings(policy_map, &prefs);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  bool affiliated = false;
  EXPECT_TRUE(
      prefs.GetBoolean(prefs::kProxyOverrideRulesAffiliation, &affiliated));
  EXPECT_TRUE(affiliated);
#endif

  // Scenario 2: Unaffiliated
  policy_map.SetDeviceAffiliationIds({"device_id"});
  policy_map.SetUserAffiliationIds({"user_id"});

  prefs.Clear();
  ASSERT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  handler->ApplyPolicySettings(policy_map, &prefs);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  EXPECT_TRUE(
      prefs.GetBoolean(prefs::kProxyOverrideRulesAffiliation, &affiliated));
  EXPECT_FALSE(affiliated);
#endif
}

TEST_F(ProxyOverrideRulesPolicyHandlerTest,
       AffiliationUpdatedWhenPolicyValueUnchanged) {
  auto handler = std::make_unique<ProxyOverrideRulesPolicyHandler>(schema());

  const char kPolicyValue[] = R"([
    {
      "DestinationMatchers": ["https://*"],
      "ProxyList": ["DIRECT"]
    }
  ])";

  policy::PolicyMap policy_map;
  policy_map.Set(
      kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
      policy::PolicyScope::POLICY_SCOPE_USER,
      policy::PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kPolicyValue, base::JSON_ALLOW_TRAILING_COMMAS),
      nullptr);

  // Scenario 1: Affiliated
  policy_map.SetDeviceAffiliationIds({"id"});
  policy_map.SetUserAffiliationIds({"id"});

  PrefValueMap prefs;
  policy::PolicyErrorMap errors;

  ASSERT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  handler->ApplyPolicySettings(policy_map, &prefs);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  bool affiliated = false;
  EXPECT_TRUE(
      prefs.GetBoolean(prefs::kProxyOverrideRulesAffiliation, &affiliated));
  EXPECT_TRUE(affiliated);
#endif

  // Scenario 2: Unaffiliated
  policy_map.SetUserAffiliationIds({"different_id"});

  // ApplyPolicySettings should update the affiliation preference even if the
  // policy value itself hasn't changed.
  ASSERT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  handler->ApplyPolicySettings(policy_map, &prefs);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  EXPECT_TRUE(
      prefs.GetBoolean(prefs::kProxyOverrideRulesAffiliation, &affiliated));
  EXPECT_FALSE(affiliated);
#endif
}

}  // namespace
}  // namespace proxy_config
