// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_bundle.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/values.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kPolicyClashing0[] = "policy-clashing-0";
const char kPolicyClashing1[] = "policy-clashing-1";
const char kPolicy0[] = "policy-0";
const char kPolicy1[] = "policy-1";
const char kPolicy2[] = "policy-2";
const char kExtension0[] = "extension-0";
const char kExtension1[] = "extension-1";
const char kExtension2[] = "extension-2";
const char kExtension3[] = "extension-3";

// Adds test policies to |policy|.
void AddTestPolicies(PolicyMap* policy) {
  policy->Set("mandatory-user", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(123), nullptr);
  policy->Set("mandatory-machine", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
              POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("omg"),
              nullptr);
  policy->Set("recommended-user", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
              POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true),
              nullptr);
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetBoolean("false", false);
  dict->SetInteger("int", 456);
  dict->SetString("str", "bbq");
  policy->Set("recommended-machine", POLICY_LEVEL_RECOMMENDED,
              POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, std::move(dict),
              nullptr);
}

// Adds test policies to |policy| based on the parameters:
// - kPolicyClashing0 mapped to |value|, user mandatory
// - kPolicyClashing1 mapped to |value|, with |level| and |scope|
// - |name| mapped to |value|, user mandatory
void AddTestPoliciesWithParams(PolicyMap *policy,
                               const char* name,
                               int value,
                               PolicyLevel level,
                               PolicyScope scope) {
  policy->Set(kPolicyClashing0, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(value),
              nullptr);
  policy->Set(kPolicyClashing1, level, scope, POLICY_SOURCE_CLOUD,
              std::make_unique<base::Value>(value), nullptr);
  policy->Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(value),
              nullptr);
}

// Returns true if |bundle| is empty.
bool IsEmpty(const PolicyBundle& bundle) {
  return bundle.begin() == bundle.end();
}

}  // namespace

TEST(PolicyBundleTest, Get) {
  PolicyBundle bundle;
  EXPECT_TRUE(IsEmpty(bundle));

  AddTestPolicies(&bundle.Get(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())));
  EXPECT_FALSE(IsEmpty(bundle));

  PolicyMap policy;
  AddTestPolicies(&policy);
  EXPECT_TRUE(bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                         std::string())).Equals(policy));

  PolicyBundle::const_iterator it = bundle.begin();
  ASSERT_TRUE(it != bundle.end());
  EXPECT_EQ(POLICY_DOMAIN_CHROME, it->first.domain);
  EXPECT_EQ("", it->first.component_id);
  ASSERT_TRUE(it->second);
  EXPECT_TRUE(it->second->Equals(policy));
  ++it;
  EXPECT_TRUE(it == bundle.end());
  EXPECT_TRUE(bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                         kExtension0)).empty());

  EXPECT_FALSE(IsEmpty(bundle));
  bundle.Clear();
  EXPECT_TRUE(IsEmpty(bundle));
}

TEST(PolicyBundleTest, SwapAndCopy) {
  PolicyBundle bundle0;
  PolicyBundle bundle1;

  AddTestPolicies(&bundle0.Get(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())));
  AddTestPolicies(&bundle0.Get(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension0)));
  EXPECT_FALSE(IsEmpty(bundle0));
  EXPECT_TRUE(IsEmpty(bundle1));

  PolicyMap policy;
  AddTestPolicies(&policy);
  EXPECT_TRUE(bundle0.Get(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                          std::string())).Equals(policy));
  EXPECT_TRUE(bundle0.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                          kExtension0)).Equals(policy));

  bundle0.Swap(&bundle1);
  EXPECT_TRUE(IsEmpty(bundle0));
  EXPECT_FALSE(IsEmpty(bundle1));

  EXPECT_TRUE(bundle1.Get(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                          std::string())).Equals(policy));
  EXPECT_TRUE(bundle1.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                          kExtension0)).Equals(policy));

  bundle0.CopyFrom(bundle1);
  EXPECT_FALSE(IsEmpty(bundle0));
  EXPECT_FALSE(IsEmpty(bundle1));
  EXPECT_TRUE(bundle0.Get(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                          std::string())).Equals(policy));
  EXPECT_TRUE(bundle0.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                          kExtension0)).Equals(policy));
}

TEST(PolicyBundleTest, MergeFrom) {
  // Each bundleN has kExtensionN. Each bundle also has policy for
  // chrome and kExtension3.
  // |bundle0| has the highest priority, |bundle2| the lowest.
  PolicyBundle bundle0;
  PolicyBundle bundle1;
  PolicyBundle bundle2;

  PolicyMap policy0;
  AddTestPoliciesWithParams(
      &policy0, kPolicy0, 0u, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER);
  bundle0.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .CopyFrom(policy0);
  bundle0.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension0))
      .CopyFrom(policy0);
  bundle0.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension3))
      .CopyFrom(policy0);

  PolicyMap policy1;
  AddTestPoliciesWithParams(
      &policy1, kPolicy1, 1u, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE);
  bundle1.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .CopyFrom(policy1);
  bundle1.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension1))
      .CopyFrom(policy1);
  bundle1.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension3))
      .CopyFrom(policy1);

  PolicyMap policy2;
  AddTestPoliciesWithParams(
      &policy2, kPolicy2, 2u, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
  bundle2.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .CopyFrom(policy2);
  bundle2.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension2))
      .CopyFrom(policy2);
  bundle2.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension3))
      .CopyFrom(policy2);

  // Merge in order of decreasing priority.
  PolicyBundle merged;
  merged.MergeFrom(bundle0);
  merged.MergeFrom(bundle1);
  merged.MergeFrom(bundle2);
  PolicyBundle empty_bundle;
  merged.MergeFrom(empty_bundle);

  // chrome and kExtension3 policies are merged:
  // - kPolicyClashing0 comes from bundle0, which has the highest priority;
  // - kPolicyClashing1 comes from bundle1, which has the highest level/scope
  //   combination;
  // - kPolicyN are merged from each bundle.
  PolicyMap expected;
  expected.Set(kPolicyClashing0, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(0), nullptr);
  expected.GetMutable(kPolicyClashing0)
      ->AddConflictingPolicy(policy1.Get(kPolicyClashing0)->DeepCopy());
  expected.GetMutable(kPolicyClashing0)
      ->AddConflictingPolicy(policy2.Get(kPolicyClashing0)->DeepCopy());
  expected.GetMutable(kPolicyClashing0)
      ->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected.GetMutable(kPolicyClashing0)
      ->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected.Set(kPolicyClashing1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
               POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(1), nullptr);
  expected.GetMutable(kPolicyClashing1)
      ->AddConflictingPolicy(policy0.Get(kPolicyClashing1)->DeepCopy());
  expected.GetMutable(kPolicyClashing1)
      ->AddConflictingPolicy(policy2.Get(kPolicyClashing1)->DeepCopy());
  expected.GetMutable(kPolicyClashing1)
      ->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected.GetMutable(kPolicyClashing1)
      ->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected.Set(kPolicy0, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(0), nullptr);
  expected.Set(kPolicy1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(1), nullptr);
  expected.Set(kPolicy2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(2), nullptr);
  EXPECT_TRUE(merged.Get(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                         std::string())).Equals(expected));
  EXPECT_TRUE(merged.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                         kExtension3)).Equals(expected));
  // extension0 comes only from bundle0.
  EXPECT_TRUE(merged.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                         kExtension0)).Equals(policy0));
  // extension1 comes only from bundle1.
  EXPECT_TRUE(merged.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                         kExtension1)).Equals(policy1));
  // extension2 comes only from bundle2.
  EXPECT_TRUE(merged.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                         kExtension2)).Equals(policy2));
}

TEST(PolicyBundleTest, Equals) {
  PolicyBundle bundle;
  AddTestPolicies(&bundle.Get(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())));
  AddTestPolicies(&bundle.Get(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension0)));

  PolicyBundle other;
  EXPECT_FALSE(bundle.Equals(other));
  other.CopyFrom(bundle);
  EXPECT_TRUE(bundle.Equals(other));

  AddTestPolicies(&bundle.Get(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension1)));
  EXPECT_FALSE(bundle.Equals(other));
  other.CopyFrom(bundle);
  EXPECT_TRUE(bundle.Equals(other));
  AddTestPolicies(&other.Get(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension2)));
  EXPECT_FALSE(bundle.Equals(other));

  other.CopyFrom(bundle);
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(kPolicy0, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(123), nullptr);
  EXPECT_FALSE(bundle.Equals(other));
  other.CopyFrom(bundle);
  EXPECT_TRUE(bundle.Equals(other));
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(kPolicy0, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
           POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(123), nullptr);
  EXPECT_FALSE(bundle.Equals(other));

  // Test non-const Get().
  bundle.Clear();
  other.Clear();
  PolicyMap& policy_map =
      bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  EXPECT_TRUE(bundle.Equals(other));
  policy_map.Set(kPolicy0, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(123),
                 nullptr);
  EXPECT_FALSE(bundle.Equals(other));
}

}  // namespace policy
