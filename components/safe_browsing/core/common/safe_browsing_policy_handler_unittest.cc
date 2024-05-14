// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingPolicyHandlerTest
    : public policy::ConfigurationPolicyPrefStoreTest {
 public:
  SafeBrowsingPolicyHandlerTest() {
    handler_list_.AddHandler(std::make_unique<SafeBrowsingPolicyHandler>());
  }

  const std::vector<int> kValidSBProtectionLevelValues{0, 1, 2};
  const std::vector<bool> kValidSBEnabledValues{false, true};

  bool GetExpectedSBEnabledValueForProtectionLevelValue(int protection_level) {
    switch (protection_level) {
      case 0:
        return false;
      case 1:
        return true;
      case 2:
        return true;
      default:
        NOTREACHED_IN_MIGRATION();
        return false;
    }
  }

  bool GetExpectedSBEnhancedValueForProtectionLevelValue(int protection_level) {
    switch (protection_level) {
      case 0:
        return false;
      case 1:
        return false;
      case 2:
        return true;
      default:
        NOTREACHED_IN_MIGRATION();
        return false;
    }
  }
};

TEST_F(SafeBrowsingPolicyHandlerTest, OnlyNewPolicySet) {
  EXPECT_FALSE(store_->GetValue(prefs::kSafeBrowsingEnabled, nullptr));
  EXPECT_FALSE(store_->GetValue(prefs::kSafeBrowsingEnhanced, nullptr));

  for (auto level : kValidSBProtectionLevelValues) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kSafeBrowsingProtectionLevel,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(level), nullptr);
    UpdateProviderPolicy(policy);

    const base::Value* value = nullptr;
    ASSERT_TRUE(store_->GetValue(prefs::kSafeBrowsingEnabled, &value));
    EXPECT_EQ(GetExpectedSBEnabledValueForProtectionLevelValue(level),
              value->GetBool());

    ASSERT_TRUE(store_->GetValue(prefs::kSafeBrowsingEnhanced, &value));
    EXPECT_EQ(GetExpectedSBEnhancedValueForProtectionLevelValue(level),
              value->GetBool());
  }
}

TEST_F(SafeBrowsingPolicyHandlerTest, OnlyLegacyPolicySet) {
  EXPECT_FALSE(store_->GetValue(prefs::kSafeBrowsingEnabled, nullptr));
  EXPECT_FALSE(store_->GetValue(prefs::kSafeBrowsingEnhanced, nullptr));

  for (auto enabled : kValidSBEnabledValues) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kSafeBrowsingEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(enabled), nullptr);
    UpdateProviderPolicy(policy);

    const base::Value* value = nullptr;
    ASSERT_TRUE(store_->GetValue(prefs::kSafeBrowsingEnabled, &value));
    EXPECT_EQ(enabled, value->GetBool());

    ASSERT_TRUE(store_->GetValue(prefs::kSafeBrowsingEnhanced, &value));
    EXPECT_FALSE(value->GetBool());
  }
}

TEST_F(SafeBrowsingPolicyHandlerTest, NewPolicyOverridesLegacyPolicy) {
  EXPECT_FALSE(store_->GetValue(prefs::kSafeBrowsingEnabled, nullptr));
  EXPECT_FALSE(store_->GetValue(prefs::kSafeBrowsingEnhanced, nullptr));

  for (auto level : kValidSBProtectionLevelValues) {
    for (auto enabled : kValidSBEnabledValues) {
      policy::PolicyMap policy;
      policy.Set(policy::key::kSafeBrowsingProtectionLevel,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(level), nullptr);
      policy.Set(policy::key::kSafeBrowsingEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(enabled), nullptr);
      UpdateProviderPolicy(policy);

      const base::Value* value = nullptr;
      ASSERT_TRUE(store_->GetValue(prefs::kSafeBrowsingEnabled, &value));
      EXPECT_EQ(GetExpectedSBEnabledValueForProtectionLevelValue(level),
                value->GetBool());

      ASSERT_TRUE(store_->GetValue(prefs::kSafeBrowsingEnhanced, &value));
      EXPECT_EQ(GetExpectedSBEnhancedValueForProtectionLevelValue(level),
                value->GetBool());
    }
  }
}

TEST_F(SafeBrowsingPolicyHandlerTest, LegacyPolicyAppliesIfNewPolicyInvalid) {
  EXPECT_FALSE(store_->GetValue(prefs::kSafeBrowsingEnabled, nullptr));
  EXPECT_FALSE(store_->GetValue(prefs::kSafeBrowsingEnhanced, nullptr));

  for (auto enabled : kValidSBEnabledValues) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kSafeBrowsingProtectionLevel,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(5 /* out of range */),
               nullptr);
    policy.Set(policy::key::kSafeBrowsingEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(enabled), nullptr);
    UpdateProviderPolicy(policy);

    const base::Value* value = nullptr;
    ASSERT_TRUE(store_->GetValue(prefs::kSafeBrowsingEnabled, &value));
    EXPECT_EQ(enabled, value->GetBool());

    ASSERT_TRUE(store_->GetValue(prefs::kSafeBrowsingEnhanced, &value));
    EXPECT_FALSE(value->GetBool());
  }
}

TEST_F(SafeBrowsingPolicyHandlerTest, NewPolicyAppliesIfLegacyPolicyInvalid) {
  EXPECT_FALSE(store_->GetValue(prefs::kSafeBrowsingEnabled, nullptr));
  EXPECT_FALSE(store_->GetValue(prefs::kSafeBrowsingEnhanced, nullptr));

  for (auto level : kValidSBProtectionLevelValues) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kSafeBrowsingProtectionLevel,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(level), nullptr);
    policy.Set(policy::key::kSafeBrowsingEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(4 /* wrong type */),
               nullptr);
    UpdateProviderPolicy(policy);

    const base::Value* value = nullptr;
    ASSERT_TRUE(store_->GetValue(prefs::kSafeBrowsingEnabled, &value));
    EXPECT_EQ(GetExpectedSBEnabledValueForProtectionLevelValue(level),
              value->GetBool());

    ASSERT_TRUE(store_->GetValue(prefs::kSafeBrowsingEnhanced, &value));
    EXPECT_EQ(GetExpectedSBEnhancedValueForProtectionLevelValue(level),
              value->GetBool());
  }
}

}  // namespace safe_browsing
