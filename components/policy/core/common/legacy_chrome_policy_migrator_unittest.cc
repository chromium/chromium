// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/legacy_chrome_policy_migrator.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {
const char kOldPolicy[] = "OldPolicy";
const char kNewPolicy[] = "NewPolicy";
const char kOtherPolicy[] = "OtherPolicy";

const int kOldValue = 111;
const int kNewValue = 222;
const int kTransformedValue = 333;
const int kOtherValue = 999;

void MultiplyByThree(base::Value* val) {
  *val = base::Value(val->GetInt() * 3);
}

void SetPolicy(PolicyMap* policy, const char* policy_name, base::Value value) {
  policy->Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              POLICY_SOURCE_CLOUD, std::move(value), nullptr);
}

}  // namespace

TEST(LegacyChromePolicyMigratorTest, CopyPolicyIfUnset) {
  PolicyBundle bundle;

  PolicyMap& chrome_map = bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));

  SetPolicy(&chrome_map, kOldPolicy, base::Value(kOldValue));
  SetPolicy(&chrome_map, kOtherPolicy, base::Value(kOtherValue));

  LegacyChromePolicyMigrator migrator(kOldPolicy, kNewPolicy);

  migrator.Migrate(&bundle);

  // kOldPolicy should have been copied to kNewPolicy, kOtherPolicy remains
  EXPECT_EQ(3u, chrome_map.size());
  ASSERT_TRUE(chrome_map.GetValue(kNewPolicy, base::Value::Type::INTEGER));
  // Old Value should be copied over.
  EXPECT_EQ(base::Value(kOldValue),
            *chrome_map.GetValue(kNewPolicy, base::Value::Type::INTEGER));
  // Other Value should be unchanged.
  EXPECT_EQ(base::Value(kOtherValue),
            *chrome_map.GetValue(kOtherPolicy, base::Value::Type::INTEGER));
  base::RepeatingCallback<std::u16string(int)> l10nlookup =
      base::BindRepeating(&l10n_util::GetStringUTF16);
  // Old policy should always be marked deprecated
  EXPECT_FALSE(
      chrome_map.Get(kOldPolicy)
          ->GetLocalizedMessages(PolicyMap::MessageType::kError, l10nlookup)
          .empty());
  EXPECT_FALSE(
      chrome_map.Get(kNewPolicy)
          ->GetLocalizedMessages(PolicyMap::MessageType::kWarning, l10nlookup)
          .empty());
}

TEST(LegacyChromePolicyMigratorTest, TransformPolicy) {
  PolicyBundle bundle;

  PolicyMap& chrome_map = bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));

  SetPolicy(&chrome_map, kOldPolicy, base::Value(kOldValue));

  LegacyChromePolicyMigrator migrator(kOldPolicy, kNewPolicy,
                                      base::BindRepeating(&MultiplyByThree));

  migrator.Migrate(&bundle);

  ASSERT_TRUE(chrome_map.GetValue(kNewPolicy, base::Value::Type::INTEGER));
  // Old Value should be transformed
  EXPECT_EQ(base::Value(kTransformedValue),
            *chrome_map.GetValue(kNewPolicy, base::Value::Type::INTEGER));
}

TEST(LegacyChromePolicyMigratorTest, IgnoreOldIfNewIsSet) {
  PolicyBundle bundle;

  PolicyMap& chrome_map = bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));

  SetPolicy(&chrome_map, kOldPolicy, base::Value(kOldValue));
  SetPolicy(&chrome_map, kNewPolicy, base::Value(kNewValue));

  LegacyChromePolicyMigrator migrator(kOldPolicy, kNewPolicy);

  migrator.Migrate(&bundle);
  // New Value is unchanged
  EXPECT_EQ(base::Value(kNewValue),
            *chrome_map.GetValue(kNewPolicy, base::Value::Type::INTEGER));
  // Should be no warning on new policy
  base::RepeatingCallback<std::u16string(int)> l10nlookup =
      base::BindRepeating(&l10n_util::GetStringUTF16);
  // Old policy should always be marked deprecated
  EXPECT_FALSE(
      chrome_map.Get(kOldPolicy)
          ->GetLocalizedMessages(PolicyMap::MessageType::kError, l10nlookup)
          .empty());
  // No warnings on new policy because it was unchanged.
  EXPECT_TRUE(
      chrome_map.Get(kNewPolicy)
          ->GetLocalizedMessages(PolicyMap::MessageType::kWarning, l10nlookup)
          .empty());
  EXPECT_TRUE(
      chrome_map.Get(kNewPolicy)
          ->GetLocalizedMessages(PolicyMap::MessageType::kError, l10nlookup)
          .empty());
}

}  // namespace policy
