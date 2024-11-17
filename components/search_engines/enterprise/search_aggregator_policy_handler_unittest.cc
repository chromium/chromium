// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise/search_aggregator_policy_handler.h"

#include <array>
#include <iterator>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/enterprise/enterprise_search_manager.h"
#include "components/search_engines/enterprise/field_validation_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::AllOf;
using testing::Conditional;
using testing::ElementsAre;

namespace policy {

namespace {

// Represents field values for EnterpriseSearchAggregatorSettings policy, used
// for generating policy value entries. Fields set as nullptr will not be added
// to the entry dictionary.
struct TestSearchAggregator {
  const char* name;
  const char* shortcut;
  const char* search_url;
  const char* suggest_url;
  const char* icon_url;
  // If not-zero, the ID of the error message expected in the policy error map.
  const int expected_error_msg_id;
};

// Used for tests that require a valid search aggregator.
TestSearchAggregator kValidTestSearchAggregator = {
    .name = "work name",
    .shortcut = "work",
    .search_url = "https://work.com/{searchTerms}",
    .suggest_url = "https://work.com/suggest",
    .icon_url = "https://work.com/favicon.ico",
};

TestSearchAggregator kValidTestSearchAggregatorNoIcon = {
    .name = "work name",
    .shortcut = "work",
    .search_url = "https://work.com/{searchTerms}",
    .suggest_url = "https://work.com/suggest",
};

// Used for tests of search aggregators missing a required field.
auto kTestSearchAggregatorMissingRequiredField =
    std::to_array<TestSearchAggregator>({
        {
            .shortcut = "work",
            .search_url = "https://work.com/{searchTerms}",
            .suggest_url = "https://work.com/suggest",
        },
        {
            .name = "work name",
            .search_url = "https://work.com/{searchTerms}",
            .suggest_url = "https://work.com/suggest",
        },
        {
            .name = "work name",
            .shortcut = "work",
            .suggest_url = "https://work.com/suggest",
        },
        {
            .name = "work name",
            .shortcut = "work",
            .search_url = "https://work.com/{searchTerms}",
        },
    });

// Used for tests of search aggregators with an empty required field.
auto kTestSearchAggregatorEmptyRequiredField =
    std::to_array<TestSearchAggregator>({
        {
            .name = "",
            .shortcut = "work",
            .search_url = "https://work.com/{searchTerms}",
            .suggest_url = "https://work.com/suggest",
            .expected_error_msg_id =
                IDS_POLICY_SITE_SEARCH_SETTINGS_NAME_IS_EMPTY,
        },
        {
            .name = "work name",
            .shortcut = "",
            .search_url = "https://work.com/{searchTerms}",
            .suggest_url = "https://work.com/suggest",
            .expected_error_msg_id =
                IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_IS_EMPTY,
        },
        {
            .name = "work name",
            .shortcut = "work",
            .search_url = "",
            .suggest_url = "https://work.com/suggest",
            .expected_error_msg_id =
                IDS_POLICY_SITE_SEARCH_SETTINGS_URL_IS_EMPTY,
        },
        {
            .name = "work name",
            .shortcut = "work",
            .search_url = "https://work.com/{searchTerms}",
            .suggest_url = "",
            .expected_error_msg_id =
                IDS_POLICY_SITE_SEARCH_SETTINGS_URL_IS_EMPTY,
        },
    });

// Used for tests of search aggregators with shortcut containing a space.
auto kTestSearchAggregatorShortcutWithSpaces =
    std::to_array<TestSearchAggregator>({
        {
            .name = "work",
            .shortcut = " work",
            .search_url = "https://work.com/{searchTerms}",
            .suggest_url = "https://work.com/suggest",
        },
        {
            .name = "work",
            .shortcut = "work ",
            .search_url = "https://work.com/{searchTerms}",
            .suggest_url = "https://work.com/suggest",
        },
        {
            .name = "work",
            .shortcut = "wo rk",
            .search_url = "https://work.com/{searchTerms}",
            .suggest_url = "https://work.com/suggest",
        },
    });

// Used for tests of search aggregators with shortcut starting with "@".
TestSearchAggregator kTestSearchAggregatorShortcutStartsWithAt = {
    .name = "work",
    .shortcut = "@work",
    .search_url = "https://work.com/{searchTerms}",
    .suggest_url = "https://work.com/suggest",
};

// Used for tests of search aggregators with invalid search URL.
TestSearchAggregator kTestSearchAggregatorSearchUrlNonHttps = {
    .name = "work",
    .shortcut = "work",
    .search_url = "http://work.com/{searchTerms}",
    .suggest_url = "https://work.com/suggest",
    .icon_url = "https://work.com/favicon.ico",
};

TestSearchAggregator kTestSearchAggregatorSuggestUrlNonHttps = {
    .name = "work",
    .shortcut = "work",
    .search_url = "https://work.com/{searchTerms}",
    .suggest_url = "http://work.com/suggest",
    .icon_url = "https://work.com/favicon.ico",
};

TestSearchAggregator kTestSearchAggregatorIconUrlNonHttps = {
    .name = "work",
    .shortcut = "work",
    .search_url = "https://work.com/{searchTerms}",
    .suggest_url = "https://work.com/suggest",
    .icon_url = "http://work.com/favicon.ico",
};

TestSearchAggregator kTestSearchAggregatorNoStringReplacementSearchUrl = {
    .name = "work",
    .shortcut = "work",
    .search_url = "https://work.com/searchTerms",
    .suggest_url = "https://work.com/suggest",
};

void SetFieldIfNotEmpty(const std::string& field,
                        const char* value,
                        base::Value::Dict* dict) {
  if (value) {
    dict->Set(field, value);
  }
}

base::Value::Dict GeneratePolicyEntry(TestSearchAggregator test_case) {
  base::Value::Dict entry;
  SetFieldIfNotEmpty(SearchAggregatorPolicyHandler::kIconUrl,
                     test_case.icon_url, &entry);
  SetFieldIfNotEmpty(SearchAggregatorPolicyHandler::kName, test_case.name,
                     &entry);
  SetFieldIfNotEmpty(SearchAggregatorPolicyHandler::kSearchUrl,
                     test_case.search_url, &entry);
  SetFieldIfNotEmpty(SearchAggregatorPolicyHandler::kShortcut,
                     test_case.shortcut, &entry);
  SetFieldIfNotEmpty(SearchAggregatorPolicyHandler::kSuggestUrl,
                     test_case.suggest_url, &entry);
  return entry;
}

// Returns a matcher that accepts entries for the pref corresponding to the
// search aggregator policy. Field values are obtained from |test_case|.
testing::Matcher<const base::Value&> IsSearchAggregatorEntry(
    TestSearchAggregator test_case,
    bool featured) {
  std::string expected_keyword =
      base::StringPrintf("%s%s", (featured ? "@" : ""), test_case.shortcut);
  return AllOf(
      HasStringField(DefaultSearchManager::kShortName,
                     std::string(test_case.name)),
      HasStringField(DefaultSearchManager::kKeyword, expected_keyword),
      HasStringField(DefaultSearchManager::kURL,
                     std::string(test_case.search_url)),
      HasStringField(DefaultSearchManager::kSuggestionsURL,
                     std::string(test_case.suggest_url)),
      Conditional(
          test_case.icon_url,
          HasStringField(DefaultSearchManager::kFaviconURL,
                         test_case.icon_url ? std::string(test_case.icon_url)
                                            : std::string()),
          FieldNotSet(DefaultSearchManager::kFaviconURL)),
      HasIntegerField(DefaultSearchManager::kCreatedByPolicy,
                      static_cast<int>(
                          TemplateURLData::CreatedByPolicy::kSearchAggregator)),
      HasBooleanField(DefaultSearchManager::kEnforcedByPolicy, false),
      HasBooleanField(DefaultSearchManager::kFeaturedByPolicy, featured),
      HasIntegerField(DefaultSearchManager::kIsActive,
                      static_cast<int>(TemplateURLData::ActiveStatus::kTrue)),
      HasBooleanField(DefaultSearchManager::kSafeForAutoReplace, false),
      HasDoubleField(DefaultSearchManager::kDateCreated),
      HasDoubleField(DefaultSearchManager::kLastModified));
}

testing::Matcher<const base::Value&> IsNonFeaturedSearchAggregatorEntry(
    TestSearchAggregator test_case) {
  return IsSearchAggregatorEntry(test_case, /*featured=*/false);
}

testing::Matcher<const base::Value&> IsFeaturedSearchAggregatorEntry(
    TestSearchAggregator test_case) {
  return IsSearchAggregatorEntry(test_case, /*featured=*/true);
}

MATCHER_P(HasValidationError,
          expected_str,
          base::StringPrintf(
              "%s error message `%s` for `EnterpriseSearchAggregatorSettings`",
              negation ? "does not contain" : "contains",
              base::UTF16ToUTF8(expected_str).c_str())) {
  return arg->HasError(key::kEnterpriseSearchAggregatorSettings) &&
         arg->GetErrorMessages(key::kEnterpriseSearchAggregatorSettings)
                 .find(expected_str) != std::wstring::npos;
}

}  // namespace

TEST(SearchAggregatorPolicyHandlerTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  // Invalid format, will not fail validation because feature is disabled.
  policy::PolicyMap policies;
  policies.Set(key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);

  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_FALSE(errors.HasError(key::kEnterpriseSearchAggregatorSettings));
}

TEST(SearchAggregatorPolicyHandlerTest, PolicyNotSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.empty());

  PrefValueMap prefs;
  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
      &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_TRUE(providers->GetList().empty());
}

TEST(SearchAggregatorPolicyHandlerTest, Valid) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  base::Value::Dict policy_value =
      GeneratePolicyEntry(kValidTestSearchAggregator);
  policies.Set(key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.empty());

  PrefValueMap prefs;
  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
      &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(
      providers->GetList(),
      ElementsAre(
          IsNonFeaturedSearchAggregatorEntry(kValidTestSearchAggregator),
          IsFeaturedSearchAggregatorEntry(kValidTestSearchAggregator)));
}

TEST(SearchAggregatorPolicyHandlerTest, Valid_NoIcon) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  base::Value::Dict policy_value =
      GeneratePolicyEntry(kValidTestSearchAggregatorNoIcon);
  policies.Set(key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.empty());

  PrefValueMap prefs;
  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
      &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(
      providers->GetList(),
      ElementsAre(
          IsNonFeaturedSearchAggregatorEntry(kValidTestSearchAggregatorNoIcon),
          IsFeaturedSearchAggregatorEntry(kValidTestSearchAggregatorNoIcon)));
}

TEST(SearchAggregatorPolicyHandlerTest, InvalidFormat) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  policies.Set(key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);

  PolicyErrorMap errors;
  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError(key::kEnterpriseSearchAggregatorSettings));
}

TEST(SearchAggregatorPolicyHandlerTest, MissingRequiredField) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  for (auto it = std::begin(kTestSearchAggregatorMissingRequiredField);
       it != std::end(kTestSearchAggregatorMissingRequiredField); ++it) {
    policy::PolicyMap policies;
    policies.Set(key::kEnterpriseSearchAggregatorSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(GeneratePolicyEntry(*it)), nullptr);

    PolicyErrorMap errors;
    ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
    EXPECT_TRUE(errors.HasError(key::kEnterpriseSearchAggregatorSettings));
  }
}

TEST(SearchAggregatorPolicyHandlerTest, EmptyRequiredField) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  for (auto it = std::begin(kTestSearchAggregatorEmptyRequiredField);
       it != std::end(kTestSearchAggregatorEmptyRequiredField); ++it) {
    policy::PolicyMap policies;
    policies.Set(key::kEnterpriseSearchAggregatorSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(GeneratePolicyEntry(*it)), nullptr);

    PolicyErrorMap errors;
    ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
    EXPECT_THAT(&errors, HasValidationError(l10n_util::GetStringUTF16(
                             it->expected_error_msg_id)));
  }
}

TEST(SearchAggregatorPolicyHandlerTest, UnknownField) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  constexpr char kUnknownFieldName[] = "unknown_field";

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  base::Value::Dict entry = GeneratePolicyEntry(kValidTestSearchAggregator);
  entry.Set(kUnknownFieldName, true);
  policies.Set(key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(std::move(entry)),
               nullptr);

  // A warning is registered during policy validation, but valid fields are
  // still used for building a new template URL.
  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_FALSE(errors.empty());
}

TEST(SearchAggregatorPolicyHandlerTest, ShortcutWithSpace) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  for (auto it = std::begin(kTestSearchAggregatorShortcutWithSpaces);
       it != std::end(kTestSearchAggregatorShortcutWithSpaces); ++it) {
    policy::PolicyMap policies;
    policies.Set(key::kEnterpriseSearchAggregatorSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(GeneratePolicyEntry(*it)), nullptr);

    PolicyErrorMap errors;
    ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
    EXPECT_THAT(&errors,
                HasValidationError(l10n_util::GetStringFUTF16(
                    IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_CONTAINS_SPACE,
                    base::UTF8ToUTF16(it->shortcut))));
  }
}

TEST(SearchAggregatorPolicyHandlerTest, ShortcutStartsWithAt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  policies.Set(
      key::kEnterpriseSearchAggregatorSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(
          GeneratePolicyEntry(kTestSearchAggregatorShortcutStartsWithAt)),
      nullptr);

  PolicyErrorMap errors;
  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(&errors,
              HasValidationError(l10n_util::GetStringFUTF16(
                  IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_STARTS_WITH_AT,
                  base::UTF8ToUTF16(
                      kTestSearchAggregatorShortcutStartsWithAt.shortcut))));
}

TEST(SearchAggregatorPolicyHandlerTest, NonHttpsUrl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  struct NonHttpsUrlTestCase {
    TestSearchAggregator policy_value;
    const char* invalid_url;
  };

  auto kTestCases = std::to_array<NonHttpsUrlTestCase>({
      {
          .policy_value = kTestSearchAggregatorSearchUrlNonHttps,
          .invalid_url = kTestSearchAggregatorSearchUrlNonHttps.search_url,
      },
      {
          .policy_value = kTestSearchAggregatorSuggestUrlNonHttps,
          .invalid_url = kTestSearchAggregatorSuggestUrlNonHttps.suggest_url,
      },
      {
          .policy_value = kTestSearchAggregatorIconUrlNonHttps,
          .invalid_url = kTestSearchAggregatorIconUrlNonHttps.icon_url,
      },
  });

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  for (auto it = std::begin(kTestCases); it != std::end(kTestCases); ++it) {
    policy::PolicyMap policies;
    policies.Set(key::kEnterpriseSearchAggregatorSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(GeneratePolicyEntry(it->policy_value)), nullptr);

    PolicyErrorMap errors;
    ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
    EXPECT_THAT(&errors, HasValidationError(l10n_util::GetStringFUTF16(
                             IDS_POLICY_SITE_SEARCH_SETTINGS_URL_NOT_HTTPS,
                             base::UTF8ToUTF16(it->invalid_url))));
  }
}

TEST(SearchAggregatorPolicyHandlerTest, NoStringReplacementInSearchUrl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  policies.Set(key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(GeneratePolicyEntry(
                   kTestSearchAggregatorNoStringReplacementSearchUrl)),
               nullptr);

  PolicyErrorMap errors;
  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(
      &errors,
      HasValidationError(l10n_util::GetStringFUTF16(
          IDS_POLICY_SITE_SEARCH_SETTINGS_URL_DOESNT_SUPPORT_REPLACEMENT,
          base::UTF8ToUTF16(
              kTestSearchAggregatorNoStringReplacementSearchUrl.search_url))));
}

TEST(SearchAggregatorPolicyHandlerTest,
     ShortcutSameAsDSPKeyword_DSPEnabledNotSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  policies.Set(key::kDefaultSearchProviderKeyword,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(kValidTestSearchAggregator.shortcut), nullptr);
  policies.Set(
      key::kEnterpriseSearchAggregatorSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(GeneratePolicyEntry(kValidTestSearchAggregator)), nullptr);

  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.empty());
}

TEST(SearchAggregatorPolicyHandlerTest, ShortcutSameAsDSPKeyword_DSPDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  policies.Set(key::kDefaultSearchProviderEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policies.Set(key::kDefaultSearchProviderKeyword,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(kValidTestSearchAggregator.shortcut), nullptr);
  policies.Set(
      key::kEnterpriseSearchAggregatorSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(GeneratePolicyEntry(kValidTestSearchAggregator)), nullptr);

  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.empty());
}

TEST(SearchAggregatorPolicyHandlerTest, ShortcutSameAsDSPKeyword_DSPEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  SearchAggregatorPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  policies.Set(key::kDefaultSearchProviderEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policies.Set(key::kDefaultSearchProviderKeyword,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(kValidTestSearchAggregator.shortcut), nullptr);
  policies.Set(
      key::kEnterpriseSearchAggregatorSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(GeneratePolicyEntry(kValidTestSearchAggregator)), nullptr);

  PolicyErrorMap errors;
  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(&errors,
              HasValidationError(l10n_util::GetStringFUTF16(
                  IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_EQUALS_DSP_KEYWORD,
                  base::UTF8ToUTF16(kValidTestSearchAggregator.shortcut))));
}

}  // namespace policy
