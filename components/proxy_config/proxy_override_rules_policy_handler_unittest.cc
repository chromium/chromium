// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/proxy_override_rules_policy_handler.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/types/expected_macros.h"
#include "components/policy/core/browser/policy_error_map.h"
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
    }
  }
})";

constexpr std::pair<const char*, const char16_t*> kInvalidTestCases[] = {
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
};

class ProxyOverrideRulesPolicyHandlerTest
    : public testing::TestWithParam<std::pair<const char*, const char16_t*>> {
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
    policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                   policy::PolicyScope::POLICY_SCOPE_MACHINE, policy_source,
                   base::JSONReader::Read(policy_value(),
                                          base::JSON_ALLOW_TRAILING_COMMAS),
                   nullptr);

    return policy_map;
  }

  const char* policy_value() { return GetParam().first; }
  const char16_t* expected_messages() { return GetParam().second; }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ProxyOverrideRulesPolicyHandlerTest,
                         testing::ValuesIn(kInvalidTestCases));

TEST_P(ProxyOverrideRulesPolicyHandlerTest, Test) {
  policy::PolicyMap map = CreatePolicyMap(
      policy_value(), policy::PolicySource::POLICY_SOURCE_CLOUD);
  auto handler = std::make_unique<ProxyOverrideRulesPolicyHandler>(schema());

  policy::PolicyErrorMap errors;

  // Invalid list entries are tolerated, so in that case `CheckPolicySettings`
  // will still return true.
  ASSERT_TRUE(handler->CheckPolicySettings(map, &errors));

  ASSERT_FALSE(errors.empty());
  ASSERT_TRUE(errors.HasError(kPolicyName));

  std::u16string messages = errors.GetErrorMessages(kPolicyName);
  ASSERT_EQ(messages, expected_messages());
}

}  // namespace
}  // namespace proxy_config
