// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_manager.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/search_engines/prepopulated_engines.h"
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
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &search_engines_test_environment_.pref_service();
  }

  search_engines::SearchEngineChoiceService* search_engine_choice_service() {
    return &search_engines_test_environment_.search_engine_choice_service();
  }

  std::unique_ptr<DefaultSearchManager> create_manager() {
    return std::make_unique<DefaultSearchManager>(
        pref_service(), search_engine_choice_service(),
        DefaultSearchManager::ObserverCallback()
#if BUILDFLAG(IS_CHROMEOS_LACROS)
            ,
        /*for_lacros_main_profile=*/false
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    );
  }

 private:
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
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
  data.created_from_play_api = true;

  manager->SetUserSelectedDefaultSearchEngine(data);
  const TemplateURLData* read_data = manager->GetDefaultSearchEngine(nullptr);
  ExpectSimilar(&data, read_data);
}

// Test DefaultSearchmanager handles user-selected DSEs correctly.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByUserPref) {
  auto manager = create_manager();
  std::unique_ptr<TemplateURLData> fallback_t_url_data =
      TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          pref_service(), search_engine_choice_service());
  EXPECT_EQ(fallback_t_url_data->keyword(),
            TemplateURLPrepopulateData::google.keyword);
  EXPECT_EQ(fallback_t_url_data->prepopulate_id,
            TemplateURLPrepopulateData::google.id);
  DefaultSearchManager::Source source = DefaultSearchManager::FROM_POLICY;
  // If no user pref is set, we should use the pre-populated values.
  ExpectSimilar(fallback_t_url_data.get(),
                manager->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);

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
      TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          pref_service(), search_engine_choice_service());
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
  fallback_t_url_data =
      TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          pref_service(), search_engine_choice_service());

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

  // Play definitions must not be reconciled using prepopulated_id.
  supplied_engine->created_from_play_api = true;
  manager->SetUserSelectedDefaultSearchEngine(*supplied_engine);
  result = manager->GetDefaultSearchEngine(nullptr);
  ExpectSimilar(supplied_engine.get(), result);
}

TEST_F(DefaultSearchManagerTest,
       DefaultSearchSetByPlayAPI_MergeByKeyword_FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(switches::kTemplateUrlReconciliation);

  auto manager = create_manager();
  auto* builtin_engine = manager->GetDefaultSearchEngine(nullptr);

  auto supplied_engine = GenerateDummyTemplateURLData(
      base::UTF16ToUTF8(builtin_engine->keyword()));
  supplied_engine->created_from_play_api = true;
  // Needed by ExpectSimilar.
  supplied_engine->favicon_url = builtin_engine->favicon_url;

  // Verify no merge done.
  manager->SetUserSelectedDefaultSearchEngine(*supplied_engine);
  auto* result = manager->GetDefaultSearchEngine(nullptr);
  ExpectSimilar(supplied_engine.get(), result);
}

TEST_F(DefaultSearchManagerTest,
       DefaultSearchSetByPlayAPI_MergeByKeyword_FeatureEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(switches::kTemplateUrlReconciliation);

  auto manager = create_manager();
  auto* builtin_engine = manager->GetDefaultSearchEngine(nullptr);

  auto supplied_engine = GenerateDummyTemplateURLData(
      base::UTF16ToUTF8(builtin_engine->keyword()));
  supplied_engine->created_from_play_api = true;
  // Needed by ExpectSimilar.
  supplied_engine->favicon_url = builtin_engine->favicon_url;

  // Verify engine reconciled with builtin definition.
  manager->SetUserSelectedDefaultSearchEngine(*supplied_engine);
  auto* result = manager->GetDefaultSearchEngine(nullptr);

  TemplateURLData expected_engine = *builtin_engine;
  expected_engine.created_from_play_api = true;
  ExpectSimilar(&expected_engine, result);
}

TEST_F(DefaultSearchManagerTest,
       DefaultSearchSetByPlayAPI_MergeByDomainName_FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(switches::kTemplateUrlReconciliation);

  auto manager = create_manager();
  auto* builtin_engine = manager->GetDefaultSearchEngine(nullptr);

  auto supplied_engine = GenerateDummyTemplateURLData("yahoo.com");
  supplied_engine->SetURL("https://emea.yahoo.com/search");
  supplied_engine->created_from_play_api = true;
  // Needed by ExpectSimilar.
  supplied_engine->favicon_url = builtin_engine->favicon_url;

  // Verify no merge done.
  manager->SetUserSelectedDefaultSearchEngine(*supplied_engine);
  auto* result = manager->GetDefaultSearchEngine(nullptr);
  ExpectSimilar(supplied_engine.get(), result);
}

TEST_F(DefaultSearchManagerTest,
       DefaultSearchSetByPlayAPI_MergeByDomainName_FeatureEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(switches::kTemplateUrlReconciliation);

  SetOverrides(pref_service(), false);
  auto manager = create_manager();

  // Find the expected engine. We could fabricate one too, this is easier.
  auto all_engines = TemplateURLPrepopulateData::GetPrepopulatedEngines(
      pref_service(), search_engine_choice_service());
  const auto& builtin_engine =
      *base::ranges::find_if(all_engines, [](const auto& engine) {
        GURL url(engine->url());
        return url.is_valid() && url.host_piece() == "emea.search.yahoo.com";
      });

  auto supplied_engine = GenerateDummyTemplateURLData("yahoo.com");
  supplied_engine->SetURL("https://emea.search.yahoo.com/any_path");
  supplied_engine->created_from_play_api = true;
  // Needed by ExpectSimilar.
  supplied_engine->favicon_url = builtin_engine->favicon_url;

  // Verify engine reconciled with builtin definition.
  manager->SetUserSelectedDefaultSearchEngine(*supplied_engine);
  auto* result = manager->GetDefaultSearchEngine(nullptr);

  TemplateURLData expected_engine = *builtin_engine;
  expected_engine.created_from_play_api = true;
  ExpectSimilar(&expected_engine, result);
}
