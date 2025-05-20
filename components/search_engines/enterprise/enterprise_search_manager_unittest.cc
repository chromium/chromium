// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise/enterprise_search_manager.h"

#include <optional>
#include <string>
#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pointee;
using testing::Property;

double kTimestamp = static_cast<double>(
    base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

base::Value::Dict GenerateSearchPrefEntry(const std::string& keyword,
                                          bool featured,
                                          bool enforced_by_policy) {
  base::Value::Dict entry;
  entry.Set(DefaultSearchManager::kShortName, keyword + "name");
  entry.Set(DefaultSearchManager::kKeyword, featured ? "@" + keyword : keyword);
  entry.Set(DefaultSearchManager::kURL,
            std::string("https://") + keyword + ".com/{searchTerms}");
  entry.Set(DefaultSearchManager::kEnforcedByPolicy, enforced_by_policy);
  entry.Set(DefaultSearchManager::kFeaturedByPolicy, featured);
  entry.Set(DefaultSearchManager::kFaviconURL,
            std::string("https://") + keyword + ".com/favicon.ico");
  entry.Set(DefaultSearchManager::kSafeForAutoReplace, false);
  entry.Set(DefaultSearchManager::kDateCreated, kTimestamp);
  entry.Set(DefaultSearchManager::kLastModified, kTimestamp);
  return entry;
}

base::Value::Dict GenerateSiteSearchPrefEntry(const std::string& keyword,
                                              bool enforced_by_policy = true) {
  base::Value::Dict entry =
      GenerateSearchPrefEntry(keyword, /*featured=*/false, enforced_by_policy);
  entry.Set(DefaultSearchManager::kPolicyOrigin,
            static_cast<int>(TemplateURLData::PolicyOrigin::kSiteSearch));
  return entry;
}

base::Value::Dict GenerateSearchAggregatorPrefEntry(const std::string& keyword,
                                                    bool featured) {
  base::Value::Dict entry =
      GenerateSearchPrefEntry(keyword, featured, /*enforced_by_policy=*/true);
  entry.Set(DefaultSearchManager::kPolicyOrigin,
            static_cast<int>(TemplateURLData::PolicyOrigin::kSearchAggregator));
  entry.Set(DefaultSearchManager::kSuggestionsURL,
            std::string("https://") + keyword + ".com/suggest");
  return entry;
}

class EnterpriseSearchManagerTestBase : public testing::Test {
 public:
  EnterpriseSearchManagerTestBase() = default;
  ~EnterpriseSearchManagerTestBase() override = default;

  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    EnterpriseSearchManager::RegisterProfilePrefs(pref_service_->registry());
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return pref_service_.get();
  }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
};

class EnterpriseSearchManagerTest : public EnterpriseSearchManagerTestBase {
 public:
  EnterpriseSearchManagerTest() = default;
  ~EnterpriseSearchManagerTest() override = default;

  void SetUp() override {
    EnterpriseSearchManagerTestBase::SetUp();
  }
};

TEST_F(EnterpriseSearchManagerTest, EmptyList) {
  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback, Run(IsEmpty())).Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      base::Value::List());
}

TEST_F(EnterpriseSearchManagerTest,
       SiteSearchOnly_AllowUserOverrideFeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      omnibox::kEnableSiteSearchAllowUserOverridePolicy);

  base::Value::List pref_value;
  pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  pref_value.Append(GenerateSiteSearchPrefEntry("docs"));
  pref_value.Append(
      GenerateSiteSearchPrefEntry("mail", /*enforced_by_policy=*/false));
  pref_value.Append(
      GenerateSiteSearchPrefEntry("calendar", /*enforced_by_policy=*/false));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  Pointee(Property(&TemplateURLData::keyword, u"work")),
                  Pointee(Property(&TemplateURLData::keyword, u"docs")),
                  Pointee(Property(&TemplateURLData::keyword, u"mail")),
                  Pointee(Property(&TemplateURLData::keyword, u"calendar")))))
      .Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(pref_value));

  const base::Value::List& final_overridden_keywords = pref_service()->GetList(
      EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName);
  EXPECT_THAT(final_overridden_keywords, IsEmpty());
}

TEST_F(EnterpriseSearchManagerTest, SiteSearchOnly_AllowUserOverrideFeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSiteSearchAllowUserOverridePolicy);

  base::Value::List pref_value;
  pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  pref_value.Append(GenerateSiteSearchPrefEntry("docs"));
  pref_value.Append(
      GenerateSiteSearchPrefEntry("mail", /*enforced_by_policy=*/false));
  pref_value.Append(
      GenerateSiteSearchPrefEntry("calendar", /*enforced_by_policy=*/false));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  Pointee(Property(&TemplateURLData::keyword, u"work")),
                  Pointee(Property(&TemplateURLData::keyword, u"docs")),
                  Pointee(Property(&TemplateURLData::keyword, u"mail")),
                  Pointee(Property(&TemplateURLData::keyword, u"calendar")))))
      .Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(pref_value));

  const base::Value::List& final_overridden_keywords = pref_service()->GetList(
      EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName);
  EXPECT_THAT(final_overridden_keywords, IsEmpty());
}

TEST_F(EnterpriseSearchManagerTest,
       SiteSearch_SetOverriddenKeyword_AllowUserOverrideFeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSiteSearchAllowUserOverridePolicy);

  base::Value::List pref_value;
  pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  pref_value.Append(GenerateSiteSearchPrefEntry("docs"));
  pref_value.Append(
      GenerateSiteSearchPrefEntry("mail", /*enforced_by_policy=*/false));
  pref_value.Append(
      GenerateSiteSearchPrefEntry("calendar", /*enforced_by_policy=*/false));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  Pointee(Property(&TemplateURLData::keyword, u"work")),
                  Pointee(Property(&TemplateURLData::keyword, u"docs")),
                  Pointee(Property(&TemplateURLData::keyword, u"mail")),
                  Pointee(Property(&TemplateURLData::keyword, u"calendar")))))
      .Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(pref_value));

  // Mark "mail" as overridden by user.
  manager.AddOverriddenKeyword("mail");

  const base::Value::List& overridden_keywords_pref = pref_service()->GetList(
      EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName);
  EXPECT_THAT(overridden_keywords_pref.size(), 1);
  EXPECT_TRUE(overridden_keywords_pref.contains("mail"));
}

TEST_F(
    EnterpriseSearchManagerTest,
    SiteSearch_ResetOverriddenKeywordWhenEnforced_AllowUserOverrideFeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSiteSearchAllowUserOverridePolicy);

  base::Value::List initial_pref_value;
  initial_pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  initial_pref_value.Append(GenerateSiteSearchPrefEntry("docs"));
  initial_pref_value.Append(
      GenerateSiteSearchPrefEntry("mail", /*enforced_by_policy=*/false));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(
      callback,
      Run(ElementsAre(Pointee(Property(&TemplateURLData::keyword, u"work")),
                      Pointee(Property(&TemplateURLData::keyword, u"docs")),
                      Pointee(Property(&TemplateURLData::keyword, u"mail")))))
      .Times(1);
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  Pointee(Property(&TemplateURLData::keyword, u"work")),
                  Pointee(Property(&TemplateURLData::keyword, u"docs")),
                  Pointee(Property(&TemplateURLData::keyword, u"mail")),
                  Pointee(Property(&TemplateURLData::keyword, u"calendar")))))
      .Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(initial_pref_value));

  // Mark "mail" as overridden by user.
  manager.AddOverriddenKeyword("mail");
  const base::Value::List& overridden_keywords_pref = pref_service()->GetList(
      EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName);
  EXPECT_TRUE(overridden_keywords_pref.contains("mail"));

  // Update policy to make "mail" enforced and add "calendar" as enforced.
  base::Value::List updated_pref_value;
  updated_pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  updated_pref_value.Append(GenerateSiteSearchPrefEntry("docs"));
  updated_pref_value.Append(
      GenerateSiteSearchPrefEntry("mail", /*enforced_by_policy=*/true));
  updated_pref_value.Append(
      GenerateSiteSearchPrefEntry("calendar", /*enforced_by_policy=*/true));
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(updated_pref_value));

  EXPECT_THAT(overridden_keywords_pref, IsEmpty());
}

TEST_F(EnterpriseSearchManagerTest,
       SiteSearch_RemoveKeywordWhenNotInPolicy_AllowUserOverrideFeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSiteSearchAllowUserOverridePolicy);

  base::Value::List initial_pref_value;
  initial_pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  initial_pref_value.Append(GenerateSiteSearchPrefEntry("docs"));
  initial_pref_value.Append(
      GenerateSiteSearchPrefEntry("mail", /*enforced_by_policy=*/false));
  initial_pref_value.Append(
      GenerateSiteSearchPrefEntry("calendar", /*enforced_by_policy=*/false));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  Pointee(Property(&TemplateURLData::keyword, u"work")),
                  Pointee(Property(&TemplateURLData::keyword, u"docs")),
                  Pointee(Property(&TemplateURLData::keyword, u"mail")),
                  Pointee(Property(&TemplateURLData::keyword, u"calendar")))))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(ElementsAre(Pointee(Property(&TemplateURLData::keyword, u"work")),
                      Pointee(Property(&TemplateURLData::keyword, u"docs")))))
      .Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(initial_pref_value));

  const base::Value::List& overridden_keywords_pref = pref_service()->GetList(
      EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName);
  EXPECT_THAT(overridden_keywords_pref, IsEmpty());

  // Update policy to remove "mail" and "calendar".
  base::Value::List updated_pref_value;
  updated_pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  updated_pref_value.Append(GenerateSiteSearchPrefEntry("docs"));
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(updated_pref_value));

  EXPECT_THAT(overridden_keywords_pref, IsEmpty());
}

TEST_F(EnterpriseSearchManagerTest,
       SearchAggregatorsOnly_AllowUserOverrideFeatureOff) {
  base::Value::List pref_value;
  pref_value.Append(
      GenerateSearchAggregatorPrefEntry("aggregator", /*featured=*/true));
  pref_value.Append(
      GenerateSearchAggregatorPrefEntry("aggregator", /*featured=*/false));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  Pointee(Property(&TemplateURLData::keyword, u"@aggregator")),
                  Pointee(Property(&TemplateURLData::keyword, u"aggregator")))))
      .Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
      std::move(pref_value));

  const base::Value::List& final_overridden_keywords = pref_service()->GetList(
      EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName);
  EXPECT_THAT(final_overridden_keywords, IsEmpty());
}

TEST_F(EnterpriseSearchManagerTest,
       SearchAggregatorsOnly_AllowUserOverrideFeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kEnableSiteSearchAllowUserOverridePolicy);

  base::Value::List pref_value;
  pref_value.Append(
      GenerateSearchAggregatorPrefEntry("aggregator", /*featured=*/true));
  pref_value.Append(
      GenerateSearchAggregatorPrefEntry("aggregator", /*featured=*/false));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  Pointee(Property(&TemplateURLData::keyword, u"@aggregator")),
                  Pointee(Property(&TemplateURLData::keyword, u"aggregator")))))
      .Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
      std::move(pref_value));

  const base::Value::List& final_overridden_keywords = pref_service()->GetList(
      EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName);
  EXPECT_THAT(final_overridden_keywords, IsEmpty());
}

TEST_F(EnterpriseSearchManagerTest,
       SearchAggregatorsOnlyWithRequireShortcutTrue) {
  base::Value::List pref_value;
  pref_value.Append(
      GenerateSearchAggregatorPrefEntry("aggregator", /*featured=*/true));
  pref_value.Append(
      GenerateSearchAggregatorPrefEntry("aggregator", /*featured=*/false));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  Pointee(Property(&TemplateURLData::keyword, u"@aggregator")),
                  Pointee(Property(&TemplateURLData::keyword, u"aggregator")))))
      .Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
      std::move(pref_value));
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::
          kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName,
      base::Value(true));
}

TEST_F(EnterpriseSearchManagerTest, SiteSearchAndSearchAggregators) {
  base::Value::List site_search_pref_value;
  site_search_pref_value.Append(GenerateSiteSearchPrefEntry("work"));

  base::Value::List aggregator_pref_value;
  aggregator_pref_value.Append(
      GenerateSearchAggregatorPrefEntry("aggregator", /*featured=*/true));
  aggregator_pref_value.Append(
      GenerateSearchAggregatorPrefEntry("aggregator", /*featured=*/false));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(
      callback,
      Run(ElementsAre(Pointee(Property(&TemplateURLData::keyword, u"work")))));
  EXPECT_CALL(
      callback,
      Run(ElementsAre(
          Pointee(Property(&TemplateURLData::keyword, u"work")),
          Pointee(Property(&TemplateURLData::keyword, u"@aggregator")),
          Pointee(Property(&TemplateURLData::keyword, u"aggregator")))));

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(site_search_pref_value));

  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
      std::move(aggregator_pref_value));
}

TEST_F(EnterpriseSearchManagerTest, SiteSearch_NotCreatedByPolicy) {
  base::Value::List pref_value;
  pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  pref_value.Append(GenerateSiteSearchPrefEntry("docs"));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback, Run(_)).Times(0);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetUserPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(pref_value));
}

// Configuration for search aggregator value injection.
enum class PolicyLoadingStatus {
  kPolicyDisabled,
  kPolicyEnabledNoEngines,
  kPolicyEnabledWithEngines,
};

enum class MockSettingStatus {
  kDisabled,
  kEnabledInvalid,
  kEnabledValid,
};

enum class ExpectedResult {
  kNothingLoaded,
  kEmptyListLoaded,
  kPolicyListLoaded,
  kMockListLoaded,
};

struct ProviderInjectionTestCase {
  PolicyLoadingStatus policy_loading_status;
  MockSettingStatus mock_setting_status;
  ExpectedResult expected_result;
} kProviderInjectionTestCases[] = {
    {
        .policy_loading_status = PolicyLoadingStatus::kPolicyDisabled,
        .mock_setting_status = MockSettingStatus::kDisabled,
        .expected_result = ExpectedResult::kNothingLoaded,
    },
    {
        .policy_loading_status = PolicyLoadingStatus::kPolicyDisabled,
        .mock_setting_status = MockSettingStatus::kEnabledInvalid,
        .expected_result = ExpectedResult::kNothingLoaded,
    },
    {
        .policy_loading_status = PolicyLoadingStatus::kPolicyDisabled,
        .mock_setting_status = MockSettingStatus::kEnabledValid,
        .expected_result = ExpectedResult::kMockListLoaded,
    },
    {
        .policy_loading_status = PolicyLoadingStatus::kPolicyEnabledNoEngines,
        .mock_setting_status = MockSettingStatus::kDisabled,
        .expected_result = ExpectedResult::kEmptyListLoaded,
    },
    {
        .policy_loading_status = PolicyLoadingStatus::kPolicyEnabledNoEngines,
        .mock_setting_status = MockSettingStatus::kEnabledInvalid,
        .expected_result = ExpectedResult::kEmptyListLoaded,
    },
    {
        .policy_loading_status = PolicyLoadingStatus::kPolicyEnabledNoEngines,
        .mock_setting_status = MockSettingStatus::kEnabledValid,
        .expected_result = ExpectedResult::kMockListLoaded,
    },
    {
        .policy_loading_status = PolicyLoadingStatus::kPolicyEnabledWithEngines,
        .mock_setting_status = MockSettingStatus::kDisabled,
        .expected_result = ExpectedResult::kPolicyListLoaded,
    },
    {
        .policy_loading_status = PolicyLoadingStatus::kPolicyEnabledWithEngines,
        .mock_setting_status = MockSettingStatus::kEnabledInvalid,
        .expected_result = ExpectedResult::kPolicyListLoaded,
    },
    {
        .policy_loading_status = PolicyLoadingStatus::kPolicyEnabledWithEngines,
        .mock_setting_status = MockSettingStatus::kEnabledValid,
        .expected_result = ExpectedResult::kMockListLoaded,
    },
};

class EnterpriseSearchManagerProviderInjectionTest
    : public EnterpriseSearchManagerTestBase,
      public testing::WithParamInterface<ProviderInjectionTestCase> {
 public:
  EnterpriseSearchManagerProviderInjectionTest() = default;
  ~EnterpriseSearchManagerProviderInjectionTest() override = default;

  void InitScopedConfig(bool enabled,
                        const std::string& name,
                        const std::string& shortcut,
                        const std::string& search_url,
                        const std::string& suggest_url,
                        const std::string& icon_url,
                        bool require_shortcut,
                        int min_query_length) {
    scoped_config_.Get().enabled = enabled;
    scoped_config_.Get().name = name;
    scoped_config_.Get().shortcut = shortcut;
    scoped_config_.Get().search_url = search_url;
    scoped_config_.Get().suggest_url = suggest_url;
    scoped_config_.Get().icon_url = icon_url;
    scoped_config_.Get().require_shortcut = require_shortcut;
    scoped_config_.Get().min_query_length = min_query_length;
  }

  void InitScopedConfig(bool enabled, bool require_shortcut) {
    scoped_config_.Get().enabled = enabled;
    scoped_config_.Get().require_shortcut = require_shortcut;
  }

  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::SearchAggregatorProvider>
      scoped_config_;
};

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseSearchManagerProviderInjectionTest,
                         testing::ValuesIn(kProviderInjectionTestCases));

TEST_P(EnterpriseSearchManagerProviderInjectionTest, Verify) {
  ProviderInjectionTestCase test_case = GetParam();

  // Configure policy for test case.
  base::test::ScopedFeatureList scoped_feature_list;
  if (test_case.policy_loading_status == PolicyLoadingStatus::kPolicyDisabled) {
    scoped_feature_list.InitAndDisableFeature(
        omnibox::kEnableSearchAggregatorPolicy);
  } else {
    scoped_feature_list.InitAndEnableFeature(
        omnibox::kEnableSearchAggregatorPolicy);
    base::Value::List pref_value;
    if (test_case.policy_loading_status ==
        PolicyLoadingStatus::kPolicyEnabledWithEngines) {
      pref_value.Append(
          GenerateSearchAggregatorPrefEntry("from_policy", /*featured=*/true));
      pref_value.Append(
          GenerateSearchAggregatorPrefEntry("from_policy", /*featured=*/false));
    }
    pref_service()->SetManagedPref(
        EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
        std::move(pref_value));
  }

  // Configure mock settings for test case.
  if (test_case.mock_setting_status == MockSettingStatus::kDisabled) {
    InitScopedConfig(
        /*enabled=*/false,
        /*require_shortcut=*/true);

    EXPECT_FALSE(scoped_config_.Get().enabled);
    EXPECT_FALSE(scoped_config_.Get().AreMockEnginesValid());
    EXPECT_TRUE(scoped_config_.Get().require_shortcut);
  } else {
    // Use empty shortcut for invalid mock engine.
    InitScopedConfig(
        /*enabled=*/true,
        /*name=*/"Mocked",
        /*shortcut=*/test_case.mock_setting_status ==
                MockSettingStatus::kEnabledValid
            ? "mocked"
            : "",
        /*search_url=*/"https://www.mocked.com/q={searchTerms}",
        /*suggest_url=*/"https://www.mocked.com/ac",
        /*icon_url=*/"https://www.mocked.com/favicon.ico",
        /*require_shortcut=*/true,
        /*min_query_length=*/4);

    EXPECT_TRUE(scoped_config_.Get().enabled);
    EXPECT_EQ(
        scoped_config_.Get().AreMockEnginesValid(),
        test_case.mock_setting_status == MockSettingStatus::kEnabledValid);
    EXPECT_TRUE(scoped_config_.Get().require_shortcut);
  }

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  if (test_case.expected_result == ExpectedResult::kNothingLoaded) {
    EXPECT_CALL(callback, Run(_)).Times(0);
  } else if (test_case.expected_result == ExpectedResult::kEmptyListLoaded) {
    EXPECT_CALL(callback, Run(IsEmpty())).Times(1);
  } else {
    EXPECT_CALL(callback,
                Run(ElementsAre(
                    Pointee(Property(&TemplateURLData::keyword,
                                     test_case.expected_result ==
                                             ExpectedResult::kPolicyListLoaded
                                         ? u"@from_policy"
                                         : u"@mocked")),
                    Pointee(Property(&TemplateURLData::keyword,
                                     test_case.expected_result ==
                                             ExpectedResult::kPolicyListLoaded
                                         ? u"from_policy"
                                         : u"mocked")))))
        .Times(1);
  }

  EnterpriseSearchManager manager(pref_service(), callback.Get());
}

struct RequireShortcutTestCase {
  std::optional<bool> policy_require_shortcut;
  std::optional<bool> mock_require_shortcut;
  bool expected_result;
} kRequireShortcutTestCases[] = {
    {
        .policy_require_shortcut = std::nullopt,
        .mock_require_shortcut = std::nullopt,
        .expected_result = false,
    },
    {
        .policy_require_shortcut = std::nullopt,
        .mock_require_shortcut = false,
        .expected_result = false,
    },
    {
        .policy_require_shortcut = std::nullopt,
        .mock_require_shortcut = true,
        .expected_result = true,
    },
    {
        .policy_require_shortcut = false,
        .mock_require_shortcut = std::nullopt,
        .expected_result = false,
    },
    {
        .policy_require_shortcut = false,
        .mock_require_shortcut = false,
        .expected_result = false,
    },
    {
        .policy_require_shortcut = false,
        .mock_require_shortcut = true,
        .expected_result = true,
    },
    {
        .policy_require_shortcut = true,
        .mock_require_shortcut = std::nullopt,
        .expected_result = true,
    },
    {
        .policy_require_shortcut = true,
        .mock_require_shortcut = false,
        .expected_result = false,
    },
    {
        .policy_require_shortcut = true,
        .mock_require_shortcut = true,
        .expected_result = true,
    },
};

class EnterpriseSearchManagerRequireShortcutTest
    : public EnterpriseSearchManagerTestBase,
      public testing::WithParamInterface<RequireShortcutTestCase> {
 public:
  EnterpriseSearchManagerRequireShortcutTest() = default;
  ~EnterpriseSearchManagerRequireShortcutTest() override = default;

  void SetUp() override {
    EnterpriseSearchManagerTestBase::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        omnibox::kEnableSearchAggregatorPolicy);
  }

  void InitScopedConfig(bool enabled,
                        const std::string& name,
                        const std::string& shortcut,
                        const std::string& search_url,
                        const std::string& suggest_url,
                        const std::string& icon_url,
                        bool require_shortcut) {
    scoped_config_.Get().enabled = enabled;
    scoped_config_.Get().name = name;
    scoped_config_.Get().shortcut = shortcut;
    scoped_config_.Get().search_url = search_url;
    scoped_config_.Get().suggest_url = suggest_url;
    scoped_config_.Get().icon_url = icon_url;
    scoped_config_.Get().require_shortcut = require_shortcut;
  }

  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::SearchAggregatorProvider>
      scoped_config_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseSearchManagerRequireShortcutTest,
                         testing::ValuesIn(kRequireShortcutTestCases));

TEST_P(EnterpriseSearchManagerRequireShortcutTest,
       SearchAggregatorRequiresShortcut) {
  RequireShortcutTestCase test_case = GetParam();

  // Configure policy for test case.
  if (test_case.policy_require_shortcut.has_value()) {
    base::Value::List pref_value;
    pref_value.Append(
        GenerateSearchAggregatorPrefEntry("from_policy", /*featured=*/true));
    pref_value.Append(
        GenerateSearchAggregatorPrefEntry("from_policy", /*featured=*/false));
    pref_service()->SetManagedPref(
        EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
        std::move(pref_value));
    pref_service()->SetManagedPref(
        EnterpriseSearchManager::
            kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName,
        base::Value(test_case.policy_require_shortcut.value()));
  }

  // Configure mock settings for test case.
  if (test_case.mock_require_shortcut.has_value()) {
    InitScopedConfig(
        /*enabled=*/true,
        /*name=*/"Mocked",
        /*shortcut=*/"mocked",
        /*search_url=*/"https://www.mocked.com/q={searchTerms}",
        /*suggest_url=*/"https://www.mocked.com/ac",
        /*icon_url=*/"https://www.mocked.com/favicon.ico",
        /*require_shortcut=*/test_case.mock_require_shortcut.value());
  }

  // Initialize `EnterpriseSearchManager` based on policy and mock settings.
  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EnterpriseSearchManager manager(pref_service(), callback.Get());

  // Verify preference values for test case.
  const PrefService::Preference* pref = pref_service()->FindPreference(
      EnterpriseSearchManager::
          kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName);
  EXPECT_TRUE(pref);
  EXPECT_EQ(pref->IsManaged(), test_case.policy_require_shortcut.has_value());
  EXPECT_EQ(pref->GetValue()->GetBool(),
            test_case.policy_require_shortcut.value_or(false));

  // Verify `SearchAggregatorRequiresShortcut()` for test case.
  EXPECT_EQ(manager.GetRequireShortcutValue(), test_case.expected_result);
}

// Test `SearchAggregatorRequiresShortcut()`, verifying that mock setting
// `require_shortcut` field is ignored if mock setting does not have a valid
// search engine defined.
TEST_F(EnterpriseSearchManagerRequireShortcutTest,
       SearchAggregatorRequiresShortcutInvalidMockSetting) {
  // Configure invalid mock settings.
  InitScopedConfig(
      /*enabled=*/false,
      /*name=*/"Mocked",
      /*shortcut=*/"mocked",
      /*search_url=*/"https://www.mocked.com/q={searchTerms}",
      /*suggest_url=*/"https://www.mocked.com/ac",
      /*icon_url=*/"https://www.mocked.com/favicon.ico",
      /*require_shortcut=*/true);

  // Initialize `EnterpriseSearchManager` based on policy and mock settings.
  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EnterpriseSearchManager manager(pref_service(), callback.Get());

  // Verify preference values for test case.
  const PrefService::Preference* pref = pref_service()->FindPreference(
      EnterpriseSearchManager::
          kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName);
  EXPECT_TRUE(pref);
  EXPECT_FALSE(pref->IsManaged());
  EXPECT_FALSE(pref->GetValue()->GetBool());

  // Verify `SearchAggregatorRequiresShortcut()` is false/default.
  EXPECT_EQ(manager.GetRequireShortcutValue(), false);
}
