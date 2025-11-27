// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_manager.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/preferences/tracked/pref_hash_filter.h"
#include "template_url_prepopulate_data_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#include "services/preferences/tracked/features.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

void SetOverrides(sync_preferences::TestingPrefServiceSyncable* prefs,
                  bool update) {
  auto overrides = base::Value::List();

  // Lambda facilitating insertion of TemplateURL definitions, ensuring that all
  // mandatory fields are present.
  auto add_definition = [&overrides](TemplateURLID id,
                                     std::string name_and_keyword,
                                     std::string base_url) {
    auto alternate_urls = base::Value::List();
    alternate_urls.Append(base_url + "/alternate?q={searchTerms}");

    overrides.Append(
        base::Value::Dict()
            .Set("name", name_and_keyword)
            .Set("id", (int)id)
            .Set("keyword", name_and_keyword)
            .Set("search_url", base_url + "/search?q={searchTerms}")
            .Set("suggest_url", base_url + "/suggest?q={searchTerms}")
            .Set("favicon_url", base_url + "/favicon.ico")
            .Set("encoding", "UTF-8")
            .Set("alternate_urls", std::move(alternate_urls)));
  };

  add_definition(100, update ? "new_foo" : "foo", "https://atlas.com");
  add_definition(101, update ? "new_bar" : "bar", "https://brave.com");
  add_definition(102, update ? "new_bar" : "bar", "https://conduit.com");
  add_definition(103, "at.yahoo.com", "https://at.search.yahoo.com");
  add_definition(104, "emea.yahoo.com", "https://emea.search.yahoo.com");

  prefs->SetUserPref(prefs::kSearchProviderOverridesVersion, base::Value(1));
  prefs->SetUserPref(prefs::kSearchProviderOverrides, std::move(overrides));
}

void SetPolicy(sync_preferences::TestingPrefServiceSyncable* prefs,
               bool enabled,
               TemplateURLData* data,
               bool is_mandatory) {
  if (enabled) {
    EXPECT_FALSE(data->keyword().empty());
    EXPECT_FALSE(data->url().empty());
  }
  base::Value::Dict entry = TemplateURLDataToDictionary(*data);
  entry.Set(DefaultSearchManager::kDisabledByPolicy, !enabled);

  is_mandatory ? prefs->SetManagedPref(
                     DefaultSearchManager::kDefaultSearchProviderDataPrefName,
                     std::move(entry))
               : prefs->SetRecommendedPref(
                     DefaultSearchManager::kDefaultSearchProviderDataPrefName,
                     std::move(entry));
}

}  // namespace

class DefaultSearchManagerTest : public testing::Test {
 public:
  void SetUp() override {
    // Override the country checks to simulate being in the US.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "US");
    PrefHashFilter::RegisterProfilePrefs(pref_service()->registry());
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &search_engines_test_environment_.pref_service();
  }

  search_engines::SearchEngineChoiceService* search_engine_choice_service() {
    return &search_engines_test_environment_.search_engine_choice_service();
  }

  TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver() {
    return search_engines_test_environment_.prepopulate_data_resolver();
  }

  std::unique_ptr<DefaultSearchManager> create_manager() {
    return std::make_unique<DefaultSearchManager>(
        pref_service(), search_engine_choice_service(),
        search_engines_test_environment_.prepopulate_data_resolver(),
        DefaultSearchManager::ObserverCallback());
  }

  std::unique_ptr<TemplateURLData> set_default_search_provider_data_pref(
      const std::string& keyword) {
    std::unique_ptr<TemplateURLData> data =
        GenerateDummyTemplateURLData(keyword);
    pref_service()->SetDict(
        DefaultSearchManager::kDefaultSearchProviderDataPrefName,
        TemplateURLDataToDictionary(*data));
    return data;
  }

  void set_mirrored_default_search_provider_data_pref(
      const std::string& keyword) {
    std::unique_ptr<TemplateURLData> data =
        GenerateDummyTemplateURLData(keyword);
    pref_service()->SetDict(
        DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName,
        TemplateURLDataToDictionary(*data));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
};

// Test that a TemplateURLData object is properly written and read from Prefs.
TEST_F(DefaultSearchManagerTest, ReadAndWritePref) {
  auto manager = create_manager();
  TemplateURLData data;
  data.SetShortName(u"name1");
  data.SetKeyword(u"key1");
  data.SetURL("http://foo1/{searchTerms}");
  data.suggestions_url = "http://sugg1";
  data.alternate_urls.push_back("http://foo1/alt");
  data.favicon_url = GURL("http://icon1");
  data.safe_for_autoreplace = true;
  data.input_encodings = base::SplitString(
      "UTF-8;UTF-16", ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  data.date_created = base::Time();
  data.last_modified = base::Time();
  data.last_modified = base::Time();
  data.regulatory_origin = RegulatoryExtensionType::kAndroidEEA;

  manager->SetUserSelectedDefaultSearchEngine(data);
  const TemplateURLData* read_data = manager->GetDefaultSearchEngine(nullptr);
  ExpectSimilar(&data, read_data);
}

// Test DefaultSearchmanager handles user-selected DSEs correctly.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByUserPref) {
  auto manager = create_manager();
  std::unique_ptr<TemplateURLData> fallback_t_url_data =
      prepopulate_data_resolver().GetFallbackSearch();
  EXPECT_EQ(fallback_t_url_data->keyword(),
            TemplateURLPrepopulateData::google.keyword);
  EXPECT_EQ(fallback_t_url_data->prepopulate_id,
            TemplateURLPrepopulateData::google.id);
  DefaultSearchManager::Source source = DefaultSearchManager::FROM_POLICY;
  // If no user pref is set, we should use the pre-populated values.
  ExpectSimilar(fallback_t_url_data.get(),
                manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);

  base::HistogramTester histograms;

  // Setting a user pref overrides the pre-populated values.
  std::unique_ptr<TemplateURLData> data = GenerateDummyTemplateURLData("user");
  manager->SetUserSelectedDefaultSearchEngine(*data);

  ExpectSimilar(data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Updating the user pref (externally to this instance of
  // DefaultSearchManager) triggers an update.
  std::unique_ptr<TemplateURLData> new_data =
      GenerateDummyTemplateURLData("user2");

  auto other_manager = create_manager();
  other_manager->SetUserSelectedDefaultSearchEngine(*new_data);

  ExpectSimilar(new_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Check that the mirrored pref metric didn't record a mismatch.
  // Metric is recorded once for the first search manager, and then four more
  // times when there are two search managers.
  histograms.ExpectBucketCount(
      DefaultSearchManager::kDefaultSearchEngineMirroredMetric, true, 1);

  histograms.ExpectBucketCount(
      DefaultSearchManager::kDefaultSearchEngineMirroredMetric, false, 0);

  // Clearing the user pref should cause the default search to revert to the
  // prepopulated values.
  manager->ClearUserSelectedDefaultSearchEngine();
  ExpectSimilar(fallback_t_url_data.get(),
                manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);
}

// Test that DefaultSearch manager detects changes to kSearchProviderOverrides.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByOverrides) {
  SetOverrides(pref_service(), false);
  auto manager = create_manager();

  std::unique_ptr<TemplateURLData> fallback_t_url_data =
      prepopulate_data_resolver().GetFallbackSearch();
  EXPECT_NE(fallback_t_url_data->keyword(),
            TemplateURLPrepopulateData::google.keyword);
  EXPECT_NE(fallback_t_url_data->prepopulate_id,
            TemplateURLPrepopulateData::google.id);

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_POLICY;
  EXPECT_TRUE(manager->GetDefaultSearchEngine(&source));
  TemplateURLData first_default(*manager->GetDefaultSearchEngine(&source));
  ExpectSimilar(fallback_t_url_data.get(), &first_default);
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);

  // Update the overrides:
  SetOverrides(pref_service(), true);
  fallback_t_url_data = prepopulate_data_resolver().GetFallbackSearch();

  // Make sure DefaultSearchManager updated:
  ExpectSimilar(fallback_t_url_data.get(),
                manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);
  EXPECT_NE(manager->GetDefaultSearchEngine(nullptr)->short_name(),
            first_default.short_name());
  EXPECT_NE(manager->GetDefaultSearchEngine(nullptr)->keyword(),
            first_default.keyword());
}

// Test DefaultSearchManager handles policy-enforced DSEs correctly.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByEnforcedPolicy) {
  auto manager = create_manager();
  std::unique_ptr<TemplateURLData> data = GenerateDummyTemplateURLData("user");
  manager->SetUserSelectedDefaultSearchEngine(*data);

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_FALLBACK;
  ExpectSimilar(data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  std::unique_ptr<TemplateURLData> policy_data =
      GenerateDummyTemplateURLData("policy");
  SetPolicy(pref_service(), true, policy_data.get(), /*is_mandatory=*/true);

  ExpectSimilar(policy_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY, source);

  TemplateURLData null_policy_data;
  SetPolicy(pref_service(), false, &null_policy_data, /*is_mandatory=*/true);
  EXPECT_EQ(nullptr, manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY, source);

  pref_service()->RemoveManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  ExpectSimilar(data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}

// Policy-recommended DSE is handled correctly when no existing DSE is present.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByRecommendedPolicy) {
  auto manager = create_manager();
  DefaultSearchManager::Source source = DefaultSearchManager::FROM_FALLBACK;

  // Set recommended policy DSE with valid data.
  std::unique_ptr<TemplateURLData> policy_data =
      GenerateDummyTemplateURLData("policy");
  SetPolicy(pref_service(), true, policy_data.get(), /*is_mandatory=*/false);
  ExpectSimilar(policy_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY_RECOMMENDED, source);

  // Set recommended policy DSE with null data.
  TemplateURLData null_policy_data;
  SetPolicy(pref_service(), false, &null_policy_data, /*is_mandatory=*/false);
  EXPECT_EQ(nullptr, manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY_RECOMMENDED, source);

  // Set user-configured DSE.
  std::unique_ptr<TemplateURLData> user_data =
      GenerateDummyTemplateURLData("user");
  manager->SetUserSelectedDefaultSearchEngine(*user_data);
  // The user-configured DSE overrides the recommended policy DSE.
  ExpectSimilar(user_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Remove the recommended policy DSE.
  pref_service()->RemoveRecommendedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  ExpectSimilar(user_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}

// Policy-recommended DSE does not override existing DSE set by user.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByUserAndRecommendedPolicy) {
  auto manager = create_manager();

  // Set user-configured DSE.
  std::unique_ptr<TemplateURLData> user_data =
      GenerateDummyTemplateURLData("user");
  manager->SetUserSelectedDefaultSearchEngine(*user_data);
  DefaultSearchManager::Source source = DefaultSearchManager::FROM_FALLBACK;
  ExpectSimilar(user_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Check that the TemplateURLData was mirrored to the mirrored pref.
  const base::Value* user_value = pref_service()->GetUserPrefValue(
      DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName);
  ASSERT_TRUE(user_value && user_value->is_dict());
  auto turl_data = TemplateURLDataFromDictionary(user_value->GetDict());
  ExpectSimilar(user_data.get(), turl_data.get());

  // Set recommended policy DSE.
  std::unique_ptr<TemplateURLData> policy_data =
      GenerateDummyTemplateURLData("policy");
  SetPolicy(pref_service(), true, policy_data.get(), /*is_mandatory=*/false);
  // The recommended policy DSE does not override the existing user DSE.
  ExpectSimilar(user_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Remove the recommended policy DSE.
  pref_service()->RemoveRecommendedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  ExpectSimilar(user_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}

// Test DefaultSearchManager handles extension-controlled DSEs correctly.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByExtension) {
  auto manager = create_manager();
  std::unique_ptr<TemplateURLData> data = GenerateDummyTemplateURLData("user");
  manager->SetUserSelectedDefaultSearchEngine(*data);

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_FALLBACK;
  ExpectSimilar(data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Extension trumps prefs:
  std::unique_ptr<TemplateURLData> extension_data_1 =
      GenerateDummyTemplateURLData("ext1");
  SetExtensionDefaultSearchInPrefs(pref_service(), *extension_data_1);
  ExpectSimilar(extension_data_1.get(),
                manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_EXTENSION, source);

  // Policy trumps extension:
  std::unique_ptr<TemplateURLData> policy_data =
      GenerateDummyTemplateURLData("policy");
  SetPolicy(pref_service(), true, policy_data.get(), /*is_mandatory=*/true);

  ExpectSimilar(policy_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY, source);
  pref_service()->RemoveManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);

  // Extensions trump each other:
  std::unique_ptr<TemplateURLData> extension_data_2 =
      GenerateDummyTemplateURLData("ext2");
  std::unique_ptr<TemplateURLData> extension_data_3 =
      GenerateDummyTemplateURLData("ext3");

  SetExtensionDefaultSearchInPrefs(pref_service(), *extension_data_2);
  SetExtensionDefaultSearchInPrefs(pref_service(), *extension_data_3);
  ExpectSimilar(extension_data_3.get(),
                manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_EXTENSION, source);

  RemoveExtensionDefaultSearchFromPrefs(pref_service());
  ExpectSimilar(data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}

TEST_F(DefaultSearchManagerTest,
       DefaultSearchSetByPlayAPI_MergeByPrepopulatedId) {
  auto manager = create_manager();
  auto* builtin_engine = manager->GetDefaultSearchEngine(nullptr);

  // The test tries to set DSE to the one with prepopulate_id, matching existing
  // prepopulated search engine.
  auto supplied_engine = GenerateDummyTemplateURLData(
      base::UTF16ToUTF8(builtin_engine->keyword()));
  supplied_engine->prepopulate_id = builtin_engine->prepopulate_id;
  // Needed by ExpectSimilar.
  supplied_engine->favicon_url = builtin_engine->favicon_url;

  // Non-Play definitions should be reconciled using prepopulated_id.
  manager->SetUserSelectedDefaultSearchEngine(*supplied_engine);
  auto* result = manager->GetDefaultSearchEngine(nullptr);
  ExpectSimilar(builtin_engine, result);
}

TEST_F(DefaultSearchManagerTest,
       DefaultSearchSetByPlayAPI_MergeByKeyword_FeatureEnabled) {
  auto manager = create_manager();
  auto* builtin_engine = manager->GetDefaultSearchEngine(nullptr);

  auto supplied_engine = GenerateDummyTemplateURLData(
      base::UTF16ToUTF8(builtin_engine->keyword()));
  supplied_engine->regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  // Needed by ExpectSimilar.
  supplied_engine->favicon_url = builtin_engine->favicon_url;

  // Verify engine reconciled with builtin definition.
  manager->SetUserSelectedDefaultSearchEngine(*supplied_engine);
  auto* result = manager->GetDefaultSearchEngine(nullptr);

  TemplateURLData expected_engine = *builtin_engine;
  expected_engine.regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  ExpectSimilar(&expected_engine, result);
}

TEST_F(DefaultSearchManagerTest,
       DefaultSearchSetByPlayAPI_MergeByDomainName_FeatureEnabled) {
  SetOverrides(pref_service(), false);
  auto manager = create_manager();

  // Find the expected engine. We could fabricate one too, this is easier.
  auto all_engines = prepopulate_data_resolver().GetPrepopulatedEngines();
  const auto& builtin_engine =
      *std::ranges::find_if(all_engines, [](const auto& engine) {
        GURL url(engine->url());
        return url.is_valid() && url.host() == "emea.search.yahoo.com";
      });

  auto supplied_engine = GenerateDummyTemplateURLData("yahoo.com");
  supplied_engine->SetURL("https://emea.search.yahoo.com/any_path");
  supplied_engine->regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  // Needed by ExpectSimilar.
  supplied_engine->favicon_url = builtin_engine->favicon_url;

  // Verify engine reconciled with builtin definition.
  manager->SetUserSelectedDefaultSearchEngine(*supplied_engine);
  auto* result = manager->GetDefaultSearchEngine(nullptr);

  TemplateURLData expected_engine = *builtin_engine;
  expected_engine.regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  ExpectSimilar(&expected_engine, result);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
TEST_F(DefaultSearchManagerTest, DefaultSearchReset) {
  base::test::ScopedFeatureList feature_list{
      switches::kResetTamperedDefaultSearchEngine};
  base::HistogramTester histograms;

  set_default_search_provider_data_pref("search_engine_A");
  set_mirrored_default_search_provider_data_pref("search_engine_B");

  auto manager = create_manager();

  // The original and mirrored DSE prefs should have been cleared since they
  // were holding different data.
  EXPECT_TRUE(
      pref_service()
          ->GetDict(DefaultSearchManager::kDefaultSearchProviderDataPrefName)
          .empty());
  EXPECT_TRUE(
      pref_service()
          ->GetDict(
              DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName)
          .empty());

  // Mirror check reset recorded.
  histograms.ExpectUniqueSample(
      DefaultSearchManager::kDefaultSearchEngineMirrorCheckOutcomeMetric,
      static_cast<int>(
          DefaultSearchManager::DefaultSearchEngineMirrorCheckOutcomeType::
              kMirrorCheckReset),
      1);

  // Unacknowledged (notification not yet shown) reset occurred.
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred));
  EXPECT_FALSE(pref_service()->GetTime(
                   prefs::kDefaultSearchEngineMirrorCheckResetTimeStamp) ==
               base::Time());

  // The DSE should now be the fallback.
  DefaultSearchManager::Source source;
  manager->GetDefaultSearchEngine(&source);
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);
}

TEST_F(DefaultSearchManagerTest, UserDseChangeDisablesResetNotification) {
  base::test::ScopedFeatureList feature_list{
      switches::kResetTamperedDefaultSearchEngine};

  auto user_data = set_default_search_provider_data_pref("search_engine_A");
  set_mirrored_default_search_provider_data_pref("search_engine_B");

  auto manager = create_manager();

  // The DSE was reset and notification dialog will show.
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred));
  DefaultSearchManager::Source source;
  manager->GetDefaultSearchEngine(&source);
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);

  // Change the DSE (before the notification is shown).
  set_default_search_provider_data_pref("search_engine_A");

  // The DSE should have been changed.
  ExpectSimilar(user_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Ensure the notification is not shown.
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred));
}

#if BUILDFLAG(IS_WIN)
TEST_F(DefaultSearchManagerTest, DefaultSearchNotResetForEnterprisePolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{switches::kResetTamperedDefaultSearchEngine},
      /*disabled_features=*/{tracked::kEnableEncryptedTrackedPrefOnEnterprise});

  // Simulate an enterprise device.
  base::win::ScopedDomainStateForTesting scoped_domain_state_(true);
  base::HistogramTester histograms;

  auto user_data = set_default_search_provider_data_pref("search_engine_A");
  set_mirrored_default_search_provider_data_pref("search_engine_B");

  auto manager = create_manager();

  // The DSE prefs should NOT be cleared since this is an enterprise device
  // and the feature flag is disabled.
  EXPECT_FALSE(
      pref_service()
          ->GetDict(DefaultSearchManager::kDefaultSearchProviderDataPrefName)
          .empty());
  EXPECT_FALSE(
      pref_service()
          ->GetDict(
              DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName)
          .empty());

  // DSE reset was skipped due to enterprise policy recorded.
  histograms.ExpectUniqueSample(
      DefaultSearchManager::kDefaultSearchEngineMirrorCheckOutcomeMetric,
      static_cast<int>(
          DefaultSearchManager::DefaultSearchEngineMirrorCheckOutcomeType::
              kResetSkippedForEnterpriseDevice),
      1);

  // Reset did not occur.
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred));
  // A mirror check reset time is not recorded.
  EXPECT_TRUE(pref_service()->GetTime(
                  prefs::kDefaultSearchEngineMirrorCheckResetTimeStamp) ==
              base::Time());

  // The DSE should not have been changed.
  DefaultSearchManager::Source source;
  ExpectSimilar(user_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}

TEST_F(DefaultSearchManagerTest,
       DefaultSearchResetForEnterpriseWithFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{switches::kResetTamperedDefaultSearchEngine,
                            tracked::kEnableEncryptedTrackedPrefOnEnterprise},
      /*disabled_features=*/{});

  // Simulate an enterprise device.
  base::win::ScopedDomainStateForTesting scoped_domain_state_(true);
  base::HistogramTester histograms;

  set_default_search_provider_data_pref("search_engine_A");
  set_mirrored_default_search_provider_data_pref("search_engine_B");

  auto manager = create_manager();

  // The original and mirrored DSE prefs should have been cleared, even though
  // this is an enterprise device, because the feature flag is enabled.
  EXPECT_TRUE(
      pref_service()
          ->GetDict(DefaultSearchManager::kDefaultSearchProviderDataPrefName)
          .empty());
  EXPECT_TRUE(
      pref_service()
          ->GetDict(
              DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName)
          .empty());

  histograms.ExpectUniqueSample(
      DefaultSearchManager::kDefaultSearchEngineMirrorCheckOutcomeMetric,
      static_cast<int>(
          DefaultSearchManager::DefaultSearchEngineMirrorCheckOutcomeType::
              kMirrorCheckReset),
      1);

  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred));
  EXPECT_FALSE(pref_service()->GetTime(
                   prefs::kDefaultSearchEngineMirrorCheckResetTimeStamp) ==
               base::Time());

  // The DSE should now be the fallback.
  DefaultSearchManager::Source source;
  manager->GetDefaultSearchEngine(&source);
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(DefaultSearchManagerTest, DontResetDefaultSearchIfFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kResetTamperedDefaultSearchEngine);
  base::HistogramTester histograms;

  auto user_data = set_default_search_provider_data_pref("search_engine_A");
  set_mirrored_default_search_provider_data_pref("search_engine_B");

  auto manager = create_manager();

  // The DSE prefs should NOT be cleared.
  EXPECT_FALSE(
      pref_service()
          ->GetDict(DefaultSearchManager::kDefaultSearchProviderDataPrefName)
          .empty());
  EXPECT_FALSE(
      pref_service()
          ->GetDict(
              DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName)
          .empty());

  // Nothing recorded in DefaultSearchEngineTamperingReset metric.
  histograms.ExpectTotalCount(
      DefaultSearchManager::kDefaultSearchEngineMirrorCheckOutcomeMetric, 0);

  // Reset did not occur.
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred));
  // A mirror check reset time is not recorded.
  EXPECT_TRUE(pref_service()->GetTime(
                  prefs::kDefaultSearchEngineMirrorCheckResetTimeStamp) ==
              base::Time());

  // The DSE should not have been changed.
  DefaultSearchManager::Source source;
  ExpectSimilar(user_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}

TEST_F(DefaultSearchManagerTest, DontResetDefaultSearchIfPrefsMatch) {
  base::test::ScopedFeatureList feature_list{
      switches::kResetTamperedDefaultSearchEngine};
  base::HistogramTester histograms;

  auto user_data = set_default_search_provider_data_pref("search_engine_A");
  pref_service()->SetDict(
      DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName,
      TemplateURLDataToDictionary(*user_data));

  auto manager = create_manager();

  // The DSE prefs should NOT be cleared since the original and mirrored pref
  // are the same.
  EXPECT_FALSE(
      pref_service()
          ->GetDict(DefaultSearchManager::kDefaultSearchProviderDataPrefName)
          .empty());
  EXPECT_FALSE(
      pref_service()
          ->GetDict(
              DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName)
          .empty());

  // No tampering detected recorded.
  histograms.ExpectUniqueSample(
      DefaultSearchManager::kDefaultSearchEngineMirrorCheckOutcomeMetric,
      static_cast<int>(
          DefaultSearchManager::DefaultSearchEngineMirrorCheckOutcomeType::
              kNoTamperingDetected),
      1);

  // Reset did not occur.
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred));
  // A mirror check reset time is not recorded.
  EXPECT_TRUE(pref_service()->GetTime(
                  prefs::kDefaultSearchEngineMirrorCheckResetTimeStamp) ==
              base::Time());

  // The DSE should not have been changed.
  DefaultSearchManager::Source source;
  ExpectSimilar(user_data.get(), manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}

TEST_F(DefaultSearchManagerTest, RecentHmacReset) {
  base::test::ScopedFeatureList feature_list{
      switches::kResetTamperedDefaultSearchEngine};
  base::HistogramTester histograms;

  // Empty DSE pref and a filled mirror pref to simulate DSE reset by HMAC
  // check.
  EXPECT_TRUE(
      pref_service()
          ->GetDict(DefaultSearchManager::kDefaultSearchProviderDataPrefName)
          .empty());
  set_mirrored_default_search_provider_data_pref("search_engine_A");
  // Simulate the HMAC based reset happened now.
  PrefHashFilter::SetResetTimeForTesting(pref_service(), base::Time::Now());

  auto manager = create_manager();

  // The original DSE prefs should still be empty.
  EXPECT_TRUE(
      pref_service()
          ->GetDict(DefaultSearchManager::kDefaultSearchProviderDataPrefName)
          .empty());
  // The mirrored DSE pref should have been cleared.
  EXPECT_TRUE(
      pref_service()
          ->GetDict(
              DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName)
          .empty());

  // A recent HMAC reset was recorded.
  histograms.ExpectUniqueSample(
      DefaultSearchManager::kDefaultSearchEngineMirrorCheckOutcomeMetric,
      static_cast<int>(
          DefaultSearchManager::DefaultSearchEngineMirrorCheckOutcomeType::
              kRecentHmacReset),
      1);

  // Unacknowledged (notification not yet shown) reset occurred.
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred));
  // A mirror check reset time is not recorded.
  EXPECT_TRUE(pref_service()->GetTime(
                  prefs::kDefaultSearchEngineMirrorCheckResetTimeStamp) ==
              base::Time());
}

TEST_F(DefaultSearchManagerTest, StaleHmacReset) {
  base::test::ScopedFeatureList feature_list{
      switches::kResetTamperedDefaultSearchEngine};
  base::HistogramTester histograms;

  // Empty DSE pref and a filled mirror pref to simulate DSE reset by HMAC
  // check.
  EXPECT_TRUE(
      pref_service()
          ->GetDict(DefaultSearchManager::kDefaultSearchProviderDataPrefName)
          .empty());
  set_mirrored_default_search_provider_data_pref("search_engine_A");
  // Simulate the HMAC based reset happened previously.
  PrefHashFilter::ClearResetTime(pref_service());

  auto manager = create_manager();

  // The original DSE prefs should still be empty.
  EXPECT_TRUE(
      pref_service()
          ->GetDict(DefaultSearchManager::kDefaultSearchProviderDataPrefName)
          .empty());
  // The mirrored DSE pref should have been cleared.
  EXPECT_TRUE(
      pref_service()
          ->GetDict(
              DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName)
          .empty());

  // A stale HMAC reset was recorded.
  histograms.ExpectUniqueSample(
      DefaultSearchManager::kDefaultSearchEngineMirrorCheckOutcomeMetric,
      static_cast<int>(
          DefaultSearchManager::DefaultSearchEngineMirrorCheckOutcomeType::
              kStaleHmacReset),
      1);

  // Unacknowledged (notification not yet shown) DSE reset did not occur.
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred));
  // A mirror check reset time is not recorded.
  EXPECT_TRUE(pref_service()->GetTime(
                  prefs::kDefaultSearchEngineMirrorCheckResetTimeStamp) ==
              base::Time());
}

TEST_F(DefaultSearchManagerTest, EncryptionResetSetsUnacknowledgedResetPref) {
  base::test::ScopedFeatureList feature_list{
      switches::kResetTamperedDefaultSearchEngine};
  base::HistogramTester histograms;

  // Set up matching prefs so that no reset happens on initialization.
  auto user_data = set_default_search_provider_data_pref("search_engine_A");
  pref_service()->SetDict(
      DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName,
      TemplateURLDataToDictionary(*user_data));

  auto manager = create_manager();

  // Verify that no reset occurred on initialization.
  EXPECT_FALSE(
      pref_service()
          ->GetDict(DefaultSearchManager::kDefaultSearchProviderDataPrefName)
          .empty());

  // Simulate an encrypted hash based reset by clearing the main DSE pref.
  pref_service()->ClearPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);

  // Unacknowledged (notification not yet shown) reset occurred.
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred));
  // A mirror check reset time is not recorded.
  EXPECT_TRUE(pref_service()->GetTime(
                  prefs::kDefaultSearchEngineMirrorCheckResetTimeStamp) ==
              base::Time());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
