// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/country_codes/country_codes.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/async/common/test_encryptor.h"
#include "components/regional_capabilities/program_settings.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_prefs.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/template_url_service_test_util.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/search_engines/util.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata/common/webdata_constants.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/search_engines_data/resources/definitions/regional_settings.h"

namespace {

using regional_capabilities::CountryIdHolder;

constexpr std::u16string_view sample_engine_name = u"Search engine name";

std::unique_ptr<TemplateURLData> CreatePrepopulateTemplateURLData(
    int prepopulate_id,
    const std::string& keyword) {
  return std::make_unique<TemplateURLData>(
      sample_engine_name, base::ASCIIToUTF16(keyword), "https://search.url",
      "" /* suggest_url */, "" /* image_url */, "" /* image_translate_url */,
      "" /* new_tab_url */, "" /* contextual_search_url */, "" /* logo_url */,
      "" /* doodle_url */, "" /* search_url_post_params */,
      "" /* suggest_url_post_params */, "" /* image_url_post_params */,
      "" /* image_translate_source_language_param_key */,
      "" /* image_translate_target_language_param_key */,
      std::vector<std::string>() /* search_intent_params */,
      "" /* favicon_url */, "UTF-8", u"" /* image_search_branding_label */,
      base::ListValue() /* alternate_urls_list */,
      false /* preconnect_to_search_url */,
      false /* prefetch_likely_navigations */, /*send_x_geo_header=*/false,
      prepopulate_id,
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
    RegulatoryExtensionType reg_ext_type = RegulatoryExtensionType::kDefault) {
  std::unique_ptr<TemplateURLData> data =
      CreatePrepopulateTemplateURLData(prepopulate_id, keyword);
  data->id = id;
  data->regulatory_origin = reg_ext_type;
  return std::make_unique<TemplateURL>(*data);
}

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
  local_turls.push_back(
      CreatePrepopulateTemplateURL(0, "winner1", TemplateURLID(4)));
  local_turls.push_back(
      CreatePrepopulateTemplateURL(0, "winner2", TemplateURLID(5)));
  local_turls.push_back(
      CreatePrepopulateTemplateURL(0, "winner3", TemplateURLID(6)));
  size_t num_non_prepopulated_urls = local_turls.size();

  // Keyword match set - Prefer the one that matches the keyword of the
  // prepopulate ID.
  local_turls.push_back(
      CreatePrepopulateTemplateURL(1, "loser1", TemplateURLID(7)));
  local_turls.push_back(
      CreatePrepopulateTemplateURL(1, "loser2", TemplateURLID(8)));
  local_turls.push_back(
      CreatePrepopulateTemplateURL(1, "winner4", TemplateURLID(9)));

  // Default set - Prefer the default search engine over all other criteria.
  // The last one is the default. It will be passed as the
  // default_search_provider parameter to RemoveDuplicatePrepopulateIDs.
  local_turls.push_back(
      CreatePrepopulateTemplateURL(2, "loser3", TemplateURLID(10)));
  local_turls.push_back(
      CreatePrepopulateTemplateURL(2, "xxx", TemplateURLID(11)));
  local_turls.push_back(
      CreatePrepopulateTemplateURL(2, "winner5", TemplateURLID(12)));
  TemplateURL* default_turl = local_turls.back().get();

  // ID set - Prefer the lowest TemplateURLID if the keywords don't match and if
  // none are the default.
  local_turls.push_back(
      CreatePrepopulateTemplateURL(3, "winner6", TemplateURLID(13)));
  local_turls.push_back(
      CreatePrepopulateTemplateURL(3, "loser5", TemplateURLID(14)));
  local_turls.push_back(
      CreatePrepopulateTemplateURL(3, "loser6", TemplateURLID(15)));

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

class TemplateURLServiceUtilWithEnvTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
};

// Tests correct interaction of Play API search engine during prepopulated list
// update.
TEST_F(TemplateURLServiceUtilWithEnvTest,
       MergeEnginesFromPrepopulateData_PlayAPI) {
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_turls;
  TemplateURLService::OwnedTemplateURLVector local_turls;

  // Start with single search engine created from Play API data.
  local_turls.push_back(CreatePrepopulateTemplateURL(
      0, "play", TemplateURLID(1), RegulatoryExtensionType::kAndroidEEA));

  // Test that prepopulated search engine with matching keyword is merged with
  // Play API search engine. Search URL should come from Play API search engine.
  const std::string prepopulated_search_url = "http://prepopulated.url";
  prepopulated_turls.push_back(CreatePrepopulateTemplateURLData(1, "play"));
  prepopulated_turls.back()->SetURL(prepopulated_search_url);
  MergeEnginesFromPrepopulateData(
      nullptr, &prepopulated_turls, &local_turls, nullptr,
      search_engines_test_environment_.prepopulate_data_resolver(), nullptr);
  ASSERT_EQ(local_turls.size(), 1U);
  // Merged search engine should have both Play API flag and valid
  // prepopulate_id.
  ASSERT_EQ(local_turls[0]->GetRegulatoryExtensionType(),
            RegulatoryExtensionType::kAndroidEEA);
  EXPECT_EQ(1, local_turls[0]->prepopulate_id());
  EXPECT_NE(prepopulated_search_url, local_turls[0]->url());

  // Test that merging prepopulated search engine with matching prepopulate_id
  // preserves keyword of Play API search engine.
  prepopulated_turls.clear();
  prepopulated_turls.push_back(CreatePrepopulateTemplateURLData(1, "play2"));
  MergeEnginesFromPrepopulateData(
      nullptr, &prepopulated_turls, &local_turls, nullptr,
      search_engines_test_environment_.prepopulate_data_resolver(), nullptr);
  ASSERT_EQ(local_turls.size(), 1U);
  ASSERT_EQ(local_turls[0]->GetRegulatoryExtensionType(),
            RegulatoryExtensionType::kAndroidEEA);
  EXPECT_EQ(local_turls[0]->keyword(), u"play");

  // Test that removing search engine from prepopulated list doesn't delete Play
  // API search engine record.
  prepopulated_turls.clear();
  MergeEnginesFromPrepopulateData(
      nullptr, &prepopulated_turls, &local_turls, nullptr,
      search_engines_test_environment_.prepopulate_data_resolver(), nullptr);
  ASSERT_EQ(local_turls.size(), 1U);
  ASSERT_EQ(local_turls[0]->GetRegulatoryExtensionType(),
            RegulatoryExtensionType::kAndroidEEA);
  EXPECT_EQ(local_turls[0]->prepopulate_id(), 0);
}

TEST_F(
    TemplateURLServiceUtilWithEnvTest,
    CreateActionsFromCurrentPrepopulateData_RegulatoryDuplicatesSameKeyword) {
  TemplateURLService::OwnedTemplateURLVector existing_urls;

  // Add duplicate regulatory engines with the SAME keyword.
  TemplateURLData reg_data1;
  reg_data1.SetShortName(u"Google");
  reg_data1.SetKeyword(u"reg_keyword");
  reg_data1.SetURL("http://google.com/search?q={searchTerms}");
  reg_data1.regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  reg_data1.safe_for_autoreplace = false;
  reg_data1.is_active = TemplateURLData::ActiveStatus::kTrue;

  TemplateURLData reg_data2;
  reg_data2.SetShortName(u"Google");
  reg_data2.SetKeyword(u"reg_keyword");
  reg_data2.SetURL("http://google.com/search?q={searchTerms}");
  reg_data2.regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  reg_data2.safe_for_autoreplace = false;
  reg_data2.is_active = TemplateURLData::ActiveStatus::kTrue;

  existing_urls.push_back(std::make_unique<TemplateURL>(reg_data1));
  existing_urls.push_back(std::make_unique<TemplateURL>(reg_data2));

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls;

  base::HistogramTester histogram_tester;
  CreateActionsFromCurrentPrepopulateData(
      &prepopulated_urls, existing_urls,
      /*default_search_provider=*/nullptr,
      search_engines_test_environment_.prepopulate_data_resolver());

  // Should record TRUE since keywords are identical and raw_regulatory_count
  // > 1.
  histogram_tester.ExpectUniqueSample(
      "Omnibox.TemplateUrl.DBRefresh.RegulatoryDuplicatesSameKeyword", true, 1);
}

TEST_F(
    TemplateURLServiceUtilWithEnvTest,
    CreateActionsFromCurrentPrepopulateData_RegulatoryDuplicatesDifferentKeyword) {
  TemplateURLService::OwnedTemplateURLVector existing_urls;

  // Add duplicate regulatory engines with DIFFERENT keywords.
  TemplateURLData reg_data1;
  reg_data1.SetShortName(u"Google");
  reg_data1.SetKeyword(u"reg_keyword_1");
  reg_data1.SetURL("http://google.com/search?q={searchTerms}");
  reg_data1.regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  reg_data1.safe_for_autoreplace = false;
  reg_data1.is_active = TemplateURLData::ActiveStatus::kTrue;

  TemplateURLData reg_data2;
  reg_data2.SetShortName(u"Google");
  reg_data2.SetKeyword(u"reg_keyword_2");
  reg_data2.SetURL("http://google.com/search?q={searchTerms}");
  reg_data2.regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  reg_data2.safe_for_autoreplace = false;
  reg_data2.is_active = TemplateURLData::ActiveStatus::kTrue;

  existing_urls.push_back(std::make_unique<TemplateURL>(reg_data1));
  existing_urls.push_back(std::make_unique<TemplateURL>(reg_data2));

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls;

  base::HistogramTester histogram_tester;
  CreateActionsFromCurrentPrepopulateData(
      &prepopulated_urls, existing_urls,
      /*default_search_provider=*/nullptr,
      search_engines_test_environment_.prepopulate_data_resolver());

  // Should record FALSE since keywords are different and
  // raw_regulatory_count > 1.
  histogram_tester.ExpectUniqueSample(
      "Omnibox.TemplateUrl.DBRefresh.RegulatoryDuplicatesSameKeyword", false,
      1);
}

// Tests that user modified fields are preserved and overwritten appropriately
// in MergeIntoEngineData().
TEST(TemplateURLServiceUtilTest, MergeIntoEngineData_OverwriteUserEdits) {
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
  MergeIntoEngineData(original_turl->data(), *url_to_update.get(),
                      TemplateURLMergeOption::kOverwriteUserEdits);

  EXPECT_TRUE(url_to_update->safe_for_autoreplace);
  EXPECT_EQ(url_to_update->short_name(), sample_engine_name);
  EXPECT_EQ(url_to_update->keyword(), u"google");
  EXPECT_EQ(url_to_update->sync_guid, original_turl->sync_guid());
}

TEST(TemplateURLServiceUtilTest, MergeIntoEngineData_Default) {
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

  // Set `merge_options` to kDefault. This should preserve the modified
  // keyword and title fields from original_turl and update url_to_update
  // accordingly.
  MergeIntoEngineData(original_turl->data(), *url_to_update.get(),
                      TemplateURLMergeOption::kDefault);

  EXPECT_FALSE(url_to_update->safe_for_autoreplace);
  EXPECT_EQ(url_to_update->short_name(), u"modified name");
  EXPECT_EQ(url_to_update->keyword(), u"newkeyword");
  EXPECT_EQ(url_to_update->sync_guid, original_turl->sync_guid());
}

TEST(TemplateURLServiceUtilTest, MergeIntoEngineData_SplitPrepopulatedEntry) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kPrepopulatedEnginesMigration};

  std::unique_ptr<TemplateURLData> original_turl_data =
      CreatePrepopulateTemplateURLData(1, "google");
  std::unique_ptr<TemplateURLData> url_to_update =
      CreatePrepopulateTemplateURLData(130, "google");

  // Modify the keyword and title for original_turl and set safe_for_autoreplace
  // to false to simulate a "user edited" template url.
  original_turl_data->SetShortName(u"modified name");
  original_turl_data->SetKeyword(u"newkeyword");
  original_turl_data->safe_for_autoreplace = false;
  original_turl_data->migrate_to_id = 130;

  std::unique_ptr<TemplateURL> original_turl =
      std::make_unique<TemplateURL>(*original_turl_data);

  // Set `merge_options` to kSplitPrepopulatedEntry. This should skip resetting
  // the `sync_guid`, ensuring that the split entry does not get merged with old
  // sync data.
  MergeIntoEngineData(original_turl->data(), *url_to_update.get(),
                      TemplateURLMergeOption::kSplitPrepopulatedEntry);

  EXPECT_FALSE(url_to_update->safe_for_autoreplace);
  EXPECT_EQ(url_to_update->short_name(), u"modified name");
  EXPECT_EQ(url_to_update->keyword(), u"newkeyword");
  EXPECT_NE(url_to_update->sync_guid, original_turl->sync_guid());
}

TEST(TemplateURLServiceUtilTest, MergeIntoEngineData_MigrationLogging) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kPrepopulatedEnginesMigration};
  base::HistogramTester histogram_tester;

  std::unique_ptr<TemplateURLData> original_turl_data =
      CreatePrepopulateTemplateURLData(1, "google");
  std::unique_ptr<TemplateURLData> url_to_update =
      CreatePrepopulateTemplateURLData(130, "google");
  original_turl_data->migrate_to_id = 130;

  std::unique_ptr<TemplateURL> original_turl =
      std::make_unique<TemplateURL>(*original_turl_data);

  // Test kMigratedNonDefault
  MergeIntoEngineData(original_turl->data(), *url_to_update.get(),
                      TemplateURLMergeOption::kSplitPrepopulatedEntry);

  histogram_tester.ExpectUniqueSample(
      "Omnibox.TemplateUrl.DBRefresh.MigrationAction",
      1 /*kMigratedNonDefault*/, 1);

  // Test kMigratedDefaultProvider
  url_to_update = CreatePrepopulateTemplateURLData(130, "google");
  MergeIntoEngineData(original_turl->data(), *url_to_update.get(),
                      TemplateURLMergeOption::kSettingAsDefaultProvider);

  histogram_tester.ExpectBucketCount(
      "Omnibox.TemplateUrl.DBRefresh.MigrationAction",
      0 /*kMigratedDefaultProvider*/, 1);
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
    std::optional<CountryIdHolder> country = std::nullopt;

    // Number of keywords search engines available. Ignored when passing the
    // struct as input to set the database's initial state.
    size_t keyword_engines_count = 0;

    // Prepopulated engines migration state stored in the database.
    KeywordTable::PrepopulatedEngineMigrationSet migration_state;

    // Formatter method for Google Test.
    friend std::ostream& operator<<(std::ostream& out,
                                    const KeywordTestMetadata& m) {
      return out << "{data_version=" << m.data_version << ", country="
                 << (m.country.has_value()
                         ? base::NumberToString(
                               m.country->GetForTesting().Serialize())
                         : "<null>")
                 << ", keyword_engines_count=" << m.keyword_engines_count
                 << ", migration_state=" << m.migration_state.ToString() << "}";
    }

    // Needed to be able to use EXPECT_EQ with this struct.
    bool operator==(const KeywordTestMetadata& rhs) const {
      return data_version == rhs.data_version && country == rhs.country &&
             keyword_engines_count == rhs.keyword_engines_count &&
             migration_state == rhs.migration_state;
    }
  };

  const int kCurrentDataVersion =
      TemplateURLPrepopulateData::kCurrentDataVersion;

  // For country samples, using Belgium and France for EEA, and the United
  // States for non-EEA.
  const country_codes::CountryId kRawEeaCountryId =
      country_codes::CountryId("BE");
  const CountryIdHolder kEeaCountryId = CountryIdHolder(kRawEeaCountryId);
  const CountryIdHolder kOtherEeaCountryId =
      CountryIdHolder(country_codes::CountryId("FR"));
  const country_codes::CountryId kRawNonEeaCountryId =
      country_codes::CountryId("US");
  const CountryIdHolder kNonEeaCountryId = CountryIdHolder(kRawNonEeaCountryId);

  // Sets up dependencies and calls `GetSearchProvidersUsingLoadedEngines()`.
  // As with the wrapped function, `template_urls` will be updated with the
  // loaded engines, including the starter pack ones, and
  // `*resource_keyword_version` will be set to the version number for the
  // loaded data or to 0 if no prepopulated engines were loaded.
  void CallGetSearchProvidersUsingLoadedEngines(
      TemplateURLService::OwnedTemplateURLVector* template_urls,
      WDKeywordsResult::Metadata& inout_resource_metadata,
      os_crypt_async::OSCryptAsync* os_crypt) {
    // Setup inspired by
    // `//components/webdata_services/web_data_service_wrapper*`

    auto task_runner = task_environment_.GetMainThreadTaskRunner();

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
          keyword_web_data.get(),
          &search_engines_test_environment_.pref_service(),
          search_engines_test_environment_.prepopulate_data_resolver(),
          template_urls,
          /*default_search_provider=*/nullptr, search_terms_data,
          inout_resource_metadata,
          /*had_prepopulated_decryption_failure=*/false,
          /*had_starter_pack_decryption_failure=*/false,
          &removed_keyword_guids);

      EXPECT_TRUE(removed_keyword_guids.empty());
    }

    keyword_web_data->ShutdownOnUISequence();
    profile_database->ShutdownDatabase();
  }

  // Simulates how the search providers are loaded during Chrome init by
  // calling `GetSearchProvidersUsingLoadedEngines()`.
  // The `initial_state` struct represents the state of the database from its
  // metadata, before the search providers are loaded. Note:
  // `keyword_engines_count` is ignored in the input.
  // The returned struct represents the database state after the search
  // providers are loaded.
  KeywordTestMetadata SimulateFromDatabaseState(
      KeywordTestMetadata initial_state) {
    TemplateURLService::OwnedTemplateURLVector template_urls;
    WDKeywordsResult::Metadata resource_metadata;
    resource_metadata.builtin_keyword_data_version = initial_state.data_version;
    resource_metadata.builtin_keyword_country = initial_state.country;
    resource_metadata.prepopulated_engines_migration_state =
        initial_state.migration_state;
    CallGetSearchProvidersUsingLoadedEngines(&template_urls, resource_metadata,
                                             os_crypt_.get());
    size_t keyword_engines_count =
        template_urls.size() -
        template_url_starter_pack_data::GetStarterPackEngines().size();

    return {
        .data_version = resource_metadata.builtin_keyword_data_version,
        .country = resource_metadata.builtin_keyword_country,
        .keyword_engines_count = keyword_engines_count,
        .migration_state =
            resource_metadata.prepopulated_engines_migration_state,
    };
  }

  search_engines::SearchEngineChoiceService& search_engine_choice_service() {
    return search_engines_test_environment_.search_engine_choice_service();
  }

  regional_capabilities::RegionalCapabilitiesService&
  regional_capabilities_service() {
    return search_engines_test_environment_.regional_capabilities_service();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
};

TEST_F(TemplateURLServiceUtilLoadTest,
       GetSearchProvidersUsingLoadedEngines_OutOfEea) {
  regional_capabilities_service().SetCacheForTesting(
      kRawNonEeaCountryId, regional_capabilities::GetSettingsForProgram(
                               regional_capabilities::Program::kDefault));

  const KeywordTestMetadata kDefaultUpdatedState = {
      .data_version = kCurrentDataVersion,
      .country = kNonEeaCountryId,
      .keyword_engines_count = 5u};
  const KeywordTestMetadata kNoUpdate = {};

  // Initial state: nothing. Simulates a fresh install.
  // The function should populate the profile with 5 engines and current
  // metadata.
  auto output = SimulateFromDatabaseState({});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // When using the latest metadata from the binary, the function should not
  // update anything.
  output = SimulateFromDatabaseState(
      {.data_version = kCurrentDataVersion, .country = kNonEeaCountryId});
  EXPECT_EQ(output, kNoUpdate);

  // Missing country ID triggers updates.
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // Out of date keyword data versions trigger updates
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion - 1});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // Country changes trigger updates
  output = SimulateFromDatabaseState(
      {.data_version = kCurrentDataVersion, .country = kOtherEeaCountryId});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // If database's data version is more recent than the one built-in to the
  // client, the updates are suppressed.
  output = SimulateFromDatabaseState(
      {.data_version = kCurrentDataVersion + 1, .country = kOtherEeaCountryId});
  EXPECT_EQ(output, kNoUpdate);
}

TEST_F(TemplateURLServiceUtilLoadTest,
       GetSearchProvidersUsingLoadedEngines_InEea) {
  regional_capabilities_service().SetCacheForTesting(
      kRawEeaCountryId, regional_capabilities::GetSettingsForProgram(
                            regional_capabilities::Program::kWaffle));
  const size_t kEeaKeywordEnginesCount =
      TemplateURLPrepopulateData::kRegionalSettings
          .find(kEeaCountryId.GetForTesting())
          ->second->search_engines.size();

  const KeywordTestMetadata kDefaultUpdatedState = {
      .data_version = kCurrentDataVersion,
      .country = kEeaCountryId,
      .keyword_engines_count = kEeaKeywordEnginesCount};
  const KeywordTestMetadata kNoUpdate = {};

  // Initial state: nothing. Simulates a fresh install.
  // The function should populate the profile with 8 engines and current
  // metadata.
  auto output = SimulateFromDatabaseState({});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // When using the latest metadata from the binary, the function should not
  // update anything.
  output = SimulateFromDatabaseState(
      {.data_version = kCurrentDataVersion, .country = kEeaCountryId});
  EXPECT_EQ(output, kNoUpdate);

  // Missing country ID triggers an update.
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // Out of date keyword data versions trigger updates
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion - 1});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // Country changes trigger updates
  output = SimulateFromDatabaseState(
      {.data_version = kCurrentDataVersion, .country = kOtherEeaCountryId});
  EXPECT_EQ(output, kDefaultUpdatedState);

  // If database's data version is more recent than the one built-in to the
  // client, the updates are suppressed.
  output = SimulateFromDatabaseState(
      {.data_version = kCurrentDataVersion + 1, .country = kOtherEeaCountryId});
  EXPECT_EQ(output, kNoUpdate);
}

TEST_F(TemplateURLServiceUtilLoadTest,
       GetSearchProvidersUsingLoadedEngines_MigrationRollback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{switches::kPrepopulatedEnginesMigration,
                             switches::kPrepopulatedEnginesShadowVariants});
  regional_capabilities_service().SetCacheForTesting(
      kRawNonEeaCountryId, regional_capabilities::GetSettingsForProgram(
                               regional_capabilities::Program::kDefault));

  const KeywordTable::PrepopulatedEngineMigrationSet kMigrationApplied(
      {KeywordTable::PrepopulatedEngineMigration::kMigration});

  // The database claims that a migration was applied while the associated
  // feature is disabled. The regular update runs, refreshing the engines and
  // recording the metadata with the migration state cleared.
  auto output =
      SimulateFromDatabaseState({.data_version = kCurrentDataVersion,
                                 .country = kNonEeaCountryId,
                                 .migration_state = kMigrationApplied});
  EXPECT_EQ(output, (KeywordTestMetadata{.data_version = kCurrentDataVersion,
                                         .country = kNonEeaCountryId,
                                         .keyword_engines_count = 5u}));

  // Same when the engines needed to be refreshed anyway.
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion - 1,
                                      .country = kNonEeaCountryId,
                                      .migration_state = kMigrationApplied});
  EXPECT_EQ(output, (KeywordTestMetadata{.data_version = kCurrentDataVersion,
                                         .country = kNonEeaCountryId,
                                         .keyword_engines_count = 5u}));

  // Data version downgrades are still suppressed, rollback or not.
  output = SimulateFromDatabaseState({.data_version = kCurrentDataVersion + 1,
                                      .country = kNonEeaCountryId,
                                      .migration_state = kMigrationApplied});
  EXPECT_EQ(output, KeywordTestMetadata{});
}

TEST_F(TemplateURLServiceUtilLoadTest, PrepopulatedDecryptionRecovery) {
  for (bool in_eea : {false, true}) {
    SCOPED_TRACE(in_eea);
    regional_capabilities_service().SetCacheForTesting(
        in_eea ? kRawEeaCountryId : kRawNonEeaCountryId,
        regional_capabilities::GetSettingsForProgram(
            in_eea ? regional_capabilities::Program::kWaffle
                   : regional_capabilities::Program::kDefault));
    auto& resolver =
        search_engines_test_environment_.prepopulate_data_resolver();
    auto prepopulated = resolver.GetPrepopulatedEngines();
    ASSERT_GT(prepopulated.size(), 1u);
    const auto current_metadata =
        resolver.ComputeDatabaseUpdateRequirements({});
    ASSERT_TRUE(current_metadata);

    WDKeywordsResult result;
    result.metadata.builtin_keyword_data_version =
        current_metadata->data_version;
    result.metadata.builtin_keyword_country = current_metadata->country_id;
    result.metadata.prepopulated_engines_migration_state =
        current_metadata->prepopulated_engines_migration_state;
    result.metadata.starter_pack_version =
        template_url_starter_pack_data::GetDataVersion();
    ASSERT_FALSE(resolver.ComputeDatabaseUpdateRequirements(result.metadata));

    // Keep a valid custom engine and a user-edited regional engine.
    TemplateURLData custom;
    custom.SetShortName(u"Custom");
    custom.SetKeyword(u"custom");
    custom.SetURL("https://custom.example/?q={searchTerms}");
    custom.id = TemplateURLID(1);
    custom.safe_for_autoreplace = false;
    result.keywords.push_back(custom);
    TemplateURLData edited = *prepopulated.back();
    edited.id = TemplateURLID(2);
    edited.SetShortName(u"Edited name");
    edited.SetKeyword(u"edited");
    edited.safe_for_autoreplace = false;
    result.keywords.push_back(edited);
    TemplateURL default_search_provider(edited);

    for (bool had_prepopulated_decryption_failure : {false, true}) {
      SCOPED_TRACE(had_prepopulated_decryption_failure);
      result.had_prepopulated_decryption_failure =
          had_prepopulated_decryption_failure;
      TemplateURLService::OwnedTemplateURLVector urls;
      WDKeywordsResult::Metadata updated_metadata;
      std::set<std::string> removed_guids;
      GetSearchProvidersUsingKeywordResult(
          result, /*service=*/nullptr,
          &search_engines_test_environment_.pref_service(), resolver, &urls,
          &default_search_provider, SearchTermsData(), updated_metadata,
          &removed_guids);

      EXPECT_FALSE(updated_metadata.HasBuiltinKeywordData());
      EXPECT_FALSE(updated_metadata.HasStarterPackData());
      EXPECT_TRUE(removed_guids.empty());
      EXPECT_EQ(
          had_prepopulated_decryption_failure ? prepopulated.size() + 1 : 2,
          urls.size());
      auto custom_it =
          std::ranges::find(urls, custom.sync_guid, &TemplateURL::sync_guid);
      ASSERT_NE(custom_it, urls.end());
      EXPECT_EQ((*custom_it)->data(), custom);
      auto edited_it = std::ranges::find(urls, edited.prepopulate_id,
                                         &TemplateURL::prepopulate_id);
      ASSERT_NE(edited_it, urls.end());
      EXPECT_EQ((*edited_it)->keyword(), edited.keyword());
      EXPECT_EQ((*edited_it)->short_name(), edited.short_name());
      EXPECT_EQ((*edited_it)->sync_guid(), edited.sync_guid);
      if (had_prepopulated_decryption_failure) {
        for (const auto& engine : prepopulated) {
          EXPECT_NE(std::ranges::find(urls, engine->prepopulate_id,
                                      &TemplateURL::prepopulate_id),
                    urls.end());
        }
      }
    }
  }
}

TEST_F(TemplateURLServiceUtilLoadTest, StarterPackDecryptionRecovery) {
  auto& resolver = search_engines_test_environment_.prepopulate_data_resolver();
  const auto current_metadata = resolver.ComputeDatabaseUpdateRequirements({});
  ASSERT_TRUE(current_metadata);
  const auto starter_pack =
      template_url_starter_pack_data::GetStarterPackEngines();
  ASSERT_GT(starter_pack.size(), 1u);

  WDKeywordsResult result;
  result.metadata.builtin_keyword_data_version = current_metadata->data_version;
  result.metadata.builtin_keyword_country = current_metadata->country_id;
  result.metadata.prepopulated_engines_migration_state =
      current_metadata->prepopulated_engines_migration_state;
  ASSERT_FALSE(resolver.ComputeDatabaseUpdateRequirements(result.metadata));

  TemplateURLData custom;
  custom.SetShortName(u"Custom");
  custom.SetKeyword(u"custom");
  custom.SetURL("https://custom.example/?q={searchTerms}");
  custom.id = TemplateURLID(1);
  custom.safe_for_autoreplace = false;
  result.keywords.push_back(custom);
  TemplateURL default_search_provider(custom);
  TemplateURLData edited = *starter_pack.front();
  edited.id = TemplateURLID(2);
  edited.SetShortName(u"My bookmarks");
  edited.SetKeyword(u"@mine");
  edited.safe_for_autoreplace = false;
  result.keywords.push_back(edited);

  for (bool outdated_metadata : {false, true}) {
    SCOPED_TRACE(outdated_metadata);
    result.metadata.starter_pack_version =
        template_url_starter_pack_data::GetDataVersion() - outdated_metadata;
    for (bool decryption_failure : {false, true}) {
      SCOPED_TRACE(decryption_failure);
      result.had_starter_pack_decryption_failure = decryption_failure;
      TemplateURLService::OwnedTemplateURLVector urls;
      WDKeywordsResult::Metadata updated_metadata;
      std::set<std::string> removed_guids;
      GetSearchProvidersUsingKeywordResult(
          result, /*service=*/nullptr,
          &search_engines_test_environment_.pref_service(), resolver, &urls,
          &default_search_provider, SearchTermsData(), updated_metadata,
          &removed_guids);

      // Starter pack recovery neither refreshes regional engines nor fakes a
      // metadata migration. Normal version updates still work independently.
      EXPECT_FALSE(updated_metadata.HasBuiltinKeywordData());
      EXPECT_EQ(updated_metadata.starter_pack_version,
                outdated_metadata
                    ? template_url_starter_pack_data::GetDataVersion()
                    : 0);
      EXPECT_TRUE(removed_guids.empty());
      const bool reconciled = outdated_metadata || decryption_failure;
      EXPECT_EQ(reconciled ? starter_pack.size() + 1 : 2, urls.size());
      auto custom_it =
          std::ranges::find(urls, custom.sync_guid, &TemplateURL::sync_guid);
      ASSERT_NE(custom_it, urls.end());
      EXPECT_EQ((*custom_it)->data(), custom);
      auto edited_it =
          std::ranges::find(urls, edited.sync_guid, &TemplateURL::sync_guid);
      ASSERT_NE(edited_it, urls.end());
      EXPECT_EQ((*edited_it)->short_name(), edited.short_name());
      EXPECT_EQ((*edited_it)->keyword(), edited.keyword());
      if (reconciled) {
        for (const auto& engine : starter_pack) {
          EXPECT_NE(
              std::ranges::find(
                  urls,
                  static_cast<template_url_starter_pack_data::StarterPackId>(
                      engine->starter_pack_id),
                  &TemplateURL::starter_pack_id),
              urls.end());
        }
      }
    }
  }
}

#if BUILDFLAG(IS_WIN)
class TemplateURLServiceDecryptionLoadTest
    : public TemplateURLServiceUtilLoadTest,
      public testing::WithParamInterface<bool> {
 protected:
  enum class EngineSet { kPrepopulated, kStarterPack, kCustom, kAllBuiltins };

  void SetUp() override {
    regional_capabilities_service().SetCacheForTesting(
        kRawNonEeaCountryId, regional_capabilities::GetSettingsForProgram(
                                 regional_capabilities::Program::kDefault));
    auto task_runner = task_environment_.GetMainThreadTaskRunner();
    database_ = base::MakeRefCounted<WebDatabaseService>(
        base::FilePath(WebDatabase::kInMemoryPath), task_runner, task_runner);
    database_->AddTable(std::make_unique<KeywordTable>());
    database_->LoadDatabase(os_crypt_.get());
    keyword_service_ =
        base::MakeRefCounted<KeywordWebDataService>(database_, task_runner);
    keyword_service_->Init(base::DoNothing());
    LoadService();
  }

  void TearDown() override {
    if (service_) {
      service_->Shutdown();
      service_.reset();
    }
    keyword_service_->ShutdownOnUISequence();
    database_->ShutdownDatabase();
    WaitForPendingDatabaseTasks();
  }

  void WaitForPendingDatabaseTasks() {
    // The reply runs after previously posted DB operations, including their
    // transaction commits or shutdown, have completed.
    base::RunLoop run_loop;
    database_->GetDbSequence()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                                 run_loop.QuitClosure());
    run_loop.Run();
  }

  void LoadService() {
    auto& env = search_engines_test_environment_;
    service_ = std::make_unique<TemplateURLService>(
        env.pref_service(), env.search_engine_choice_service(),
        env.prepopulate_data_resolver(), env.regional_capabilities_service(),
        env.profile_metrics_service(), std::make_unique<SearchTermsData>(),
        keyword_service_, /*client=*/nullptr, base::RepeatingClosure());
    service_->Load();
    TemplateURLServiceLoadWaiter().WaitForLoadComplete(*service_);
  }

  void MakeHashesUndecryptable(EngineSet engine_set) {
    // Flush queued keyword writes and verify that recovery cannot rely on a
    // version or country migration.
    auto keywords = ReadKeywords();
    ASSERT_TRUE(keywords);
    EXPECT_FALSE(keywords->had_prepopulated_decryption_failure);
    EXPECT_FALSE(keywords->had_starter_pack_decryption_failure);
    ASSERT_EQ(keywords->metadata.starter_pack_version,
              template_url_starter_pack_data::GetDataVersion());
    ASSERT_FALSE(search_engines_test_environment_.prepopulate_data_resolver()
                     .ComputeDatabaseUpdateRequirements(keywords->metadata));
    const auto hash =
        service_->GetDefaultSearchProvider()->data().GenerateHash();
    service_->Shutdown();
    service_.reset();
    // Encrypt with a different key so decryption fails even though encryption
    // services remain available when the keywords are loaded.
    const auto encrypted_hash =
        os_crypt_async::GetTestEncryptorForTesting()->EncryptString(
            std::string(hash.begin(), hash.end()));
    ASSERT_TRUE(encrypted_hash);
    database_->ScheduleDBTask(
        FROM_HERE, base::BindLambdaForTesting([&](WebDatabase* db) {
          std::string query = "UPDATE keywords SET url_hash=? WHERE ";
          switch (engine_set) {
            case EngineSet::kPrepopulated:
              query += "prepopulate_id>0";
              break;
            case EngineSet::kStarterPack:
              query += "starter_pack_id>0";
              break;
            case EngineSet::kCustom:
              query += "prepopulate_id=0 AND starter_pack_id=0";
              break;
            case EngineSet::kAllBuiltins:
              query += "prepopulate_id>0 OR starter_pack_id>0";
              break;
          }
          sql::Statement s(db->GetSQLConnection()->GetUniqueStatement(query));
          s.BindBlob(0, *encrypted_hash);
          EXPECT_TRUE(s.Run());
          return WebDatabase::COMMIT_NEEDED;
        }));
    WaitForPendingDatabaseTasks();
  }

  std::optional<WDKeywordsResult> ReadKeywords() {
    class Consumer : public WebDataServiceConsumer {
     public:
      void OnWebDataServiceRequestDone(
          WebDataServiceBase::Handle,
          std::unique_ptr<WDTypedResult> result) override {
        future_.SetValue(std::move(result));
      }
      base::test::TestFuture<std::unique_ptr<WDTypedResult>> future_;
    } consumer;
    keyword_service_->GetKeywords(&consumer);
    const auto result = consumer.future_.Take();
    if (!result) {
      ADD_FAILURE() << "Failed to load keywords";
      return std::nullopt;
    }
    if (result->GetType() != KEYWORDS_RESULT) {
      ADD_FAILURE() << "Unexpected keyword result type: " << result->GetType();
      return std::nullopt;
    }
    return static_cast<const WDResult<WDKeywordsResult>&>(*result).GetValue();
  }

  void ExpectRegionalEngines() {
    const auto prepopulated =
        search_engines_test_environment_.prepopulate_data_resolver()
            .GetPrepopulatedEngines();
    const auto urls = service_->GetTemplateURLs();
    EXPECT_EQ(prepopulated.size(), static_cast<size_t>(std::ranges::count_if(
                                       urls, [](const TemplateURL* url) {
                                         return url->prepopulate_id() > 0;
                                       })));
    for (const auto& engine : prepopulated) {
      EXPECT_NE(std::ranges::find(urls, engine->prepopulate_id,
                                  &TemplateURL::prepopulate_id),
                urls.end());
    }
  }

  void ExpectStarterPackEngines() {
    const auto starter_pack =
        template_url_starter_pack_data::GetStarterPackEngines();
    const auto urls = service_->GetTemplateURLs();
    EXPECT_EQ(starter_pack.size(),
              static_cast<size_t>(
                  std::ranges::count_if(urls, [](const TemplateURL* url) {
                    return url->starter_pack_id() !=
                           template_url_starter_pack_data::StarterPackId::kNone;
                  })));
    for (const auto& engine : starter_pack) {
      const auto found = std::ranges::find(
          urls,
          static_cast<template_url_starter_pack_data::StarterPackId>(
              engine->starter_pack_id),
          &TemplateURL::starter_pack_id);
      ASSERT_NE(found, urls.end());
      EXPECT_EQ((*found)->url(), engine->url());
    }
  }

  scoped_refptr<WebDatabaseService> database_;
  scoped_refptr<KeywordWebDataService> keyword_service_;
  std::unique_ptr<TemplateURLService> service_;
};

TEST_F(TemplateURLServiceDecryptionLoadTest,
       PrepopulatedDecryptionFailurePropagatesToResult) {
  MakeHashesUndecryptable(EngineSet::kPrepopulated);
  const auto result = ReadKeywords();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->had_prepopulated_decryption_failure);
  EXPECT_FALSE(result->had_starter_pack_decryption_failure);
  EXPECT_FALSE(search_engines_test_environment_.prepopulate_data_resolver()
                   .ComputeDatabaseUpdateRequirements(result->metadata));
  EXPECT_TRUE(std::ranges::none_of(
      result->keywords,
      [](const TemplateURLData& data) { return data.prepopulate_id > 0; }));
}

TEST_P(TemplateURLServiceDecryptionLoadTest,
       StarterPackDecryptionFailurePropagatesToResult) {
  MakeHashesUndecryptable(GetParam() ? EngineSet::kAllBuiltins
                                     : EngineSet::kStarterPack);
  const auto result = ReadKeywords();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->had_prepopulated_decryption_failure, GetParam());
  EXPECT_TRUE(result->had_starter_pack_decryption_failure);
  EXPECT_EQ(result->metadata.starter_pack_version,
            template_url_starter_pack_data::GetDataVersion());
  EXPECT_FALSE(search_engines_test_environment_.prepopulate_data_resolver()
                   .ComputeDatabaseUpdateRequirements(result->metadata));
  EXPECT_TRUE(std::ranges::none_of(
      result->keywords,
      [](const TemplateURLData& data) { return data.starter_pack_id > 0; }));
}

TEST_P(TemplateURLServiceDecryptionLoadTest, StarterPackDecryptionRecovery) {
  auto& env = search_engines_test_environment_;
  const auto prepopulated =
      env.prepopulate_data_resolver().GetPrepopulatedEngines();
  ASSERT_GT(prepopulated.size(), 2u);
  const int fallback_id =
      env.prepopulate_data_resolver().GetFallbackSearch()->prepopulate_id;
  const auto selected =
      std::ranges::find_if(prepopulated, [fallback_id](const auto& data) {
        return data->prepopulate_id != fallback_id;
      });
  ASSERT_NE(selected, prepopulated.end());
  auto* selected_engine =
      service_->GetTemplateURLForKeyword((*selected)->keyword());
  ASSERT_TRUE(selected_engine);
  service_->SetUserSelectedDefaultSearchProvider(selected_engine);
  const int selected_id = selected_engine->prepopulate_id();
  const std::string selected_guid = selected_engine->sync_guid();

  std::u16string removed_keyword;
  if (!GetParam()) {
    // A starter pack failure must not undo a deliberate regional deletion.
    const auto removed = std::ranges::find_if(
        prepopulated, [fallback_id, selected_id](const auto& data) {
          return data->prepopulate_id != fallback_id &&
                 data->prepopulate_id != selected_id;
        });
    ASSERT_NE(removed, prepopulated.end());
    removed_keyword = (*removed)->keyword();
    auto* removed_engine = service_->GetTemplateURLForKeyword(removed_keyword);
    ASSERT_TRUE(removed_engine);
    service_->Remove(removed_engine);
  }

  TemplateURLData custom;
  custom.SetShortName(u"Custom");
  custom.SetKeyword(u"custom");
  custom.SetURL("https://custom.example/?q={searchTerms}");
  ASSERT_TRUE(service_->Add(std::make_unique<TemplateURL>(custom)));
  MakeHashesUndecryptable(GetParam() ? EngineSet::kAllBuiltins
                                     : EngineSet::kStarterPack);
  base::HistogramTester histograms;
  LoadService();
  histograms.ExpectBucketCount(
      "Search.KeywordTable.HashValidationStatus", /*kDecryptFailed=*/1,
      template_url_starter_pack_data::GetStarterPackEngines().size() +
          (GetParam() ? prepopulated.size() : 0));
  ExpectStarterPackEngines();
  if (GetParam()) {
    ExpectRegionalEngines();
  } else {
    EXPECT_FALSE(service_->GetTemplateURLForKeyword(removed_keyword));
  }
  ASSERT_TRUE(service_->GetDefaultSearchProvider());
  EXPECT_EQ(service_->GetDefaultSearchProvider()->prepopulate_id(),
            selected_id);
  EXPECT_EQ(service_->GetDefaultSearchProvider()->sync_guid(), selected_guid);
  EXPECT_EQ(service_->GetDefaultSearchManager()->GetDefaultSearchEngineSource(),
            DefaultSearchManager::FROM_USER);
  const auto* custom_engine =
      service_->GetTemplateURLForKeyword(custom.keyword());
  ASSERT_TRUE(custom_engine);
  EXPECT_EQ(custom_engine->url(), custom.url());
  EXPECT_EQ(custom_engine->sync_guid(), custom.sync_guid);

  // Flush the reconstructed rows and verify that neither failure signal sticks.
  const auto persisted = ReadKeywords();
  ASSERT_TRUE(persisted);
  EXPECT_FALSE(persisted->had_prepopulated_decryption_failure);
  EXPECT_FALSE(persisted->had_starter_pack_decryption_failure);
  EXPECT_EQ(persisted->metadata.starter_pack_version,
            template_url_starter_pack_data::GetDataVersion());
  service_->Shutdown();
  service_.reset();
  LoadService();
  ExpectStarterPackEngines();
  if (!GetParam()) {
    EXPECT_FALSE(service_->GetTemplateURLForKeyword(removed_keyword));
  }
}

TEST_F(TemplateURLServiceDecryptionLoadTest,
       CustomDecryptionFailureDoesNotReconcileBuiltinEngines) {
  const auto prepopulated =
      search_engines_test_environment_.prepopulate_data_resolver()
          .GetPrepopulatedEngines();
  ASSERT_GT(prepopulated.size(), 1u);
  const int default_id = service_->GetDefaultSearchProvider()->prepopulate_id();
  const auto removed =
      std::ranges::find_if(prepopulated, [default_id](const auto& data) {
        return data->prepopulate_id != default_id;
      });
  ASSERT_NE(removed, prepopulated.end());
  auto* removed_engine =
      service_->GetTemplateURLForKeyword((*removed)->keyword());
  ASSERT_TRUE(removed_engine);
  service_->Remove(removed_engine);
  const auto starter_pack =
      template_url_starter_pack_data::GetStarterPackEngines();
  ASSERT_FALSE(starter_pack.empty());
  const auto removed_starter_keyword = starter_pack.front()->keyword();
  auto* removed_starter =
      service_->GetTemplateURLForKeyword(removed_starter_keyword);
  ASSERT_TRUE(removed_starter);
  service_->Remove(removed_starter);

  TemplateURLData custom;
  custom.SetKeyword(u"custom");
  custom.SetURL("https://custom.example/?q={searchTerms}");
  ASSERT_TRUE(service_->Add(std::make_unique<TemplateURL>(custom)));
  MakeHashesUndecryptable(EngineSet::kCustom);

  base::HistogramTester histograms;
  LoadService();
  histograms.ExpectBucketCount("Search.KeywordTable.HashValidationStatus",
                               /*kDecryptFailed=*/1, 1);
  EXPECT_FALSE(service_->GetTemplateURLForKeyword(custom.keyword()));
  // A custom row's decryption failure must not restore deliberately removed
  // built-in engines when the metadata is current.
  EXPECT_FALSE(service_->GetTemplateURLForKeyword((*removed)->keyword()));
  EXPECT_FALSE(service_->GetTemplateURLForKeyword(removed_starter_keyword));
  const auto urls = service_->GetTemplateURLs();
  EXPECT_EQ(prepopulated.size() - 1, static_cast<size_t>(std::ranges::count_if(
                                         urls, [](const TemplateURL* url) {
                                           return url->prepopulate_id() > 0;
                                         })));
}

TEST_P(TemplateURLServiceDecryptionLoadTest,
       RecoveryPreservesDefaultSearchOwnership) {
  auto& env = search_engines_test_environment_;
  const auto prepopulated =
      env.prepopulate_data_resolver().GetPrepopulatedEngines();
  ASSERT_GT(prepopulated.size(), 1u);
  const int fallback_id =
      env.prepopulate_data_resolver().GetFallbackSearch()->prepopulate_id;
  const auto selected =
      std::ranges::find_if(prepopulated, [fallback_id](const auto& data) {
        return data->prepopulate_id != fallback_id;
      });
  ASSERT_NE(selected, prepopulated.end());
  const int selected_id = (*selected)->prepopulate_id;
  auto* selected_engine =
      service_->GetTemplateURLForKeyword((*selected)->keyword());
  ASSERT_TRUE(selected_engine);
  service_->SetUserSelectedDefaultSearchProvider(selected_engine);
  const std::string selected_guid = selected_engine->sync_guid();

  TemplateURLData custom;
  custom.SetShortName(u"Custom");
  custom.SetKeyword(u"custom");
  custom.SetURL("https://custom.example/?q={searchTerms}");
  custom.safe_for_autoreplace = false;
  ASSERT_TRUE(service_->Add(std::make_unique<TemplateURL>(custom)));
  MakeHashesUndecryptable(EngineSet::kPrepopulated);

  if (!GetParam()) {
    // Model the result of the separate tracked-pref reset path. Keyword
    // recovery must not resurrect a selection that prefs no longer trust.
    env.pref_service().ClearPref(
        DefaultSearchManager::kDefaultSearchProviderDataPrefName);
    env.pref_service().ClearPref(
        DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName);
    env.pref_service().ClearPref(prefs::kDefaultSearchProviderGUID);
  }

  base::HistogramTester histograms;
  LoadService();
  histograms.ExpectBucketCount("Search.KeywordTable.HashValidationStatus",
                               /*kDecryptFailed=*/1, prepopulated.size());
  ExpectRegionalEngines();
  ASSERT_TRUE(service_->GetDefaultSearchProvider());
  EXPECT_EQ(GetParam() ? selected_id : fallback_id,
            service_->GetDefaultSearchProvider()->prepopulate_id());
  EXPECT_EQ(
      GetParam() ? DefaultSearchManager::FROM_USER
                 : DefaultSearchManager::FROM_FALLBACK,
      service_->GetDefaultSearchManager()->GetDefaultSearchEngineSource());
  if (GetParam()) {
    EXPECT_EQ(selected_guid, service_->GetDefaultSearchProvider()->sync_guid());
  }
  auto* restored_custom = service_->GetTemplateURLForKeyword(custom.keyword());
  ASSERT_TRUE(restored_custom);
  EXPECT_EQ(custom.url(), restored_custom->url());
  EXPECT_EQ(custom.sync_guid, restored_custom->sync_guid());

  // Reconstructed engines were persisted and survive a normal subsequent load.
  service_->Shutdown();
  service_.reset();
  LoadService();
  ExpectRegionalEngines();
}

INSTANTIATE_TEST_SUITE_P(,
                         TemplateURLServiceDecryptionLoadTest,
                         testing::Bool());
#endif  // BUILDFLAG(IS_WIN)

}  // namespace
