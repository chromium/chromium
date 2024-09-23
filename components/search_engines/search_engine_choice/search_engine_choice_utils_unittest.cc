// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"

#include <memory>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/eea_countries_ids.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace search_engines {

const int kFranceCountryId = country_codes::CountryStringToCountryID("FR");

class SearchEngineChoiceUtilsTest : public ::testing::Test {
 public:
  SearchEngineChoiceUtilsTest() {
    TemplateURLPrepopulateData::RegisterProfilePrefs(pref_service_.registry());
  }

  ~SearchEngineChoiceUtilsTest() override = default;

  PrefService* pref_service() { return &pref_service_; }
  base::HistogramTester histogram_tester_;

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<TemplateURLService> template_url_service_;
};

// Sanity check the list.
TEST_F(SearchEngineChoiceUtilsTest, IsEeaChoiceCountry) {
  using country_codes::CountryCharsToCountryID;
  using search_engines::IsEeaChoiceCountry;

  EXPECT_TRUE(IsEeaChoiceCountry(CountryCharsToCountryID('D', 'E')));
  EXPECT_TRUE(IsEeaChoiceCountry(CountryCharsToCountryID('F', 'R')));
  EXPECT_TRUE(IsEeaChoiceCountry(CountryCharsToCountryID('V', 'A')));
  EXPECT_TRUE(IsEeaChoiceCountry(CountryCharsToCountryID('A', 'X')));
  EXPECT_TRUE(IsEeaChoiceCountry(CountryCharsToCountryID('Y', 'T')));
  EXPECT_TRUE(IsEeaChoiceCountry(CountryCharsToCountryID('N', 'C')));

  EXPECT_FALSE(IsEeaChoiceCountry(CountryCharsToCountryID('U', 'S')));

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry,
      switches::kDefaultListCountryOverride);
  EXPECT_TRUE(IsEeaChoiceCountry(CountryCharsToCountryID('U', 'S')));

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, switches::kEeaListCountryOverride);
  EXPECT_TRUE(IsEeaChoiceCountry(CountryCharsToCountryID('U', 'S')));
}

TEST_F(SearchEngineChoiceUtilsTest, ChoiceScreenDisplayState_ToDict) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_QWANT, SEARCH_ENGINE_DUCKDUCKGO,
                          SEARCH_ENGINE_GOOGLE},
      /*country_id=*/kFranceCountryId,
      /*selected_engine_index=*/1);

  base::Value::Dict dict = display_state.ToDict();
  EXPECT_THAT(
      *dict.FindList("search_engines"),
      testing::ElementsAre(SEARCH_ENGINE_QWANT, SEARCH_ENGINE_DUCKDUCKGO,
                           SEARCH_ENGINE_GOOGLE));
  EXPECT_EQ(dict.FindInt("country_id"), kFranceCountryId);
  EXPECT_EQ(dict.FindBool("list_is_modified_by_current_default"), std::nullopt);
  EXPECT_EQ(dict.FindInt("selected_engine_index"), 1);
}

TEST_F(SearchEngineChoiceUtilsTest,
       ChoiceScreenDisplayState_ToDict_WithoutSelection) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_QWANT, SEARCH_ENGINE_DUCKDUCKGO,
                          SEARCH_ENGINE_GOOGLE},
      /*country_id=*/kFranceCountryId);

  base::Value::Dict dict = display_state.ToDict();
  EXPECT_THAT(
      *dict.FindList("search_engines"),
      testing::ElementsAre(SEARCH_ENGINE_QWANT, SEARCH_ENGINE_DUCKDUCKGO,
                           SEARCH_ENGINE_GOOGLE));
  EXPECT_EQ(dict.FindInt("country_id"), kFranceCountryId);
  EXPECT_EQ(dict.FindBool("list_is_modified_by_current_default"), std::nullopt);
  EXPECT_FALSE(dict.contains("selected_engine_index"));
}

TEST_F(SearchEngineChoiceUtilsTest, ChoiceScreenDisplayState_FromDict) {
  base::Value::Dict dict;
  dict.Set("country_id", kFranceCountryId);
  dict.Set("selected_engine_index", 0);
  auto* search_engines = dict.EnsureList("search_engines");
  search_engines->Append(SEARCH_ENGINE_DUCKDUCKGO);
  search_engines->Append(SEARCH_ENGINE_GOOGLE);
  search_engines->Append(SEARCH_ENGINE_BING);

  std::optional<ChoiceScreenDisplayState> display_state =
      ChoiceScreenDisplayState::FromDict(dict);
  EXPECT_TRUE(display_state.has_value());
  EXPECT_THAT(display_state->search_engines,
              testing::ElementsAre(SEARCH_ENGINE_DUCKDUCKGO,
                                   SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING));
  EXPECT_EQ(display_state->country_id, kFranceCountryId);
  EXPECT_TRUE(display_state->selected_engine_index.has_value());
  EXPECT_EQ(display_state->selected_engine_index.value(), 0);
}

TEST_F(SearchEngineChoiceUtilsTest, ChoiceScreenDisplayState_FromDict_Errors) {
  base::Value::Dict dict;
  EXPECT_FALSE(ChoiceScreenDisplayState::FromDict(dict).has_value());

  dict.Set("country_id", kFranceCountryId);
  EXPECT_FALSE(ChoiceScreenDisplayState::FromDict(dict).has_value());

  auto* search_engines = dict.EnsureList("search_engines");
  search_engines->Append(SEARCH_ENGINE_DUCKDUCKGO);
  search_engines->Append(SEARCH_ENGINE_GOOGLE);
  search_engines->Append(SEARCH_ENGINE_BING);
  EXPECT_TRUE(ChoiceScreenDisplayState::FromDict(dict).has_value());

  // Optional fields
  dict.Set("list_is_modified_by_current_default", false);
  EXPECT_TRUE(ChoiceScreenDisplayState::FromDict(dict).has_value());

  dict.Set("selected_engine_index", 0);
  EXPECT_TRUE(ChoiceScreenDisplayState::FromDict(dict).has_value());

  // Special case: makes the dictionary invalid.
  dict.Set("list_is_modified_by_current_default", true);
  EXPECT_FALSE(ChoiceScreenDisplayState::FromDict(dict).has_value());
}

}  // namespace search_engines
