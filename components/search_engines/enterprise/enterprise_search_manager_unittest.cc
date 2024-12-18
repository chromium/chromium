// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise/enterprise_search_manager.h"

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
                                          bool featured) {
  base::Value::Dict entry;
  entry.Set(DefaultSearchManager::kShortName, keyword + "name");
  entry.Set(DefaultSearchManager::kKeyword, featured ? "@" + keyword : keyword);
  entry.Set(DefaultSearchManager::kURL,
            std::string("https://") + keyword + ".com/{searchTerms}");
  entry.Set(DefaultSearchManager::kEnforcedByPolicy, false);
  entry.Set(DefaultSearchManager::kFeaturedByPolicy, featured);
  entry.Set(DefaultSearchManager::kFaviconURL,
            std::string("https://") + keyword + ".com/favicon.ico");
  entry.Set(DefaultSearchManager::kSafeForAutoReplace, false);
  entry.Set(DefaultSearchManager::kDateCreated, kTimestamp);
  entry.Set(DefaultSearchManager::kLastModified, kTimestamp);
  return entry;
}

base::Value::Dict GenerateSiteSearchPrefEntry(const std::string& keyword) {
  base::Value::Dict entry =
      GenerateSearchPrefEntry(keyword, /*featured=*/false);
  entry.Set(DefaultSearchManager::kPolicyOrigin,
            static_cast<int>(TemplateURLData::PolicyOrigin::kSiteSearch));
  return entry;
}

base::Value::Dict GenerateSearchAggregatorPrefEntry(const std::string& keyword,
                                                    bool featured) {
  base::Value::Dict entry = GenerateSearchPrefEntry(keyword, featured);
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

    scoped_feature_list_.InitAndEnableFeature(
        omnibox::kEnableSearchAggregatorPolicy);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

TEST_F(EnterpriseSearchManagerTest, SiteSearchOnly) {
  base::Value::List pref_value;
  pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  pref_value.Append(GenerateSiteSearchPrefEntry("docs"));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(
      callback,
      Run(ElementsAre(Pointee(Property(&TemplateURLData::keyword, u"work")),
                      Pointee(Property(&TemplateURLData::keyword, u"docs")))))
      .Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(pref_value));
}

TEST_F(EnterpriseSearchManagerTest, SearchAggregatorsOnly) {
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
        .expected_result = ExpectedResult::kPolicyListLoaded,
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
                        bool trigger_omnibox_blending) {
    scoped_config_.Get().Init(enabled, name, shortcut, search_url, suggest_url,
                              icon_url, trigger_omnibox_blending);
  }

  void InitScopedConfig(bool enabled, bool trigger_omnibox_blending) {
    scoped_config_.Get().Init(enabled, trigger_omnibox_blending);
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
        /*trigger_omnibox_blending=*/true);

    ASSERT_FALSE(scoped_config_.Get().enabled());
    ASSERT_FALSE(scoped_config_.Get().valid_search_engine());
    ASSERT_FALSE(scoped_config_.Get().trigger_omnibox_blending());
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
        /*trigger_omnibox_blending=*/true);

    ASSERT_TRUE(scoped_config_.Get().enabled());
    ASSERT_EQ(
        scoped_config_.Get().valid_search_engine(),
        test_case.mock_setting_status == MockSettingStatus::kEnabledValid);
    ASSERT_TRUE(scoped_config_.Get().trigger_omnibox_blending());
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
