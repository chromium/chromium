// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/country_codes/country_codes.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/search_engines/util.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata/common/webdata_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
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
      false /* prefetch_likely_navigations */, prepopulate_id,
      /* regulatory extensions */
      base::span<TemplateURLData::RegulatoryExtension>());
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
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    WDKeywordsResult::Metadata& inout_resource_metadata,
    os_crypt_async::OSCryptAsync* os_crypt) {
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
  profile_database->LoadDatabase(os_crypt);

  auto keyword_web_data = base::MakeRefCounted<KeywordWebDataService>(
      profile_database, task_runner);
  keyword_web_data->Init(base::DoNothing());

  {
    SearchTermsData search_terms_data;
    std::set<std::string> removed_keyword_guids;

    GetSearchProvidersUsingLoadedEngines(
        keyword_web_data.get(), prefs, search_engine_choice_service,
        template_urls,
        /*default_search_provider=*/nullptr, search_terms_data,
        inout_resource_metadata, &removed_keyword_guids);

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
  original_turl_data->SetKeyword(u"newkeyword");
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
  EXPECT_EQ(url_to_update->keyword(), u"newkeyword");
}

class TemplateURLServiceUtilLoadTest : public testing::Test {
 public:
  TemplateURLServiceUtilLoadTest()
      : os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)) {}

  // Type used both as input and output of test helpers, to represent the
  // state of the database from its metadata.
  struct KeywordTestMetadata {
    // Version of the built-in keywords data.
    int data_version = 0;

    // Country stored in the database. As such, when passed as input, it will
    // be used to update only the database. To change the profile's country,
    // write directly to prefs.
    int country = 0;

    // Number of keywords search engines available. Ignored when passing the
    // struct as input to set the database's initial state.
    size_t keyword_engines_count = 0;

    // Whether the database is expected to be configured to show the extended
    // list with more than 5 keywords search engines. Gets set in prefs, not
    // in the database metadata.
    std::optional<bool> use_extended_list = std::nullopt;

    // Formatter method for Google Test.
    friend std::ostream& operator<<(std::ostream& out,
                                    const KeywordTestMetadata& m) {
      return out << "{data_version=" << m.data_version
                 << ", country=" << m.country
                 << ", keyword_engines_count=" << m.keyword_engines_count
                 << ", use_extended_list="
                 << (m.use_extended_list.has_value()
                         ? (*m.use_extended_list ? "yes" : "no")
                         : "unset")
                 << "}";
    }

    // Needed to be able to use EXPECT_EQ with this struct.
    bool operator==(const KeywordTestMetadata& rhs) const {
      return data_version == rhs.data_version && country == rhs.country &&
             keyword_engines_count == rhs.keyword_engines_count &&
             use_extended_list == rhs.use_extended_list;
    }
  };

  const int kCurrentDataVersion =
      TemplateURLPrepopulateData::kCurrentDataVersion;

  // For country samples, using Belgium and France for EEA, and the United
  // States for non-EEA.
  const int kEeaCountryId = country_codes::CountryStringToCountryID("BE");
  const int kOtherEeaCountryId = country_codes::CountryStringToCountryID("FR");
  const int kNonEeaCountryId = country_codes::CountryStringToCountryID("US");

  // Simulates how the search providers are loaded during Chrome init by
  // calling `GetSearchProvidersUsingLoadedEngines()`.
  // The `initial_state` struct represents the state of the database from its
  // metadata, before the search providers are loaded. Note:
  // `keyword_engines_count` is ignored in the input.
  // The returned struct represents the database state after the search
  // providers are loaded.
  KeywordTestMetadata SimulateFromDatabaseState(
      KeywordTestMetadata initial_state) {
    if (initial_state.use_extended_list.has_value()) {
      prefs().SetBoolean(prefs::kDefaultSearchProviderKeywordsUseExtendedList,
                         *initial_state.use_extended_list);
    } else {
      prefs().ClearPref(prefs::kDefaultSearchProviderKeywordsUseExtendedList);
    }

    TemplateURLService::OwnedTemplateURLVector template_urls;
    WDKeywordsResult::Metadata resource_metadata;
    resource_metadata.builtin_keyword_data_version = initial_state.data_version;
    resource_metadata.builtin_keyword_country = initial_state.country;
    CallGetSearchProvidersUsingLoadedEngines(
        &prefs(),
        &search_engines_test_environment_.search_engine_choice_service(),
        &template_urls, resource_metadata, os_crypt_.get());

    std::optional<bool> use_extended_list_output =
        prefs().HasPrefPath(
            prefs::kDefaultSearchProviderKeywordsUseExtendedList)
            ? std::optional<bool>(prefs().GetBoolean(
                  prefs::kDefaultSearchProviderKeywordsUseExtendedList))
            : std::nullopt;
    size_t keyword_engines_count =
        template_urls.size() -
        TemplateURLStarterPackData::GetStarterPackEngines().size();

    return {.data_version = resource_metadata.builtin_keyword_data_version,
            .country = resource_metadata.builtin_keyword_country,
            .keyword_engines_count = keyword_engines_count,
            .use_extended_list = use_extended_list_output};
  }

  PrefService& prefs() {
    return search_engines_test_environment_.pref_service();
  }

  search_engines::SearchEngineChoiceService& search_engine_choice_service() {
    return search_engines_test_environment_.search_engine_choice_service();
  }

 private:
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
};

TEST_F(TemplateURLServiceUtilLoadTest,
       GetSearchProvidersUsingLoadedEngines_OutOfEea) {
  search_engine_choice_service().ClearCountryIdCacheForTesting();
  prefs().SetInteger(country_codes::kCountryIDAtInstall, kNonEeaCountryId);

  const KeywordTestMetadata kDefaultUpdatedState = {
      .data_version = kCurrentDataVersion,
      .country = kNonEeaCountryId,
      .keyword_engines_count = 5u};
  const KeywordTestMetadata kNoUpdate = {.data_version = 0,
                                         .country = 0,
                                         .keyword_engines_count = 0u};

  // Initial state: nothing. Simulates a fresh install.
  // The function should populate the profile with 5 engines and current
  // metadata.
  auto output = SimulateFromDatabaseState({});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // When using the latest metadata from the binary, the function should not
  // update anything.
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion,
                                      .country = kNonEeaCountryId});
  EXPECT_EQ(output, (KeywordTestMetadata{.data_version = 0,
                                         .country = 0,
                                         .keyword_engines_count = 0u}));

  // Missing country ID doesn't trigger an update either.
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion});
  EXPECT_EQ(output, kNoUpdate);

  // Out of date keyword data versions trigger updates
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion - 1});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // Country changes trigger updates
  output = SimulateFromDatabaseState(
      {.data_version = kCurrentDataVersion, .country = kOtherEeaCountryId});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // If the extended list was previously used, the function will re-run to
  // shorten it.
  output = SimulateFromDatabaseState(
      {.data_version = kCurrentDataVersion, .use_extended_list = true});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // If database's data version is more recent than the one built-in to the
  // client, the updates are suppressed, including shortening the list.
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion + 1,
                                      .country = kOtherEeaCountryId,
                                      .use_extended_list = true});
  EXPECT_EQ(output, (KeywordTestMetadata{.data_version = 0,
                                         .country = 0,
                                         .keyword_engines_count = 0u,
                                         .use_extended_list = true}));
}

TEST_F(TemplateURLServiceUtilLoadTest,
       GetSearchProvidersUsingLoadedEngines_InEea) {
  search_engine_choice_service().ClearCountryIdCacheForTesting();
  prefs().SetInteger(country_codes::kCountryIDAtInstall, kEeaCountryId);
  const size_t kEeaKeywordEnginesCount =
      TemplateURLPrepopulateData::GetPrepopulationSetFromCountryIDForTesting(
          kEeaCountryId)
          .size();

  const KeywordTestMetadata kDefaultUpdatedState = {
      .data_version = kCurrentDataVersion,
      .country = kEeaCountryId,
      .keyword_engines_count = kEeaKeywordEnginesCount,
      .use_extended_list = true};
  const KeywordTestMetadata kNoUpdate = {.data_version = 0,
                                         .country = 0,
                                         .keyword_engines_count = 0u,
                                         .use_extended_list = true};

  // Initial state: nothing. Simulates a fresh install.
  // The function should populate the profile with 8 engines and current
  // metadata.
  auto output = SimulateFromDatabaseState({});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // When using the latest metadata from the binary, the function should not
  // update anything.
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion,
                                      .country = kEeaCountryId,
                                      .use_extended_list = true});
  EXPECT_EQ(output, kNoUpdate);

  // Missing country ID doesn't trigger an update either.
  output = SimulateFromDatabaseState(
      {.data_version = kCurrentDataVersion, .use_extended_list = true});
  EXPECT_EQ(output, kNoUpdate);

  // Out of date keyword data versions trigger updates
  output = SimulateFromDatabaseState(
      {.data_version = kCurrentDataVersion - 1, .use_extended_list = true});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // Country changes trigger updates
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion,
                                      .country = kOtherEeaCountryId,
                                      .use_extended_list = true});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // If the short list was previously used, the function will re-run to
  // extend it.
  output = SimulateFromDatabaseState(
      {.data_version = kCurrentDataVersion, .use_extended_list = std::nullopt});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // If database's data version is more recent than the one built-in to the
  // client, the updates are suppressed, including extending the list.
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion + 1,
                                      .country = kOtherEeaCountryId,
                                      .use_extended_list = std::nullopt});
  EXPECT_EQ(output, (KeywordTestMetadata{.data_version = 0,
                                         .country = 0,
                                         .keyword_engines_count = 0u,
                                         .use_extended_list = std::nullopt}));
}
