// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/search_engines/enterprise/site_search_policy_handler.h"

#include <optional>

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
#include "components/search_engines/enterprise/enterprise_search_manager.h"
#include "components/search_engines/enterprise/field_validation_test_utils.h"
#include "components/search_engines/enterprise/search_aggregator_policy_handler.h"
#include "components/search_engines/template_url_data.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::AllOf;
using testing::ElementsAre;

namespace policy {

namespace {

// Represents field values for SiteSearchSettings policy, used for generating
// policy value entries.
struct TestProvider {
  std::optional<std::string> name;
  std::optional<std::string> shortcut;
  std::optional<std::string> url;
  bool featured_by_policy = false;
  std::optional<std::string> favicon;
  std::optional<bool> allow_user_override;
};

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

// Used for tests that require a list of valid providers.
TestProvider kValidTestProvidersWithAllowUserOverride[] = {
    {.name = "work name",
     .shortcut = "work",
     .url = "https://work.com/{searchTerms}",
     .favicon = "https://work.com/favicon.ico",
     .allow_user_override = true},
    {.name = "docs name",
     .shortcut = "docs",
     .url = "https://docs.com/{searchTerms}",
     .favicon = "https://docs.com/favicon.ico",
     .allow_user_override = false},
    {.name = "mail name",
     .shortcut = "mail",
     .url = "https://mail.com/{searchTerms}",
     .favicon = "https://mail.com/favicon.ico",
     .allow_user_override = std::nullopt},
};

// Used for tests that require providers with missing required fields.
TestProvider kMissingRequiredFieldsTestProviders[] = {
    {.shortcut = "missing_name",
     .url = "https://missing_name.com/{searchTerms}"},
    {.name = "missing_shortcut name",
     .url = "https://missing_shortcut.com/{searchTerms}"},
    {.name = "missing_url name", .shortcut = "missing_url"},
};

// Used for tests that require providers with empty required fields.
TestProvider kEmptyFieldTestProviders[] = {
    {.name = "",
     .shortcut = "empty_name",
     .url = "https://empty_name.com/{searchTerms}"},
    {.name = "empty_shortcut name",
     .shortcut = "",
     .url = "https://empty_shortcut.com/{searchTerms}"},
    {.name = "empty_url name", .shortcut = "empty_url", .url = ""},
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
     .url = "https://work.com/q={searchTerms}&x"},
    {.name = "also work name",
     .shortcut = "work",
     .url = "https://work.com/q={searchTerms}&y"},
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
     .url = "https://work.com/q={searchTerms}&x"},
    {.name = "also work name",
     .shortcut = "work",
     .url = "https://work.com/q={searchTerms}&y"},
};

// Used for tests that require a provider shortcut containing a space
// and no valid entry.
TestProvider kShortcutWithSpacesTestProviders[] = {
    {.name = "work name 1",
     .shortcut = " shortcut",
     .url = "https://work1.com/q={searchTerms}&x"},
    {.name = "work name 2",
     .shortcut = "shortcut ",
     .url = "https://work2.com/q={searchTerms}&x"},
    {.name = "work name 3",
     .shortcut = "short cut",
     .url = "https://work3.com/q={searchTerms}&x"},
};

// Used for tests that require a provider shortcut that stars with @.
TestProvider kShortcutStartsWithAtTestProviders[] = {
    {.name = "invalid",
     .shortcut = "@work",
     .url = "https://work.com/q={searchTerms}&x"},
    {.name = "valid",
     .shortcut = "wo@rk",
     .url = "https://work.com/q={searchTerms}&y",
     .favicon = "https://work.com/favicon.ico"},
};

// Used for tests that require a provider with invalid non-empty URLs.
TestProvider kInvalidUrlTestProviders[] = {
    {.name = "invalid1 name",
     .shortcut = "invalid1",
     .url = "https://work.com/q=searchTerms"},
    {.name = "invalid2 name",
     .shortcut = "invalid2",
     .url = "https://work.com/q=%s"},
    {.name = "invalid3 name",
     .shortcut = "invalid3",
     .url = "https://work.com"},
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

constexpr char kSiteSearchKeyword[] = "ss_keyword";

TestSearchAggregator kSearchAggregatorShortcutSameAsSiteSearch = {
    .name = "work name",
    .shortcut = kSiteSearchKeyword,
    .search_url = "https://work.com/q={searchTerms}&x",
    .suggest_url = "https://work.com/suggest",
    .icon_url = "https://work.com/favicon.ico"};

TestProvider kSiteSearchShortcutSameAsSearchAggregator = {
    .name = "same as keyword",
    .shortcut = kSiteSearchKeyword,
    .url = "https://work.com/q={searchTerms}&x",
    .favicon = "https://work.com/favicon.ico"};

bool kMismatchedSearchAggregatorSettingsType = true;

TestSearchAggregator kSearchAggregatorWithoutShortcut = {
    .name = "work name",
    .search_url = "https://work.com/q={searchTerms}&x",
    .suggest_url = "https://work.com/suggest",
    .icon_url = "https://work.com/favicon.ico"};

TestSearchAggregator kSearchAggregatorSettingWithShortcut = {
    .name = "work name",
    .shortcut = "search_aggregator_shortcut",
    .search_url = "https://work.com/q={searchTerms}&x",
    .suggest_url = "https://work.com/suggest",
    .icon_url = "https://work.com/favicon.ico"};

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
  if (test_case.name.has_value()) {
    entry.Set(SiteSearchPolicyHandler::kName, test_case.name.value());
  }
  if (test_case.shortcut.has_value()) {
    entry.Set(SiteSearchPolicyHandler::kShortcut, test_case.shortcut.value());
  }
  if (test_case.url.has_value()) {
    entry.Set(SiteSearchPolicyHandler::kUrl, test_case.url.value());
  }
  entry.Set(SiteSearchPolicyHandler::kFeatured, test_case.featured_by_policy);
  if (test_case.allow_user_override.has_value()) {
    entry.Set(SiteSearchPolicyHandler::kAllowUserOverride,
              test_case.allow_user_override.value());
  }
  return entry;
}

void SetFieldIfNotEmpty(const std::string& field,
                        const char* value,
                        base::Value::Dict* dict) {
  if (value) {
    dict->Set(field, value);
  }
}

base::Value::Dict GenerateSearchAggregatorPolicyEntry(
    TestSearchAggregator test_case) {
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
// site search policy. Field values are obtained from |test_case|.
testing::Matcher<const base::Value&> IsSiteSearchEntry(
    TestProvider test_case,
    bool featured,
    bool enforced_by_policy) {
  std::string expected_keyword = base::StringPrintf(
      "%s%s", (featured ? "@" : ""), test_case.shortcut.value());
  return AllOf(
      HasStringField(DefaultSearchManager::kShortName, test_case.name.value()),
      HasStringField(DefaultSearchManager::kKeyword, expected_keyword),
      HasStringField(DefaultSearchManager::kURL, test_case.url.value()),
      HasBooleanField(DefaultSearchManager::kFeaturedByPolicy, featured),
      HasIntegerField(
          DefaultSearchManager::kPolicyOrigin,
          static_cast<int>(TemplateURLData::PolicyOrigin::kSiteSearch)),
      HasBooleanField(DefaultSearchManager::kEnforcedByPolicy,
                      enforced_by_policy),
      HasIntegerField(DefaultSearchManager::kIsActive,
                      static_cast<int>(TemplateURLData::ActiveStatus::kTrue)),
      HasStringField(DefaultSearchManager::kFaviconURL,
                     test_case.favicon.value()),
      HasBooleanField(DefaultSearchManager::kSafeForAutoReplace, false),
      HasDoubleField(DefaultSearchManager::kDateCreated),
      HasDoubleField(DefaultSearchManager::kLastModified));
}

testing::Matcher<const base::Value&> IsNonFeaturedSiteSearchEntry(
    TestProvider test_case) {
  return IsSiteSearchEntry(test_case, /*featured=*/false,
                           /*enforced_by_policy=*/true);
}

testing::Matcher<const base::Value&> IsFeaturedSiteSearchEntry(
    TestProvider test_case) {
  return IsSiteSearchEntry(test_case, /*featured=*/true,
                           /*enforced_by_policy=*/true);
}

testing::Matcher<const base::Value&> IsOverridableNonFeaturedSiteSearchEntry(
    TestProvider test_case) {
  return IsSiteSearchEntry(test_case, /*featured=*/false,
                           /*enforced_by_policy=*/false);
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

TEST(SiteSearchPolicyHandlerTest, PolicyNotSet) {
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
      EnterpriseSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_TRUE(providers->GetList().empty());
}

TEST(SiteSearchPolicyHandlerTest, ValidSiteSearchEntries) {
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
      EnterpriseSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(
      providers->GetList(),
      ElementsAre(IsNonFeaturedSiteSearchEntry(kValidTestProviders[0]),
                  IsNonFeaturedSiteSearchEntry(kValidTestProviders[1])));
}

TEST(SiteSearchPolicyHandlerTest,
     ValidSiteSearchEntriesWithAllowUserOverride_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      omnibox::kEnableSiteSearchAllowUserOverridePolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  policy_value.Append(GenerateSiteSearchPolicyEntry(
      kValidTestProvidersWithAllowUserOverride[0]));
  policy_value.Append(GenerateSiteSearchPolicyEntry(
      kValidTestProvidersWithAllowUserOverride[1]));
  policy_value.Append(GenerateSiteSearchPolicyEntry(
      kValidTestProvidersWithAllowUserOverride[2]));

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError(key::kSiteSearchSettings));

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(providers->GetList(),
              ElementsAre(IsNonFeaturedSiteSearchEntry(
                              kValidTestProvidersWithAllowUserOverride[0]),
                          IsNonFeaturedSiteSearchEntry(
                              kValidTestProvidersWithAllowUserOverride[1]),
                          IsNonFeaturedSiteSearchEntry(
                              kValidTestProvidersWithAllowUserOverride[2])));
}

TEST(SiteSearchPolicyHandlerTest,
     ValidSiteSearchEntriesWithAllowUserOverride_FeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSiteSearchAllowUserOverridePolicy);

  SiteSearchPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  policy_value.Append(GenerateSiteSearchPolicyEntry(
      kValidTestProvidersWithAllowUserOverride[0]));
  policy_value.Append(GenerateSiteSearchPolicyEntry(
      kValidTestProvidersWithAllowUserOverride[1]));
  policy_value.Append(GenerateSiteSearchPolicyEntry(
      kValidTestProvidersWithAllowUserOverride[2]));

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  // TODO(crbug.com/417479042): Remove error expectation once policy YAML
  // includes new `allow_user_override` field.
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError(key::kSiteSearchSettings));

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(providers->GetList(),
              ElementsAre(IsOverridableNonFeaturedSiteSearchEntry(
                              kValidTestProvidersWithAllowUserOverride[0]),
                          IsNonFeaturedSiteSearchEntry(
                              kValidTestProvidersWithAllowUserOverride[1]),
                          IsNonFeaturedSiteSearchEntry(
                              kValidTestProvidersWithAllowUserOverride[2])));
}

TEST(SiteSearchPolicyHandlerTest, InvalidFormat) {
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
      EnterpriseSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(providers->GetList(), ElementsAre(IsNonFeaturedSiteSearchEntry(
                                        kShortcutNotUniqueTestProviders[2])));
}

TEST(SiteSearchPolicyHandlerTest, NoUniqueShortcut) {
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
                           IDS_SEARCH_POLICY_SETTINGS_SHORTCUT_IS_EMPTY)));
  EXPECT_THAT(&errors, HasValidationError(l10n_util::GetStringUTF16(
                           IDS_SEARCH_POLICY_SETTINGS_NAME_IS_EMPTY)));
  EXPECT_THAT(&errors, HasValidationError(l10n_util::GetStringUTF16(
                           IDS_SEARCH_POLICY_SETTINGS_URL_IS_EMPTY)));
}

TEST(SiteSearchPolicyHandlerTest, UnknownField) {
  constexpr char kUnknownFieldName[] = "unknown_field";

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
      EnterpriseSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(
      providers->GetList(),
      ElementsAre(IsNonFeaturedSiteSearchEntry(kUnknownFieldTestProviders[0])));
}

TEST(SiteSearchPolicyHandlerTest, ShortcutWithSpace) {
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
    EXPECT_THAT(&errors, HasValidationError(l10n_util::GetStringFUTF16(
                             IDS_SEARCH_POLICY_SETTINGS_SHORTCUT_CONTAINS_SPACE,
                             base::UTF8ToUTF16(it->shortcut.value()))));
  }
}

TEST(SiteSearchPolicyHandlerTest, ShortcutStartsWithAt) {
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
          IDS_SEARCH_POLICY_SETTINGS_SHORTCUT_STARTS_WITH_AT,
          base::UTF8ToUTF16(
              kShortcutStartsWithAtTestProviders[0].shortcut.value()))));

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(providers->GetList(),
              ElementsAre(IsNonFeaturedSiteSearchEntry(
                  kShortcutStartsWithAtTestProviders[1])));
}

TEST(SiteSearchPolicyHandlerTest, InvalidUrl) {
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
    EXPECT_THAT(&errors,
                HasValidationError(l10n_util::GetStringFUTF16(
                    IDS_SEARCH_POLICY_SETTINGS_URL_DOESNT_SUPPORT_REPLACEMENT,
                    base::UTF8ToUTF16(it->url.value()))));
  }
}

TEST(SiteSearchPolicyHandlerTest, ShortcutSameAsDSPKeyword_DSPEnabledNotSet) {
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
      EnterpriseSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(providers->GetList(),
              ElementsAre(IsNonFeaturedSiteSearchEntry(
                              kShortcutSameAsDSPKeywordTestProviders[0]),
                          IsNonFeaturedSiteSearchEntry(
                              kShortcutSameAsDSPKeywordTestProviders[1])));
}

TEST(SiteSearchPolicyHandlerTest, ShortcutSameAsDSPKeyword_DSPDisabled) {
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
      EnterpriseSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(providers->GetList(),
              ElementsAre(IsNonFeaturedSiteSearchEntry(
                              kShortcutSameAsDSPKeywordTestProviders[0]),
                          IsNonFeaturedSiteSearchEntry(
                              kShortcutSameAsDSPKeywordTestProviders[1])));
}

TEST(SiteSearchPolicyHandlerTest, ShortcutSameAsDSPKeyword_DSPEnabled) {
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
  EXPECT_THAT(
      &errors,
      HasValidationError(l10n_util::GetStringFUTF16(
          IDS_SEARCH_POLICY_SETTINGS_SHORTCUT_EQUALS_DSP_KEYWORD,
          base::UTF8ToUTF16(
              kShortcutSameAsDSPKeywordTestProviders[0].shortcut.value()))));

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(providers->GetList(),
              ElementsAre(IsNonFeaturedSiteSearchEntry(
                  kShortcutSameAsDSPKeywordTestProviders[1])));
}

TEST(SiteSearchPolicyHandlerTest, NonHttpsUrl) {
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
  EXPECT_THAT(
      &errors,
      HasValidationError(l10n_util::GetStringFUTF16(
          IDS_SEARCH_POLICY_SETTINGS_URL_NOT_HTTPS,
          base::UTF8ToUTF16(kNonHttpsUrlTestProviders[0].url.value()))));

  handler.ApplyPolicySettings(policies, &prefs);
  base::Value* providers = nullptr;
  ASSERT_TRUE(prefs.GetValue(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName, &providers));
  ASSERT_NE(providers, nullptr);
  ASSERT_TRUE(providers->is_list());
  EXPECT_THAT(
      providers->GetList(),
      ElementsAre(IsNonFeaturedSiteSearchEntry(kNonHttpsUrlTestProviders[0])));
}

TEST(SiteSearchPolicyHandlerTest, NoValidEntry) {
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
                           IDS_SEARCH_POLICY_SETTINGS_NO_VALID_PROVIDER)));
}

TEST(SiteSearchPolicyHandlerTest, FeaturedSiteSearchEntries) {
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
      EnterpriseSearchManager::kSiteSearchSettingsPrefName, &providers));
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

TEST(SiteSearchPolicyHandlerTest, ShortcutSameAsSearchAggregatorKeyword) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  policy_value.Append(
      GenerateSiteSearchPolicyEntry(kSiteSearchShortcutSameAsSearchAggregator));

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);
  policies.Set(key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(GenerateSearchAggregatorPolicyEntry(
                   kSearchAggregatorShortcutSameAsSiteSearch)),
               nullptr);

  SearchAggregatorPolicyHandler sap_handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  sap_handler.ApplyPolicySettings(policies, &prefs);
  ASSERT_TRUE(sap_handler.CheckPolicySettings(policies, &errors));

  SiteSearchPolicyHandler ssp_handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  ASSERT_FALSE(ssp_handler.CheckPolicySettings(policies, &errors));
  EXPECT_THAT(
      &errors,
      HasValidationError(l10n_util::GetStringFUTF16(
          IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_EQUALS_SEARCH_AGGREGATOR_KEYWORD,
          base::UTF8ToUTF16(
              kSiteSearchShortcutSameAsSearchAggregator.shortcut.value()))));
}

TEST(SiteSearchPolicyHandlerTest,
     ShortcutDifferentThanSearchAggregatorKeyword) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  policy_value.Append(GenerateSiteSearchPolicyEntry(kValidTestProviders[0]));
  policy_value.Append(GenerateSiteSearchPolicyEntry(kValidTestProviders[1]));

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);
  policies.Set(key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(GenerateSearchAggregatorPolicyEntry(
                   kSearchAggregatorSettingWithShortcut)),
               nullptr);

  SearchAggregatorPolicyHandler sap_handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  sap_handler.ApplyPolicySettings(policies, &prefs);
  ASSERT_TRUE(sap_handler.CheckPolicySettings(policies, &errors));

  SiteSearchPolicyHandler ssp_handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  ssp_handler.ApplyPolicySettings(policies, &prefs);
  ASSERT_TRUE(ssp_handler.CheckPolicySettings(policies, &errors));
}

TEST(SiteSearchPolicyHandlerTest, SearchAggregatorPolicyTypeMismatch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  policy_value.Append(
      GenerateSiteSearchPolicyEntry(kSiteSearchShortcutSameAsSearchAggregator));

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  policies.Set(key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(kMismatchedSearchAggregatorSettingsType), nullptr);

  SearchAggregatorPolicyHandler sap_handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  sap_handler.ApplyPolicySettings(policies, &prefs);
  ASSERT_FALSE(sap_handler.CheckPolicySettings(policies, &errors));

  SiteSearchPolicyHandler ssp_handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  ASSERT_TRUE(ssp_handler.CheckPolicySettings(policies, &errors));
}

TEST(SiteSearchPolicyHandlerTest, SearchAggregatorPolicyMissingShortcut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSearchAggregatorPolicy);

  policy::PolicyMap policies;
  PolicyErrorMap errors;
  PrefValueMap prefs;

  base::Value::List policy_value;
  policy_value.Append(
      GenerateSiteSearchPolicyEntry(kSiteSearchShortcutSameAsSearchAggregator));

  policies.Set(key::kSiteSearchSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(policy_value)), nullptr);

  policies.Set(key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(GenerateSearchAggregatorPolicyEntry(
                   kSearchAggregatorWithoutShortcut)),
               nullptr);

  SearchAggregatorPolicyHandler sap_handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  ASSERT_FALSE(sap_handler.CheckPolicySettings(policies, &errors));

  SiteSearchPolicyHandler ssp_handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  ASSERT_TRUE(ssp_handler.CheckPolicySettings(policies, &errors));
}

}  // namespace policy
