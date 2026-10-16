// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/proxy_provisioning_domains_policy_handler.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/json/json_reader.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace enterprise_net {
namespace {

constexpr char kSchema[] = R"(
{
  "type": "object",
  "properties": {
    "ProxyProvisioningDomains": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "pvd_id": {
            "type": "string"
          },
          "auth_config": {
            "type": "object",
            "properties": {
              "type": {
                "type": "string",
                "enum": [
                  "none",
                  "profile_bearer_token"
                ]
              },
              "scope": {
                "type": "string"
              }
            }
          },
          "extra_headers": {
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "key": {
                  "type": "string"
                },
                "value": {
                  "type": "string"
                }
              },
              "required": [
                "key",
                "value"
              ]
            }
          }
        },
        "required": [
          "pvd_id"
        ]
      }
    },
    "EnableProxyOverrideRulesForAllUsers": {
      "type": "integer"
    }
  }
})";

constexpr char kValidPolicyValue[] = R"([
  {
    "pvd_id": "https://api.prod.securegateway.goog:443",
    "auth_config": {
      "type": "profile_bearer_token",
      "scope": "cloud_secure_gateway"
    },
    "extra_headers": [
      {
        "key": "x-resource-key",
        "value": "arbitrary-resource-key-value"
      }
    ]
  }
])";

policy::Schema GetTestSchema() {
  ASSIGN_OR_RETURN(const auto validation_schema, policy::Schema::Parse(kSchema),
                   [](const auto& e) {
                     ADD_FAILURE() << e;
                     return policy::Schema();
                   });
  return validation_schema;
}

TEST(ProxyProvisioningDomainsPolicyHandlerTest, PolicyNotSet) {
  ProxyProvisioningDomainsPolicyHandler handler(GetTestSchema());
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  PrefValueMap prefs;

  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  handler.ApplyPolicySettings(policy_map, &prefs);
  const base::Value* value = nullptr;
  EXPECT_FALSE(prefs.GetValue(kProxyProvisioningDomains, &value));
}

TEST(ProxyProvisioningDomainsPolicyHandlerTest, ValidPolicy) {
  ProxyProvisioningDomainsPolicyHandler handler(GetTestSchema());
  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kProxyProvisioningDomains,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD,
                 base::JSONReader::Read(kValidPolicyValue,
                                        base::JSON_ALLOW_TRAILING_COMMAS),
                 nullptr);

  policy::PolicyErrorMap errors;
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy_map, &prefs);
  const base::Value* value = nullptr;
  ASSERT_TRUE(prefs.GetValue(kProxyProvisioningDomains, &value));
  ASSERT_TRUE(value->is_list());
  EXPECT_EQ(1u, value->GetList().size());
}

TEST(ProxyProvisioningDomainsPolicyHandlerTest, InvalidSchemaRejected) {
  ProxyProvisioningDomainsPolicyHandler handler(GetTestSchema());
  // Missing required "pvd_id" field.
  constexpr char kInvalidPolicyValue[] = R"([
    {
      "auth_config": {
        "type": "none"
      }
    }
  ])";

  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kProxyProvisioningDomains,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD,
                 base::JSONReader::Read(kInvalidPolicyValue,
                                        base::JSON_ALLOW_TRAILING_COMMAS),
                 nullptr);

  policy::PolicyErrorMap errors;
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.HasError(policy::key::kProxyProvisioningDomains));

  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy_map, &prefs);
  const base::Value* value = nullptr;
  EXPECT_FALSE(prefs.GetValue(kProxyProvisioningDomains, &value));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
class ProxyProvisioningDomainsPolicyHandlerAffiliationTest
    : public testing::TestWithParam<std::tuple<bool /*affiliated*/,
                                               bool /*user_scope*/,
                                               bool /*browser_managed*/,
                                               bool /*enable_for_all_users*/>> {
 public:
  bool affiliated() const { return std::get<0>(GetParam()); }
  bool user_scope() const { return std::get<1>(GetParam()); }
  bool browser_managed() const { return std::get<2>(GetParam()); }
  bool enable_for_all_users() const { return std::get<3>(GetParam()); }

  bool policy_expected() const {
    return !browser_managed() || !user_scope() || affiliated() ||
           enable_for_all_users();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ProxyProvisioningDomainsPolicyHandlerAffiliationTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

TEST_P(ProxyProvisioningDomainsPolicyHandlerAffiliationTest,
       AffiliationEnforcement) {
  ProxyProvisioningDomainsPolicyHandler handler(GetTestSchema());
  policy::PolicyMap policy_map;

  if (browser_managed()) {
    policy_map.SetDeviceAffiliationIds({"device_id"});
  }
  if (affiliated()) {
    policy_map.SetUserAffiliationIds({"device_id"});
  } else {
    policy_map.SetUserAffiliationIds({"other_user_id"});
  }

  if (enable_for_all_users()) {
    policy_map.Set(policy::key::kEnableProxyOverrideRulesForAllUsers,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  }

  policy_map.Set(
      policy::key::kProxyProvisioningDomains, policy::POLICY_LEVEL_MANDATORY,
      user_scope() ? policy::POLICY_SCOPE_USER : policy::POLICY_SCOPE_MACHINE,
      policy::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kValidPolicyValue,
                             base::JSON_ALLOW_TRAILING_COMMAS),
      nullptr);

  policy::PolicyErrorMap errors;
  EXPECT_EQ(policy_expected(),
            handler.CheckPolicySettings(policy_map, &errors));

  PrefValueMap prefs;
  if (policy_expected()) {
    EXPECT_TRUE(errors.empty());
    handler.ApplyPolicySettings(policy_map, &prefs);
    const base::Value* value = nullptr;
    EXPECT_TRUE(prefs.GetValue(kProxyProvisioningDomains, &value));
  } else {
    EXPECT_TRUE(errors.HasError(policy::key::kProxyProvisioningDomains));
    EXPECT_EQ(errors.GetErrorMessages(policy::key::kProxyProvisioningDomains),
              l10n_util::GetStringUTF16(IDS_POLICY_UNAFFILIATED_USER_ERROR));
  }
}

struct EnableForAllUsersTestCase {
  const char* json_value;
  bool valid_enable_policy;
  bool unaffiliated_policy_allowed;
};

class ProxyProvisioningDomainsPolicyHandlerEnableForAllUsersTest
    : public testing::TestWithParam<EnableForAllUsersTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    ProxyProvisioningDomainsPolicyHandlerEnableForAllUsersTest,
    testing::Values(
        EnableForAllUsersTestCase{"0",
                                  /*valid_enable_policy=*/true,
                                  /*unaffiliated_policy_allowed=*/false},
        EnableForAllUsersTestCase{"1",
                                  /*valid_enable_policy=*/true,
                                  /*unaffiliated_policy_allowed=*/true},
        EnableForAllUsersTestCase{"-1",
                                  /*valid_enable_policy=*/false,
                                  /*unaffiliated_policy_allowed=*/false},
        EnableForAllUsersTestCase{"2",
                                  /*valid_enable_policy=*/false,
                                  /*unaffiliated_policy_allowed=*/false},
        EnableForAllUsersTestCase{"\"invalid\"",
                                  /*valid_enable_policy=*/false,
                                  /*unaffiliated_policy_allowed=*/false},
        EnableForAllUsersTestCase{"true",
                                  /*valid_enable_policy=*/false,
                                  /*unaffiliated_policy_allowed=*/false}));

TEST_P(ProxyProvisioningDomainsPolicyHandlerEnableForAllUsersTest,
       ValidatesEnableForAllUsersValue) {
  ProxyProvisioningDomainsPolicyHandler handler(GetTestSchema());
  policy::PolicyMap policy_map;
  policy_map.SetDeviceAffiliationIds({"device_id"});
  policy_map.SetUserAffiliationIds({"other_user_id"});

  policy_map.Set(policy::key::kEnableProxyOverrideRulesForAllUsers,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD,
                 base::JSONReader::Read(GetParam().json_value,
                                        base::JSON_ALLOW_TRAILING_COMMAS),
                 nullptr);
  policy_map.Set(policy::key::kProxyProvisioningDomains,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::JSONReader::Read(kValidPolicyValue,
                                        base::JSON_ALLOW_TRAILING_COMMAS),
                 nullptr);

  policy::PolicyErrorMap errors;
  EXPECT_EQ(GetParam().unaffiliated_policy_allowed,
            handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_EQ(!GetParam().valid_enable_policy,
            errors.HasError(policy::key::kEnableProxyOverrideRulesForAllUsers));
  EXPECT_EQ(!GetParam().unaffiliated_policy_allowed,
            errors.HasError(policy::key::kProxyProvisioningDomains));
}

TEST(ProxyProvisioningDomainsPolicyHandlerTest,
     EnableForAllUsersUserScopeIgnored) {
  ProxyProvisioningDomainsPolicyHandler handler(GetTestSchema());
  policy::PolicyMap policy_map;
  policy_map.SetDeviceAffiliationIds({"device_id"});
  policy_map.SetUserAffiliationIds({"other_user_id"});

  // EnableProxyOverrideRulesForAllUsers set at user scope must NOT be honored.
  policy_map.Set(policy::key::kEnableProxyOverrideRulesForAllUsers,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  policy_map.Set(policy::key::kProxyProvisioningDomains,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::JSONReader::Read(kValidPolicyValue,
                                        base::JSON_ALLOW_TRAILING_COMMAS),
                 nullptr);

  policy::PolicyErrorMap errors;
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.HasError(policy::key::kProxyProvisioningDomains));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kProxyProvisioningDomains),
            l10n_util::GetStringUTF16(IDS_POLICY_UNAFFILIATED_USER_ERROR));

  // Also verify that passing nullptr for errors works safely.
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, nullptr));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace enterprise_net
