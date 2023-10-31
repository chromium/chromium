// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/site_search_policy_handler.h"

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/enterprise_site_search_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AllOf;
using testing::ElementsAre;

namespace policy {

namespace {

struct TestCase {
  const char* name;
  const char* shortcut;
  const char* url;
  const char* favicon;
} kTestCases[] = {
    {.name = "work name",
     .shortcut = "work",
     .url = "https://work.com/{searchTerms}",
     .favicon = "https://work.com/favicon.ico"},
    {.name = "docs name",
     .shortcut = "docs",
     .url = "https://docs.com/{searchTerms}",
     .favicon = "https://docs.com/favicon.ico"},
};

// Creates a simple list item for the site search policy.
base::Value::Dict GenerateSiteSearchPolicyEntry(TestCase test_case) {
  base::Value::Dict entry;
  entry.Set(SiteSearchPolicyHandler::kName, test_case.name);
  entry.Set(SiteSearchPolicyHandler::kShortcut, test_case.shortcut);
  entry.Set(SiteSearchPolicyHandler::kUrl, test_case.url);
  return entry;
}

// Accepts a dictionary that has a string field |field_name| with value
// |expected_value|.
MATCHER_P2(HasStringField,
           field_name,
           expected_value,
           base::StringPrintf("%s string field `%s` with value `%s`",
                              negation ? "does not contain" : "contains",
                              field_name,
                              expected_value)) {
  const std::string* dict_value = (arg).GetDict().FindString(field_name);
  return dict_value && *dict_value == expected_value;
}

// Accepts a dictionary that has a boolean field |field_name| with value
// |expected_value|.
MATCHER_P2(HasBooleanField,
           field_name,
           expected_value,
           base::StringPrintf("%s boolean field `%s` with value `%d`",
                              negation ? "does not contain" : "contains",
                              field_name,
                              expected_value)) {
  absl::optional<bool> dict_value = (arg).GetDict().FindBool(field_name);
  return dict_value && *dict_value == expected_value;
}

// Accepts a dictionary that has a double field |field_name| with non-zero
// value.
MATCHER_P(HasDoubleField,
          field_name,
          base::StringPrintf("%s double field `%s` with non-zero value",
                             negation ? "does not contain" : "contains",
                             field_name)) {
  absl::optional<double> dict_value = (arg).GetDict().FindDouble(field_name);
  return dict_value && *dict_value != 0.0;
}

// Returns a matcher that accepts entries for the pref corresponding to the
// site search policy. Field values are obtained from |test_case|.
testing::Matcher<const base::Value&> IsSiteSearchEntry(TestCase test_case) {
  return AllOf(
      HasStringField(DefaultSearchManager::kShortName, test_case.name),
      HasStringField(DefaultSearchManager::kKeyword, test_case.shortcut),
      HasStringField(DefaultSearchManager::kURL, test_case.url),
      HasBooleanField(DefaultSearchManager::kCreatedByPolicy, true),
      HasBooleanField(DefaultSearchManager::kEnforcedByPolicy, false),
      HasStringField(DefaultSearchManager::kFaviconURL, test_case.favicon),
      HasBooleanField(DefaultSearchManager::kSafeForAutoReplace, false),
      HasDoubleField(DefaultSearchManager::kDateCreated),
      HasDoubleField(DefaultSearchManager::kLastModified));
}

}  // namespace

TEST(SiteSearchPolicyHandlerTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  policy_value.Append(GenerateSiteSearchPolicyEntry(kTestCases[0]));
  policy_value.Append(GenerateSiteSearchPolicyEntry(kTestCases[1]));

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.empty());

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* unset = nullptr;
  ASSERT_FALSE(prefs.GetValue(
      EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName, &unset));
}

TEST(SiteSearchPolicyHandlerTest, PolicyNotSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.empty());

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_TRUE(providers->GetList().empty());
}

TEST(SiteSearchPolicyHandlerTest, ValidSiteSearchEntries) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  policy_value.Append(GenerateSiteSearchPolicyEntry(kTestCases[0]));
  policy_value.Append(GenerateSiteSearchPolicyEntry(kTestCases[1]));

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.empty());

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(providers->GetList(),
              ElementsAre(IsSiteSearchEntry(kTestCases[0]),
                          IsSiteSearchEntry(kTestCases[1])));
}

}  // namespace policy
