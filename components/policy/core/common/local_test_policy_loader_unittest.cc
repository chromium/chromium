// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/local_test_policy_loader.h"

#include <string>

#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class LocalTestPolicyLoaderTest : public ::testing::Test {
 public:
  void LoadAndVerifyPolicies(LocalTestPolicyLoader* loader,
                             const base::Value::List& expected_policies) {
    std::unique_ptr<PolicyBundle> bundle =
        std::make_unique<PolicyBundle>(loader->Load());
    PolicyMap& map =
        bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
    EXPECT_EQ(map.size(), expected_policies.size());
    for (auto& expected_policy : expected_policies) {
      const base::Value::Dict* policy_dict = &expected_policy.GetDict();
      const PolicyMap::Entry* actual_policy =
          map.Get(*policy_dict->FindString("name"));
      ASSERT_TRUE(actual_policy);

      EXPECT_EQ(static_cast<PolicyLevel>(*policy_dict->FindInt("level")),
                actual_policy->level);
      EXPECT_EQ(static_cast<PolicyScope>(*policy_dict->FindInt("scope")),
                actual_policy->scope);
      EXPECT_EQ(static_cast<PolicySource>(*policy_dict->FindInt("source")),
                actual_policy->source);

      ASSERT_TRUE(actual_policy->value_unsafe());
      EXPECT_EQ(*policy_dict->Find("value"), *actual_policy->value_unsafe());
    }
  }

  base::Value::Dict PolicyAsDict(PolicyLevel level,
                                 PolicyScope scope,
                                 PolicySource source,
                                 const std::string& name,
                                 base::Value value) {
    base::Value::Dict policy;
    policy.Set("level", level);
    policy.Set("scope", scope);
    policy.Set("source", source);
    policy.Set("name", name);
    policy.Set("value", std::move(value));

    return policy;
  }
};

TEST_F(LocalTestPolicyLoaderTest, LoadFromJson) {
  std::unique_ptr<LocalTestPolicyLoader> policy_loader =
      std::make_unique<LocalTestPolicyLoader>();
  policy_loader->SetPolicyListJson(R"(
  [
    {
      "level": 0,
      "scope": 0,
      "source": 0,
      "name": "a",
      "value": 3
    },
    {
      "level": 1,
      "scope": 1,
      "source": 2,
      "name": "b",
      "value": "test"
    }
  ])");
  base::Value::List expected_policies;
  expected_policies.Append(PolicyAsDict(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
      POLICY_SOURCE_ENTERPRISE_DEFAULT, /*name=*/"a", base::Value(3)));
  expected_policies.Append(
      PolicyAsDict(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                   POLICY_SOURCE_CLOUD, /*name=*/"b", base::Value("test")));

  LoadAndVerifyPolicies(policy_loader.get(), expected_policies);
}

TEST_F(LocalTestPolicyLoaderTest, SamePolicyCombination) {
  std::unique_ptr<LocalTestPolicyLoader> policy_loader =
      std::make_unique<LocalTestPolicyLoader>();

  // Set same policy with different source
  policy_loader->SetPolicyListJson(R"(
  [
    {
      "level": 1,
      "scope": 1,
      "source": 1,
      "name": "CloudReportingEnabled",
      "value": true
    },
    {
      "level": 1,
      "scope": 1,
      "source": 2,
      "name": "CloudReportingEnabled",
      "value": false
    }
  ])");

  std::unique_ptr<PolicyBundle> bundle =
      std::make_unique<PolicyBundle>(policy_loader->Load());
  const PolicyMap::Entry* entry =
      bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .Get(policy::key::kCloudReportingEnabled);

  PolicyMap::Entry expected_entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                  POLICY_SOURCE_CLOUD, base::Value(false),
                                  nullptr);
  expected_entry.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                       POLICY_SOURCE_COMMAND_LINE, base::Value(true), nullptr));
  expected_entry.AddMessage(PolicyMap::MessageType::kWarning,
                            IDS_POLICY_CONFLICT_DIFF_VALUE);

  EXPECT_TRUE(entry->Equals(expected_entry));
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(LocalTestPolicyLoaderTest, PrecedencePolicy) {
  std::unique_ptr<LocalTestPolicyLoader> policy_loader =
      std::make_unique<LocalTestPolicyLoader>();
  policy_loader->SetUserAffiliated(true);

  // Set same policy with different source
  policy_loader->SetPolicyListJson(R"(
  [
    {
      "level": 1,
      "scope": 1,
      "source": 2,
      "name": "CloudUserPolicyOverridesCloudMachinePolicy",
      "value": true
    },
    {
      "level": 1,
      "scope": 1,
      "source": 2,
      "name": "CloudReportingEnabled",
      "value": true
    },
    {
      "level": 1,
      "scope": 0,
      "source": 2,
      "name": "CloudReportingEnabled",
      "value": false
    }
  ])");

  std::unique_ptr<PolicyBundle> bundle =
      std::make_unique<PolicyBundle>(policy_loader->Load());

  const PolicyMap::Entry* entry =
      bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .Get(policy::key::kCloudReportingEnabled);

  PolicyMap::Entry expected_entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                                  POLICY_SOURCE_CLOUD, base::Value(false),
                                  nullptr);
  expected_entry.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                       POLICY_SOURCE_CLOUD, base::Value(true), nullptr));
  expected_entry.AddMessage(PolicyMap::MessageType::kWarning,
                            IDS_POLICY_CONFLICT_DIFF_VALUE);

  EXPECT_TRUE(entry->Equals(expected_entry));
}
#endif

TEST_F(LocalTestPolicyLoaderTest, InvalidInput_MissingScope) {
  std::unique_ptr<LocalTestPolicyLoader> policy_loader =
      std::make_unique<LocalTestPolicyLoader>();

  EXPECT_DEATH_IF_SUPPORTED(
      {
        policy_loader->SetPolicyListJson(R"(
    [
      {
        "level": 0,
        "source": 0,
        "name": "a",
        "value": 3
      },
      {
        "level": 1,
        "scope": 1,
        "source": 2,
        "name": "b",
        "value": "test"
      }
    ])");
      },
      "");
}

TEST_F(LocalTestPolicyLoaderTest, InvalidInput_MissingName) {
  std::unique_ptr<LocalTestPolicyLoader> policy_loader =
      std::make_unique<LocalTestPolicyLoader>();

  EXPECT_DEATH_IF_SUPPORTED(
      {
        policy_loader->SetPolicyListJson(R"(
    [
      {
        "level": 0,
        "scope": 0,
        "source": 0,
        "name": "a",
        "value": 3
      },
      {
        "level": 1,
        "scope": 1,
        "source": 2,
        "value": "test"
      }
    ])");
      },
      "");
}

TEST_F(LocalTestPolicyLoaderTest, InvalidInput_NotList) {
  std::unique_ptr<LocalTestPolicyLoader> policy_loader =
      std::make_unique<LocalTestPolicyLoader>();
  // Not a list
  EXPECT_DEATH_IF_SUPPORTED({ policy_loader->SetPolicyListJson(R"({})"); }, "");
}

TEST_F(LocalTestPolicyLoaderTest, InvalidInput_PolicyNotDict) {
  std::unique_ptr<LocalTestPolicyLoader> policy_loader =
      std::make_unique<LocalTestPolicyLoader>();

  // Entry not a dictionary
  EXPECT_DEATH_IF_SUPPORTED({ policy_loader->SetPolicyListJson(R"([[]])"); },
                            "");
}

TEST_F(LocalTestPolicyLoaderTest, InvalidInput_PolicyLevel) {
  std::unique_ptr<LocalTestPolicyLoader> policy_loader =
      std::make_unique<LocalTestPolicyLoader>();

  // Invalid policy level
  EXPECT_DEATH_IF_SUPPORTED(
      {
        policy_loader->SetPolicyListJson(R"(
    [
      {
        "level": 4,
        "scope": 0,
        "source": 0,
        "name": "a",
        "value": 3
      }
    ])");
      },
      "");
}

TEST_F(LocalTestPolicyLoaderTest, InvalidInput_PolicyScope) {
  std::unique_ptr<LocalTestPolicyLoader> policy_loader =
      std::make_unique<LocalTestPolicyLoader>();

  // Invalid policy scope
  EXPECT_DEATH_IF_SUPPORTED(
      {
        policy_loader->SetPolicyListJson(R"(
    [
      {
        "level": 0,
        "scope": 5,
        "source": 0,
        "name": "a",
        "value": 3
      }
    ])");
      },
      "");
}

TEST_F(LocalTestPolicyLoaderTest, InvalidInput_PolicySource) {
  std::unique_ptr<LocalTestPolicyLoader> policy_loader =
      std::make_unique<LocalTestPolicyLoader>();

  // Invalid policy source
  EXPECT_DEATH_IF_SUPPORTED(
      {
        policy_loader->SetPolicyListJson(R"(
    [
      {
        "level": 0,
        "scope": 0,
        "source": 11,
        "name": "a",
        "value": 3
      }
    ])");
      },
      "");
}

}  // namespace policy
