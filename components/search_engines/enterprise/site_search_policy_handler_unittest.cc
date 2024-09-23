// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/search_engines/enterprise/site_search_policy_handler.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
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
#include "components/search_engines/enterprise/enterprise_site_search_manager.h"
#include "components/search_engines/template_url_data.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::AllOf;
using testing::ElementsAre;

namespace policy {

namespace {

// Represents field values for SiteSearchSettings policy, used for generating
// policy value entries. Fields set as nullptr will not be added to the
// entry dictionary.
struct TestProvider {
  const char* name;
  const char* shortcut;
  const char* url;
  bool featured_by_policy = false;
  const char* favicon;
};

// Used for tests that require a list of valid providers.
TestProvider kValidTestProviders[] = {
    {.name = "work name",
     .shortcut = "work",
     .url = "https://work.com/{searchTerms}",
     .featured_by_policy = false,
     .favicon = "https://work.com/favicon.ico"},
    {.name = "docs name",
     .shortcut = "docs",
     .url = "https://docs.com/{searchTerms}",
     .favicon = "https://docs.com/favicon.ico"},
};

// Used for tests that require providers with missing required fields. Missing
// fields for test are represented as nullptr.
TestProvider kMissingRequiredFieldsTestProviders[] = {
    {.name = nullptr,
     .shortcut = "missing_name",
     .url = "https://missing_name.com/{searchTerms}",
     .favicon = nullptr},
    {.name = "missing_shortcut name",
     .shortcut = nullptr,
     .url = "https://missing_shortcut.com/{searchTerms}",
     .favicon = nullptr},
    {.name = "missing_url name",
     .shortcut = "missing_url",
     .url = nullptr,
     .favicon = nullptr},
};

// Used for tests that require providers with empty required fields.
TestProvider kEmptyFieldTestProviders[] = {
    {.name = "",
     .shortcut = "empty_name",
     .url = "https://empty_name.com/{searchTerms}",
     .favicon = nullptr},
    {.name = "empty_shortcut name",
     .shortcut = "",
     .url = "https://empty_shortcut.com/{searchTerms}",
     .favicon = nullptr},
    {.name = "empty_url name",
     .shortcut = "empty_url",
     .url = "",
     .favicon = nullptr},
};

// Used for tests that require a provider with unknown field.
TestProvider kUnknownFieldTestProviders[] = {
    {.name = "work name",
     .shortcut = "work",
     .url = "https://work.com/{searchTerms}",
     .favicon = "https://work.com/favicon.ico"},
};

// Used for tests that require a list of providers with a duplicated shortcut,
// but at least one valid entry.
TestProvider kShortcutNotUniqueTestProviders[] = {
    {.name = "work name",
     .shortcut = "work",
     .url = "https://work.com/q={searchTerms}&x",
     .favicon = nullptr},
    {.name = "also work name",
     .shortcut = "work",
     .url = "https://work.com/q={searchTerms}&y",
     .favicon = nullptr},
    {.name = "docs name",
     .shortcut = "docs",
     .url = "https://docs.com/{searchTerms}",
     .favicon = "https://docs.com/favicon.ico"},
};

// Used for tests that require a list of providers with a duplicated shortcut
// and no valid entry.
TestProvider kNoUniqueShortcutTestProviders[] = {
    {.name = "work name",
     .shortcut = "work",
     .url = "https://work.com/q={searchTerms}&x",
     .favicon = nullptr},
    {.name = "also work name",
     .shortcut = "work",
     .url = "https://work.com/q={searchTerms}&y",
     .favicon = nullptr},
};

// Used for tests that require a provider shortcut containing a space
// and no valid entry.
TestProvider kShortcutWithSpacesTestProviders[] = {
    {.name = "work name 1",
     .shortcut = " shortcut",
     .url = "https://work1.com/q={searchTerms}&x",
     .favicon = nullptr},
    {.name = "work name 2",
     .shortcut = "shortcut ",
     .url = "https://work2.com/q={searchTerms}&x",
     .favicon = nullptr},
    {.name = "work name 3",
     .shortcut = "short cut",
     .url = "https://work3.com/q={searchTerms}&x",
     .favicon = nullptr},
};

// Used for tests that require a provider shortcut that stars with @.
TestProvider kShortcutStartsWithAtTestProviders[] = {
    {.name = "invalid",
     .shortcut = "@work",
     .url = "https://work.com/q={searchTerms}&x",
     .favicon = nullptr},
    {.name = "valid",
     .shortcut = "wo@rk",
     .url = "https://work.com/q={searchTerms}&y",
     .favicon = "https://work.com/favicon.ico"},
};

// Used for tests that require a provider with invalid non-empty URLs.
TestProvider kInvalidUrlTestProviders[] = {
    {.name = "invalid1 name",
     .shortcut = "invalid1",
     .url = "https://work.com/q=searchTerms",
     .favicon = nullptr},
    {.name = "invalid2 name",
     .shortcut = "invalid2",
     .url = "https://work.com/q=%s",
     .favicon = nullptr},
    {.name = "invalid3 name",
     .shortcut = "invalid3",
     .url = "https://work.com",
     .favicon = nullptr},
};

constexpr char kDSPKeyword[] = "dsp_keyword";

// Used for tests that require a provider with invalid non-empty URLs.
TestProvider kShortcutSameAsDSPKeywordTestProviders[] = {
    {.name = "same as keyword",
     .shortcut = kDSPKeyword,
     .url = "https://work.com/q={searchTerms}&x",
     .favicon = "https://work.com/favicon.ico"},
    {.name = "something else",
     .shortcut = "something_else",
     .url = "https://work.com/q={searchTerms}&y",
     .favicon = "https://work.com/favicon.ico"},
};

// Used for tests that require a provider with non-HTTPS URL.
TestProvider kNonHttpsUrlTestProviders[] = {
    {.name = "work name",
     .shortcut = "work",
     .url = "http://work.com/q={searchTerms}",
     .favicon = "http://work.com/favicon.ico"},
};

// Used for tests that require a list of featured providers.
TestProvider kTestProvidersWithFeaturedEntries[] = {
    {.name = "work name",
     .shortcut = "work",
     .url = "https://work.com/{searchTerms}",
     .featured_by_policy = true,
     .favicon = "https://work.com/favicon.ico"},
    {.name = "non-featured name",
     .shortcut = "non-featured",
     .url = "https://non-featured.com/{searchTerms}",
     .featured_by_policy = false,
     .favicon = "https://non-featured.com/favicon.ico"},
    {.name = "docs name",
     .shortcut = "docs",
     .url = "https://docs.com/{searchTerms}",
     .featured_by_policy = true,
     .favicon = "https://docs.com/favicon.ico"},
};

// Creates a simple list item for the site search policy.
base::Value::Dict GenerateSiteSearchPolicyEntry(const std::string& name,
                                                const std::string& shortcut,
                                                const std::string& url,
                                                bool featured_by_policy) {
  base::Value::Dict entry;
  entry.Set(SiteSearchPolicyHandler::kName, name);
  entry.Set(SiteSearchPolicyHandler::kShortcut, shortcut);
  entry.Set(SiteSearchPolicyHandler::kUrl, url);
  entry.Set(SiteSearchPolicyHandler::kFeatured, featured_by_policy);
  return entry;
}

base::Value::Dict GenerateSiteSearchPolicyEntry(TestProvider test_case) {
  base::Value::Dict entry;
  if (test_case.name) {
    entry.Set(SiteSearchPolicyHandler::kName, test_case.name);
  }
  if (test_case.shortcut) {
    entry.Set(SiteSearchPolicyHandler::kShortcut, test_case.shortcut);
  }
  if (test_case.url) {
    entry.Set(SiteSearchPolicyHandler::kUrl, test_case.url);
  }
  entry.Set(SiteSearchPolicyHandler::kFeatured, test_case.featured_by_policy);
  return entry;
}

// Accepts a dictionary that has a string field `field_name` with value
// `expected_value`.
MATCHER_P2(HasStringField,
           field_name,
           expected_value,
           base::StringPrintf("%s string field `%s` with value `%s`",
                              negation ? "does not contain" : "contains",
                              field_name,
                              expected_value.c_str())) {
  const std::string* dict_value = (arg).GetDict().FindString(field_name);
  return dict_value && *dict_value == expected_value;
}

// Accepts a dictionary that has a boolean field `field_name` with value
// `expected_value`.
MATCHER_P2(HasBooleanField,
           field_name,
           expected_value,
           base::StringPrintf("%s boolean field `%s` with value `%d`",
                              negation ? "does not contain" : "contains",
                              field_name,
                              expected_value)) {
  std::optional<bool> dict_value = (arg).GetDict().FindBool(field_name);
  return dict_value && *dict_value == expected_value;
}

// Accepts a dictionary that has a double field `field_name` with non-zero
// value.
MATCHER_P2(HasIntegerField,
           field_name,
           expected_value,
           base::StringPrintf("%s integer field `%s` with value `%d`",
                              negation ? "does not contain" : "contains",
                              field_name,
                              expected_value)) {
  std::optional<int> dict_value = (arg).GetDict().FindInt(field_name);
  return dict_value && *dict_value == expected_value;
}

// Accepts a dictionary that has a double field `field_name` with non-zero
// value.
MATCHER_P(HasDoubleField,
          field_name,
          base::StringPrintf("%s double field `%s` with non-zero value",
                             negation ? "does not contain" : "contains",
                             field_name)) {
  std::optional<double> dict_value = (arg).GetDict().FindDouble(field_name);
  return dict_value && *dict_value != 0.0;
}

// Returns a matcher that accepts entries for the pref corresponding to the
// site search policy. Field values are obtained from |test_case|.
testing::Matcher<const base::Value&> IsSiteSearchEntry(TestProvider test_case,
                                                       bool featured) {
  std::string expected_keyword =
      base::StringPrintf("%s%s", (featured ? "@" : ""), test_case.shortcut);
  return AllOf(
      HasStringField(DefaultSearchManager::kShortName,
                     std::string(test_case.name)),
      HasStringField(DefaultSearchManager::kKeyword, expected_keyword),
      HasStringField(DefaultSearchManager::kURL, std::string(test_case.url)),
      HasBooleanField(DefaultSearchManager::kFeaturedByPolicy, featured),
      HasIntegerField(
          DefaultSearchManager::kCreatedByPolicy,
          static_cast<int>(TemplateURLData::CreatedByPolicy::kSiteSearch)),
      HasBooleanField(DefaultSearchManager::kEnforcedByPolicy, false),
      HasIntegerField(DefaultSearchManager::kIsActive,
                      static_cast<int>(TemplateURLData::ActiveStatus::kTrue)),
      HasStringField(DefaultSearchManager::kFaviconURL,
                     std::string(test_case.favicon)),
      HasBooleanField(DefaultSearchManager::kSafeForAutoReplace, false),
      HasDoubleField(DefaultSearchManager::kDateCreated),
      HasDoubleField(DefaultSearchManager::kLastModified));
}

testing::Matcher<const base::Value&> IsNonFeaturedSiteSearchEntry(
    TestProvider test_case) {
  return IsSiteSearchEntry(test_case, /*featured=*/false);
}

testing::Matcher<const base::Value&> IsFeaturedSiteSearchEntry(
    TestProvider test_case) {
  return IsSiteSearchEntry(test_case, /*featured=*/true);
}

MATCHER_P(HasValidationError,
          expected_str,
          base::StringPrintf("%s error message `%s` for `SiteSearchSettings`",
                             negation ? "does not contain" : "contains",
                             base::UTF16ToUTF8(expected_str).c_str())) {
  return arg->HasError(key::kSiteSearchSettings) &&
         arg->GetErrorMessages(key::kSiteSearchSettings).find(expected_str) !=
             std::wstring::npos;
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
  policy_value.Append(GenerateSiteSearchPolicyEntry(kValidTestProviders[0]));
  policy_value.Append(GenerateSiteSearchPolicyEntry(kValidTestProviders[1]));

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
  policy_value.Append(GenerateSiteSearchPolicyEntry(kValidTestProviders[0]));
  policy_value.Append(GenerateSiteSearchPolicyEntry(kValidTestProviders[1]));

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
  EXPECT_THAT(
      providers->GetList(),
      ElementsAre(IsNonFeaturedSiteSearchEntry(kValidTestProviders[0]),
                  IsNonFeaturedSiteSearchEntry(kValidTestProviders[1])));
}

TEST(SiteSearchPolicyHandlerTest, InvalidFormat) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(false), nullptr);

  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError(key::kSiteSearchSettings));
}

TEST(SiteSearchPolicyHandlerTest, TooManySiteSearchEntries) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  // Policy value has one list entry over the max allowed.
  base::Value::List policy_value;
  for (int i = 0; i <= SiteSearchPolicyHandler::kMaxSiteSearchProviders; ++i) {
    policy_value.Append(GenerateSiteSearchPolicyEntry(
        base::StringPrintf("shortcut_%d", i), base::StringPrintf("name %d", i),
        base::StringPrintf("https://site_%d.com/q={searchTerms}", i),
        /*featured_by_policy=*/false));
  }

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(&errors,
              HasValidationError(l10n_util::GetStringFUTF16(
                  IDS_POLICY_SITE_SEARCH_SETTINGS_MAX_PROVIDERS_LIMIT_ERROR,
                  base::NumberToString16(
                      SiteSearchPolicyHandler::kMaxSiteSearchProviders))));
}

TEST(SiteSearchPolicyHandlerTest, TooManyFeaturedSiteSearchEntries) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  // Policy value has one featured list entry over the max allowed.
  base::Value::List policy_value;
  for (int i = 0; i <= SiteSearchPolicyHandler::kMaxFeaturedProviders; ++i) {
    policy_value.Append(GenerateSiteSearchPolicyEntry(
        base::StringPrintf("shortcut_%d", i), base::StringPrintf("name %d", i),
        base::StringPrintf("https://site_%d.com/q={searchTerms}", i),
        /*featured_by_policy=*/true));
  }

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(
      &errors,
      HasValidationError(l10n_util::GetStringFUTF16(
          IDS_POLICY_SITE_SEARCH_SETTINGS_MAX_FEATURED_PROVIDERS_LIMIT_ERROR,
          base::NumberToString16(
              SiteSearchPolicyHandler::kMaxFeaturedProviders))));
}

TEST(SiteSearchPolicyHandlerTest, MissingRequiredField) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  for (auto* it = std::begin(kMissingRequiredFieldsTestProviders);
       it != std::end(kMissingRequiredFieldsTestProviders); ++it) {
    policy::PolicyMap policies;
    PolicyErrorMap errors;
    PrefValueMap prefs;

    base::Value::List policy_value;
    policy_value.Append(GenerateSiteSearchPolicyEntry(*it));

    policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                 base::Value(std::move(policy_value)), nullptr);

    ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
    EXPECT_TRUE(errors.HasError(key::kSiteSearchSettings));
  }
}

TEST(SiteSearchPolicyHandlerTest, ShortcutNotUnique) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  for (auto* it = std::begin(kShortcutNotUniqueTestProviders);
       it != std::end(kShortcutNotUniqueTestProviders); ++it) {
    policy_value.Append(GenerateSiteSearchPolicyEntry(*it));
  }

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(&errors, HasValidationError(l10n_util::GetStringFUTF16(
                           IDS_POLICY_SITE_SEARCH_SETTINGS_DUPLICATED_SHORTCUT,
                           u"work")));

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(providers->GetList(), ElementsAre(IsNonFeaturedSiteSearchEntry(
                                        kShortcutNotUniqueTestProviders[2])));
}

TEST(SiteSearchPolicyHandlerTest, NoUniqueShortcut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  for (auto* it = std::begin(kNoUniqueShortcutTestProviders);
       it != std::end(kNoUniqueShortcutTestProviders); ++it) {
    policy_value.Append(GenerateSiteSearchPolicyEntry(*it));
  }

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(&errors, HasValidationError(l10n_util::GetStringFUTF16(
                           IDS_POLICY_SITE_SEARCH_SETTINGS_DUPLICATED_SHORTCUT,
                           u"work")));
}

TEST(SiteSearchPolicyHandlerTest, EmptyRequiredField) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  for (auto* it = std::begin(kEmptyFieldTestProviders);
       it != std::end(kEmptyFieldTestProviders); ++it) {
    policy_value.Append(GenerateSiteSearchPolicyEntry(*it));
  }

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(&errors, HasValidationError(l10n_util::GetStringUTF16(
                           IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_IS_EMPTY)));
  EXPECT_THAT(&errors, HasValidationError(l10n_util::GetStringUTF16(
                           IDS_POLICY_SITE_SEARCH_SETTINGS_NAME_IS_EMPTY)));
  EXPECT_THAT(&errors, HasValidationError(l10n_util::GetStringUTF16(
                           IDS_POLICY_SITE_SEARCH_SETTINGS_URL_IS_EMPTY)));
}

TEST(SiteSearchPolicyHandlerTest, UnknownField) {
  constexpr char kUnknownFieldName[] = "unknown_field";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::Dict entry =
      GenerateSiteSearchPolicyEntry(kUnknownFieldTestProviders[0]);
  entry.Set(kUnknownFieldName, true);
  base::Value::List policy_value;
  policy_value.Append(std::move(entry));

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  // A warning is registered during policy validation, but valid fields are
  // still used for building a new template URL.
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_FALSE(errors.empty());

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(
      providers->GetList(),
      ElementsAre(IsNonFeaturedSiteSearchEntry(kUnknownFieldTestProviders[0])));
}

TEST(SiteSearchPolicyHandlerTest, ShortcutWithSpace) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  for (auto* it = std::begin(kShortcutWithSpacesTestProviders);
       it != std::end(kShortcutWithSpacesTestProviders); ++it) {
    policy_value.Append(GenerateSiteSearchPolicyEntry(*it));
  }

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  for (auto* it = std::begin(kShortcutWithSpacesTestProviders);
       it != std::end(kShortcutWithSpacesTestProviders); ++it) {
    EXPECT_THAT(&errors,
                HasValidationError(l10n_util::GetStringFUTF16(
                    IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_CONTAINS_SPACE,
                    base::UTF8ToUTF16(it->shortcut))));
  }
}

TEST(SiteSearchPolicyHandlerTest, ShortcutStartsWithAt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  for (auto* it = std::begin(kShortcutStartsWithAtTestProviders);
       it != std::end(kShortcutStartsWithAtTestProviders); ++it) {
    policy_value.Append(GenerateSiteSearchPolicyEntry(*it));
  }

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(
      &errors,
      HasValidationError(l10n_util::GetStringFUTF16(
          IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_STARTS_WITH_AT,
          base::UTF8ToUTF16(kShortcutStartsWithAtTestProviders[0].shortcut))));

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(providers->GetList(),
              ElementsAre(IsNonFeaturedSiteSearchEntry(
                  kShortcutStartsWithAtTestProviders[1])));
}

TEST(SiteSearchPolicyHandlerTest, InvalidUrl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  for (auto* it = std::begin(kInvalidUrlTestProviders);
       it != std::end(kInvalidUrlTestProviders); ++it) {
    policy_value.Append(GenerateSiteSearchPolicyEntry(*it));
  }

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  for (auto* it = std::begin(kInvalidUrlTestProviders);
       it != std::end(kInvalidUrlTestProviders); ++it) {
    EXPECT_THAT(
        &errors,
        HasValidationError(l10n_util::GetStringFUTF16(
            IDS_POLICY_SITE_SEARCH_SETTINGS_URL_DOESNT_SUPPORT_REPLACEMENT,
            base::UTF8ToUTF16(it->url))));
  }
}

TEST(SiteSearchPolicyHandlerTest, ShortcutSameAsDSPKeyword_DSPEnabledNotSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  for (auto* it = std::begin(kShortcutSameAsDSPKeywordTestProviders);
       it != std::end(kShortcutSameAsDSPKeywordTestProviders); ++it) {
    policy_value.Append(GenerateSiteSearchPolicyEntry(*it));
  }

  policies.Set(key::kDefaultSearchProviderKeyword,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(kDSPKeyword), nullptr);
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
              ElementsAre(IsNonFeaturedSiteSearchEntry(
                              kShortcutSameAsDSPKeywordTestProviders[0]),
                          IsNonFeaturedSiteSearchEntry(
                              kShortcutSameAsDSPKeywordTestProviders[1])));
}

TEST(SiteSearchPolicyHandlerTest, ShortcutSameAsDSPKeyword_DSPDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  for (auto* it = std::begin(kShortcutSameAsDSPKeywordTestProviders);
       it != std::end(kShortcutSameAsDSPKeywordTestProviders); ++it) {
    policy_value.Append(GenerateSiteSearchPolicyEntry(*it));
  }

  policies.Set(key::kDefaultSearchProviderEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policies.Set(key::kDefaultSearchProviderKeyword,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(kDSPKeyword), nullptr);
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
              ElementsAre(IsNonFeaturedSiteSearchEntry(
                              kShortcutSameAsDSPKeywordTestProviders[0]),
                          IsNonFeaturedSiteSearchEntry(
                              kShortcutSameAsDSPKeywordTestProviders[1])));
}

TEST(SiteSearchPolicyHandlerTest, ShortcutSameAsDSPKeyword_DSPEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  for (auto* it = std::begin(kShortcutSameAsDSPKeywordTestProviders);
       it != std::end(kShortcutSameAsDSPKeywordTestProviders); ++it) {
    policy_value.Append(GenerateSiteSearchPolicyEntry(*it));
  }

  policies.Set(key::kDefaultSearchProviderEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policies.Set(key::kDefaultSearchProviderKeyword,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(kDSPKeyword), nullptr);
  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(&errors,
              HasValidationError(l10n_util::GetStringFUTF16(
                  IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_EQUALS_DSP_KEYWORD,
                  base::UTF8ToUTF16(
                      kShortcutSameAsDSPKeywordTestProviders[0].shortcut))));

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(providers->GetList(),
              ElementsAre(IsNonFeaturedSiteSearchEntry(
                  kShortcutSameAsDSPKeywordTestProviders[1])));
}

TEST(SiteSearchPolicyHandlerTest, NonHttpsUrl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  policy_value.Append(
      GenerateSiteSearchPolicyEntry(kNonHttpsUrlTestProviders[0]));

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(&errors,
              HasValidationError(l10n_util::GetStringFUTF16(
                  IDS_POLICY_SITE_SEARCH_SETTINGS_URL_NOT_HTTPS,
                  base::UTF8ToUTF16(kNonHttpsUrlTestProviders[0].url))));

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(
      providers->GetList(),
      ElementsAre(IsNonFeaturedSiteSearchEntry(kNonHttpsUrlTestProviders[0])));
}

TEST(SiteSearchPolicyHandlerTest, NoValidEntry) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(base::Value::List()), nullptr);

  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(&errors, HasValidationError(l10n_util::GetStringUTF16(
                           IDS_POLICY_SITE_SEARCH_SETTINGS_NO_VALID_PROVIDER)));
}

TEST(SiteSearchPolicyHandlerTest, FeaturedSiteSearchEntries) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  for (auto* it = std::begin(kTestProvidersWithFeaturedEntries);
       it != std::end(kTestProvidersWithFeaturedEntries); ++it) {
    policy_value.Append(GenerateSiteSearchPolicyEntry(*it));
  }

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
  EXPECT_THAT(
      providers->GetList(),
      ElementsAre(
          IsNonFeaturedSiteSearchEntry(kTestProvidersWithFeaturedEntries[0]),
          IsFeaturedSiteSearchEntry(kTestProvidersWithFeaturedEntries[0]),
          IsNonFeaturedSiteSearchEntry(kTestProvidersWithFeaturedEntries[1]),
          IsNonFeaturedSiteSearchEntry(kTestProvidersWithFeaturedEntries[2]),
          IsFeaturedSiteSearchEntry(kTestProvidersWithFeaturedEntries[2])));
}

}  // namespace policy
