// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/search_engines/util.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata/common/webdata_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<TemplateURLData> CreatePrepopulateTemplateURLData(
    int prepopulate_id,
    const std::string& keyword) {
  return std::make_unique<TemplateURLData>(
      u"Search engine name", base::ASCIIToUTF16(keyword), "https://search.url",
      "" /* suggest_url */, "" /* image_url */, "" /* image_translate_url */,
      "" /* new_tab_url */, "" /* contextual_search_url */, "" /* logo_url */,
      "" /* doodle_url */, "" /* search_url_post_params */,
      "" /* suggest_url_post_params */, "" /* image_url_post_params */,
      "" /* side_search_param */, "" /* side_image_search_param */,
      "" /* image_translate_source_language_param_key */,
      "" /* image_translate_target_language_param_key */,
      std::vector<std::string>() /* search_intent_params */,
      "" /* favicon_url */, "UTF-8", u"" /* image_search_branding_label */,
      base::Value::List() /* alternate_urls_list */,
      false /* preconnect_to_search_url */,
      false /* prefetch_likely_navigations */, prepopulate_id);
}

// Creates a TemplateURL with default values except for the prepopulate ID,
// keyword and TemplateURLID. Only use this in tests if your tests do not
// care about other fields.
std::unique_ptr<TemplateURL> CreatePrepopulateTemplateURL(
    int prepopulate_id,
    const std::string& keyword,
    TemplateURLID id,
    bool is_play_api_turl = false) {
  std::unique_ptr<TemplateURLData> data =
      CreatePrepopulateTemplateURLData(prepopulate_id, keyword);
  data->id = id;
  data->created_from_play_api = is_play_api_turl;
  return std::make_unique<TemplateURL>(*data);
}

// Sets up dependencies and calls `GetSearchProvidersUsingLoadedEngines()`.
// As with the wrapped function, `template_urls` will be updated with the loaded
// engines, including the starter pack ones, and `*resource_keyword_version`
// will be set to the version number for the loaded data or to 0 if no
// prepopulated engines were loaded.
void CallGetSearchProvidersUsingLoadedEngines(
    PrefService* prefs,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    int* resource_keyword_version) {
  // Setup inspired by `//components/webdata_services/web_data_service_wrapper*`

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::UI};
  auto task_runner = task_environment.GetMainThreadTaskRunner();

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  auto profile_database = base::MakeRefCounted<WebDatabaseService>(
      scoped_temp_dir.GetPath().Append(kWebDataFilename),
      /*ui_task_runner=*/task_runner,
      /*db_task_runner=*/task_runner);
  profile_database->AddTable(std::make_unique<KeywordTable>());
  profile_database->LoadDatabase();

  auto keyword_web_data = base::MakeRefCounted<KeywordWebDataService>(
      profile_database, task_runner);
  keyword_web_data->Init(base::DoNothing());

  {
    SearchTermsData search_terms_data;
    std::set<std::string> removed_keyword_guids;
    int resource_starter_pack_version = 0;

    GetSearchProvidersUsingLoadedEngines(
        keyword_web_data.get(), prefs, template_urls,
        /*default_search_provider=*/nullptr, search_terms_data,
        resource_keyword_version, &resource_starter_pack_version,
        &removed_keyword_guids);

    EXPECT_TRUE(removed_keyword_guids.empty());
  }

  keyword_web_data->ShutdownOnUISequence();
  profile_database->ShutdownDatabase();
}

}  // namespace

TEST(TemplateURLServiceUtilTest, RemoveDuplicatePrepopulateIDs) {
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_turls;
  TemplateURLService::OwnedTemplateURLVector local_turls;

  prepopulated_turls.push_back(CreatePrepopulateTemplateURLData(1, "winner4"));
  prepopulated_turls.push_back(CreatePrepopulateTemplateURLData(2, "xxx"));
  prepopulated_turls.push_back(CreatePrepopulateTemplateURLData(3, "yyy"));

  // Create a sets of different TURLs grouped by prepopulate ID. Each group
  // will test a different heuristic of RemoveDuplicatePrepopulateIDs.
  // Ignored set - These should be left alone as they do not have valid
  // prepopulate IDs.
  local_turls.push_back(CreatePrepopulateTemplateURL(0, "winner1", 4));
  local_turls.push_back(CreatePrepopulateTemplateURL(0, "winner2", 5));
  local_turls.push_back(CreatePrepopulateTemplateURL(0, "winner3", 6));
  size_t num_non_prepopulated_urls = local_turls.size();

  // Keyword match set - Prefer the one that matches the keyword of the
  // prepopulate ID.
  local_turls.push_back(CreatePrepopulateTemplateURL(1, "loser1", 7));
  local_turls.push_back(CreatePrepopulateTemplateURL(1, "loser2", 8));
  local_turls.push_back(CreatePrepopulateTemplateURL(1, "winner4", 9));

  // Default set - Prefer the default search engine over all other criteria.
  // The last one is the default. It will be passed as the
  // default_search_provider parameter to RemoveDuplicatePrepopulateIDs.
  local_turls.push_back(CreatePrepopulateTemplateURL(2, "loser3", 10));
  local_turls.push_back(CreatePrepopulateTemplateURL(2, "xxx", 11));
  local_turls.push_back(CreatePrepopulateTemplateURL(2, "winner5", 12));
  TemplateURL* default_turl = local_turls.back().get();

  // ID set - Prefer the lowest TemplateURLID if the keywords don't match and if
  // none are the default.
  local_turls.push_back(CreatePrepopulateTemplateURL(3, "winner6", 13));
  local_turls.push_back(CreatePrepopulateTemplateURL(3, "loser5", 14));
  local_turls.push_back(CreatePrepopulateTemplateURL(3, "loser6", 15));

  RemoveDuplicatePrepopulateIDs(nullptr, prepopulated_turls, default_turl,
                                &local_turls, SearchTermsData(), nullptr);

  // Verify that the expected local TURLs survived the process.
  EXPECT_EQ(local_turls.size(),
            prepopulated_turls.size() + num_non_prepopulated_urls);
  for (const auto& turl : local_turls) {
    EXPECT_TRUE(base::StartsWith(turl->keyword(), u"winner",
                                 base::CompareCase::SENSITIVE));
  }
}

// Tests correct interaction of Play API search engine during prepopulated list
// update.
TEST(TemplateURLServiceUtilTest, MergeEnginesFromPrepopulateData_PlayAPI) {
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_turls;
  TemplateURLService::OwnedTemplateURLVector local_turls;

  // Start with single search engine created from Play API data.
  local_turls.push_back(CreatePrepopulateTemplateURL(0, "play", 1, true));

  // Test that prepopulated search engine with matching keyword is merged with
  // Play API search engine. Search URL should come from Play API search engine.
  const std::string prepopulated_search_url = "http://prepopulated.url";
  prepopulated_turls.push_back(CreatePrepopulateTemplateURLData(1, "play"));
  prepopulated_turls.back()->SetURL(prepopulated_search_url);
  MergeEnginesFromPrepopulateData(nullptr, &prepopulated_turls, &local_turls,
                                  nullptr, nullptr);
  ASSERT_EQ(local_turls.size(), 1U);
  // Merged search engine should have both Play API flag and valid
  // prepopulate_id.
  EXPECT_TRUE(local_turls[0]->created_from_play_api());
  EXPECT_EQ(1, local_turls[0]->prepopulate_id());
  EXPECT_NE(prepopulated_search_url, local_turls[0]->url());

  // Test that merging prepopulated search engine with matching prepopulate_id
  // preserves keyword of Play API search engine.
  prepopulated_turls.clear();
  prepopulated_turls.push_back(CreatePrepopulateTemplateURLData(1, "play2"));
  MergeEnginesFromPrepopulateData(nullptr, &prepopulated_turls, &local_turls,
                                  nullptr, nullptr);
  ASSERT_EQ(local_turls.size(), 1U);
  EXPECT_TRUE(local_turls[0]->created_from_play_api());
  EXPECT_EQ(local_turls[0]->keyword(), u"play");

  // Test that removing search engine from prepopulated list doesn't delete Play
  // API search engine record.
  prepopulated_turls.clear();
  MergeEnginesFromPrepopulateData(nullptr, &prepopulated_turls, &local_turls,
                                  nullptr, nullptr);
  ASSERT_EQ(local_turls.size(), 1U);
  EXPECT_TRUE(local_turls[0]->created_from_play_api());
  EXPECT_EQ(local_turls[0]->prepopulate_id(), 0);
}

// Tests that user modified fields are preserved and overwritten appropriately
// in MergeIntoEngineData().
TEST(TemplateURLServiceUtilTest, MergeIntoEngineData) {
  std::unique_ptr<TemplateURLData> original_turl_data =
      CreatePrepopulateTemplateURLData(1, "google");
  std::unique_ptr<TemplateURLData> url_to_update =
      CreatePrepopulateTemplateURLData(1, "google");

  // Modify the keyword and title for original_turl and set safe_for_autoreplace
  // to false to simulate a "user edited" template url.
  original_turl_data->SetShortName(u"modified name");
  original_turl_data->SetKeyword(u"new keyword");
  original_turl_data->safe_for_autoreplace = false;

  std::unique_ptr<TemplateURL> original_turl =
      std::make_unique<TemplateURL>(*original_turl_data);

  // Set `merge_options` to kOverwriteUserEdits. This should NOT preserve the
  // modified fields.  `url_to_update` should keep the default keyword and name
  // values as well as safe_for_autoreplace being true.
  MergeIntoEngineData(original_turl.get(), url_to_update.get(),
                      TemplateURLMergeOption::kOverwriteUserEdits);

  EXPECT_TRUE(url_to_update->safe_for_autoreplace);
  EXPECT_EQ(url_to_update->short_name(), u"Search engine name");
  EXPECT_EQ(url_to_update->keyword(), u"google");

  // Set `merge_options` to kDefault. This should preserve the modified
  // keyword and title fields from original_turl and update url_to_update
  // accordingly.
  MergeIntoEngineData(original_turl.get(), url_to_update.get(),
                      TemplateURLMergeOption::kDefault);

  EXPECT_FALSE(url_to_update->safe_for_autoreplace);
  EXPECT_EQ(url_to_update->short_name(), u"modified name");
  EXPECT_EQ(url_to_update->keyword(), u"new keyword");
}

TEST(TemplateURLServiceUtilTest, GetSearchProvidersUsingLoadedEngines) {
  using TemplateURLPrepopulateData::kCurrentDataVersion;

  sync_preferences::TestingPrefServiceSyncable prefs;
  size_t starter_pack_engines_count =
      TemplateURLStarterPackData::GetStarterPackEngines().size();

  // Simulates how the search providers are loaded during Chrome init by calling
  // `GetSearchProvidersUsingLoadedEngines()`. `prefs` carries the profile prefs
  // between runs.
  // Returns a struct with fields (`loaded_engines_count` and `loaded_version`)
  // that can be verified against expectations.
  auto simulate_run = [&](bool enable_feature, int mocked_current_version) {
    base::test::ScopedFeatureList feature_list;
    if (enable_feature) {
      feature_list.InitAndEnableFeature(switches::kSearchEngineChoice);
    } else {
      feature_list.InitWithFeatures({}, {switches::kSearchEngineChoice,
                                         switches::kSearchEngineChoiceFre});
    }

    TemplateURLService::OwnedTemplateURLVector template_urls;
    int resource_keyword_version = mocked_current_version;
    CallGetSearchProvidersUsingLoadedEngines(&prefs, &template_urls,
                                             &resource_keyword_version);

    struct {
      size_t loaded_engines_count;
      int loaded_version;
    } result{
        template_urls.size() - starter_pack_engines_count,
        resource_keyword_version,
    };

    return result;
  };

  TemplateURLPrepopulateData::RegisterProfilePrefs(prefs.registry());
  prefs.SetInteger(country_codes::kCountryIDAtInstall,
                   country_codes::CountryCharsToCountryID('B', 'E'));

  // Users initially have the feature is disabled, we should load the 5 engines.
  auto result = simulate_run(/*enable_feature=*/false,
                             /*mocked_current_version=*/0);
  EXPECT_EQ(result.loaded_engines_count, 5u);
  EXPECT_EQ(result.loaded_version, kCurrentDataVersion);
  EXPECT_FALSE(
      prefs.GetBoolean(prefs::kDefaultSearchProviderKeywordsUseExtendedList));

  // When re-loading with the same configuration, no new load should happen as
  // the data did not change.
  result = simulate_run(/*enable_feature=*/false,
                        /*mocked_current_version=*/kCurrentDataVersion);
  EXPECT_EQ(result.loaded_engines_count, 0u);
  EXPECT_EQ(result.loaded_version, 0);
  EXPECT_FALSE(
      prefs.GetBoolean(prefs::kDefaultSearchProviderKeywordsUseExtendedList));

  // When loading with the feature this time, the engines should be reloaded.
  result = simulate_run(/*enable_feature=*/true,
                        /*mocked_current_version=*/kCurrentDataVersion);
  EXPECT_EQ(result.loaded_engines_count, 12u);
  EXPECT_EQ(result.loaded_version, kCurrentDataVersion);
  EXPECT_TRUE(
      prefs.GetBoolean(prefs::kDefaultSearchProviderKeywordsUseExtendedList));

  // As for without the feature, no reload when the configuration is the same.
  result = simulate_run(/*enable_feature=*/true,
                        /*mocked_current_version=*/kCurrentDataVersion);
  EXPECT_EQ(result.loaded_engines_count, 0u);
  EXPECT_EQ(result.loaded_version, 0);
  EXPECT_TRUE(
      prefs.GetBoolean(prefs::kDefaultSearchProviderKeywordsUseExtendedList));

  // And when disabling the feature, we should reload the shorter list.
  result = simulate_run(/*enable_feature=*/false,
                        /*mocked_current_version=*/kCurrentDataVersion);
  EXPECT_EQ(result.loaded_engines_count, 5u);
  EXPECT_EQ(result.loaded_version, kCurrentDataVersion);
  EXPECT_FALSE(
      prefs.GetBoolean(prefs::kDefaultSearchProviderKeywordsUseExtendedList));

  // Toggling the feature state with an older prepopulated data version should
  // not force the data merge. Guards against a small edge cases, for example if
  // a user has different versions of chrome with different sets of prepopulated
  // engines running on different computers.
  result = simulate_run(/*enable_feature=*/true,
                        /*mocked_current_version=*/kCurrentDataVersion + 1);
  EXPECT_EQ(result.loaded_engines_count, 0u);
  EXPECT_EQ(result.loaded_version, 0);
  EXPECT_FALSE(
      prefs.GetBoolean(prefs::kDefaultSearchProviderKeywordsUseExtendedList));
}
