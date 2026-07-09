// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/permissions/autofill_policy_service.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace {

class AutofillPolicyServiceTest : public testing::Test {
 public:
  AutofillPolicyServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableAutofillSettingsEnterprisePolicy);
    prefs_.registry()->RegisterListPref(prefs::kAutofillTypesBlocked);
    prefs_.registry()->RegisterBooleanPref(prefs::kAutofillProfileEnabled,
                                           true);
    prefs_.registry()->RegisterBooleanPref(prefs::kAutofillCreditCardEnabled,
                                           true);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kAutofillAiIdentityEntitiesEnabled, true);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kAutofillAiTravelEntitiesEnabled, true);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kAutofillAiShoppingEntitiesEnabled, true);
    service_ = std::make_unique<AutofillPolicyService>(&prefs_);
  }

  void SetPolicy(base::ListValue blocked_list) {
    prefs_.SetList(prefs::kAutofillTypesBlocked, std::move(blocked_list));
  }

  AutofillPolicyService* service() { return service_.get(); }

  bool IsAutofillTypeBlockedByPolicy(
      const GURL& url,
      AutofillClient::AutofillPolicyDataCategory category) {
    bool instance_result =
        service()->IsAutofillTypeBlockedByPolicy(url, category);
    bool static_result =
        AutofillPolicyService::IsAutofillTypeBlockedByPolicyFromPref(
            prefs_, url, category);
    EXPECT_EQ(instance_result, static_result);
    return instance_result;
  }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AutofillPolicyService> service_;
};

// Tests that when no enterprise policy rules are configured in preference, all
// Autofill data categories are permitted across all navigation URLs.
TEST_F(AutofillPolicyServiceTest, EmptyPolicyAllowsAllAutofillCategories) {
  const GURL url("https://www.example.com");
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      url, AutofillClient::AutofillPolicyDataCategory::kContactInfo));
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      url, AutofillClient::AutofillPolicyDataCategory::kPayments));
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      url, AutofillClient::AutofillPolicyDataCategory::kIdentityDocs));
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      url, AutofillClient::AutofillPolicyDataCategory::kTravel));
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      url, AutofillClient::AutofillPolicyDataCategory::kShopping));
}

// Tests that categories explicitly configured in the policy rule are blocked
// on matching subdomains, while unlisted categories remain allowed.
TEST_F(AutofillPolicyServiceTest,
       ConfiguredCategoryOnMatchingDomainBlocksAutofill) {
  base::ListValue blocked_list;
  base::DictValue entry;
  entry.Set("url_pattern", "https://[*.]example.com");
  base::ListValue blocked_types;
  blocked_types.Append("contact_info");
  blocked_types.Append("identity_docs");
  entry.Set("blocked_types", std::move(blocked_types));
  blocked_list.Append(std::move(entry));
  SetPolicy(std::move(blocked_list));

  EXPECT_TRUE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kContactInfo));
  EXPECT_TRUE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kIdentityDocs));
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kPayments));
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kTravel));
}

// Tests that domain block rules strictly scope to matching URL patterns
// and do not affect unrelated navigation URLs.
TEST_F(AutofillPolicyServiceTest, ActiveRuleOnUnrelatedDomainAllowsAutofill) {
  base::ListValue blocked_list;
  base::DictValue entry;
  entry.Set("url_pattern", "https://[*.]example.com");
  base::ListValue blocked_types;
  blocked_types.Append("contact_info");
  entry.Set("blocked_types", std::move(blocked_types));
  blocked_list.Append(std::move(entry));
  SetPolicy(std::move(blocked_list));

  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.google.com"),
      AutofillClient::AutofillPolicyDataCategory::kContactInfo));
}

// Tests that when the enterprise policy feature flag is disabled, all
// Autofill data categories remain allowed regardless of configured rules.
TEST_F(AutofillPolicyServiceTest, FeatureFlagDisabledAllowsAllAutofill) {
  base::test::ScopedFeatureList disabled_features;
  disabled_features.InitAndDisableFeature(
      features::kAutofillEnableAutofillSettingsEnterprisePolicy);

  base::ListValue blocked_list;
  base::DictValue entry;
  entry.Set("url_pattern", "https://[*.]example.com");
  base::ListValue blocked_types;
  blocked_types.Append("contact_info");
  entry.Set("blocked_types", std::move(blocked_types));
  blocked_list.Append(std::move(entry));
  SetPolicy(std::move(blocked_list));

  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kContactInfo));
}

// Tests that when a user preference is disabled, IsAutofillTypeBlockedByPolicy
// returns true (blocked) regardless of whether there is an enterprise policy.
TEST_F(AutofillPolicyServiceTest, UserSettingDisabledBlocksAutofill) {
  const GURL url("https://www.example.com");

  // 1. Initially enabled.
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      url, AutofillClient::AutofillPolicyDataCategory::kTravel));

  // 2. Disable user setting.
  prefs_.SetBoolean(prefs::kAutofillAiTravelEntitiesEnabled, false);

  // 3. Getter should now return true (blocked).
  EXPECT_TRUE(IsAutofillTypeBlockedByPolicy(
      url, AutofillClient::AutofillPolicyDataCategory::kTravel));

  // Other categories should still be allowed if enabled.
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      url, AutofillClient::AutofillPolicyDataCategory::kContactInfo));
}

// Tests that live preference changes pushed by Group Policy dynamically
// update the in-memory blocking cache without restarting the service.
TEST_F(AutofillPolicyServiceTest,
       LivePreferenceChangeUpdatesBlockingCacheDynamically) {
  const GURL url("https://www.example.com");

  // 1. Initial state: policy list is empty, URL should not be blocked.
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      url, AutofillClient::AutofillPolicyDataCategory::kContactInfo));

  // 2. Set policy to block contact_info on example.com.
  base::ListValue blocked_list;
  base::DictValue entry;
  entry.Set("url_pattern", "https://[*.]example.com");
  base::ListValue blocked_types;
  blocked_types.Append("contact_info");
  entry.Set("blocked_types", std::move(blocked_types));
  blocked_list.Append(std::move(entry));
  SetPolicy(std::move(blocked_list));

  // 3. Observer should have updated the cache: URL should now be blocked.
  EXPECT_TRUE(IsAutofillTypeBlockedByPolicy(
      url, AutofillClient::AutofillPolicyDataCategory::kContactInfo));

  // 4. Update policy to empty list (revoking policy).
  base::ListValue empty_list;
  SetPolicy(std::move(empty_list));

  // 5. Cache should naturally be cleared: URL should no longer be blocked.
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      url, AutofillClient::AutofillPolicyDataCategory::kContactInfo));
}

// Tests that a rule specifying a global wildcard URL pattern ('*') blocks its
// configured categories across all navigation URLs.
TEST_F(AutofillPolicyServiceTest,
       GlobalWildcardPatternBlocksAutofillAcrossAllDomains) {
  base::ListValue blocked_list;
  base::DictValue entry;
  entry.Set("url_pattern", "*");
  base::ListValue blocked_types;
  blocked_types.Append("payments");
  entry.Set("blocked_types", std::move(blocked_types));
  blocked_list.Append(std::move(entry));
  SetPolicy(std::move(blocked_list));

  EXPECT_TRUE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kPayments));
  EXPECT_TRUE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.google.com"),
      AutofillClient::AutofillPolicyDataCategory::kPayments));
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kContactInfo));
}

// Tests that when both global wildcard rules and domain-specific rules target
// the same navigation URL, their blocked categories merge into a union.
TEST_F(AutofillPolicyServiceTest,
       MultipleRulesTargetingSameDomainMergeBlockedCategoriesUnion) {
  base::ListValue blocked_list;

  // Wildcard entry: blocks payments everywhere.
  base::DictValue entry1;
  entry1.Set("url_pattern", "*");
  base::ListValue blocked_types1;
  blocked_types1.Append("payments");
  entry1.Set("blocked_types", std::move(blocked_types1));
  blocked_list.Append(std::move(entry1));

  // Specific entry: blocks contact_info on example.com.
  base::DictValue entry2;
  entry2.Set("url_pattern", "https://[*.]example.com");
  base::ListValue blocked_types2;
  blocked_types2.Append("contact_info");
  entry2.Set("blocked_types", std::move(blocked_types2));
  blocked_list.Append(std::move(entry2));

  SetPolicy(std::move(blocked_list));

  // example.com matches both wildcard '*' and '[*.]example.com' (union).
  EXPECT_TRUE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kPayments));
  EXPECT_TRUE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kContactInfo));

  // google.com only matches wildcard '*'.
  EXPECT_TRUE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.google.com"),
      AutofillClient::AutofillPolicyDataCategory::kPayments));
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.google.com"),
      AutofillClient::AutofillPolicyDataCategory::kContactInfo));
}

// Tests that policy rules with malformed URL patterns are filtered out
// silently without invalidating valid co-existing rules.
TEST_F(AutofillPolicyServiceTest,
       MalformedUrlPatternRuleIsIgnoredWithoutAffectingValidRules) {
  base::ListValue blocked_list;

  base::DictValue valid_entry;
  valid_entry.Set("url_pattern", "https://[*.]example.com");
  base::ListValue valid_types;
  valid_types.Append("contact_info");
  valid_entry.Set("blocked_types", std::move(valid_types));
  blocked_list.Append(std::move(valid_entry));

  base::DictValue invalid_entry;
  invalid_entry.Set("url_pattern", "invalid-pattern-123");
  base::ListValue invalid_types;
  invalid_types.Append("travel");
  invalid_entry.Set("blocked_types", std::move(invalid_types));
  blocked_list.Append(std::move(invalid_entry));

  SetPolicy(std::move(blocked_list));

  EXPECT_TRUE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kContactInfo));
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kTravel));
}

TEST_F(AutofillPolicyServiceTest, ShoppingCategoryBlocksAutofill) {
  base::ListValue blocked_list;
  base::DictValue entry;
  entry.Set("url_pattern", "https://[*.]example.com");
  base::ListValue blocked_types;
  blocked_types.Append("shopping");
  entry.Set("blocked_types", std::move(blocked_types));
  blocked_list.Append(std::move(entry));
  SetPolicy(std::move(blocked_list));

  EXPECT_TRUE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kShopping));
  EXPECT_FALSE(IsAutofillTypeBlockedByPolicy(
      GURL("https://www.google.com"),
      AutofillClient::AutofillPolicyDataCategory::kShopping));
}

}  // namespace
}  // namespace autofill
