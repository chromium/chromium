// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"

#include <memory>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/to_vector.h"
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
#include "components/regional_capabilities/program_settings.h"
#include "components/regional_capabilities/regional_capabilities_prefs.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

using ::country_codes::CountryId;

namespace search_engines {

namespace {
std::vector<std::unique_ptr<TemplateURL>> ToOwnedTemplateURLs(
    const std::vector<const TemplateURLPrepopulateData::PrepopulatedEngine*>&
        engines) {
  return base::ToVector(
      engines,
      [](const TemplateURLPrepopulateData::PrepopulatedEngine* engine) {
        auto data = TemplateURLDataFromPrepopulatedEngine(*engine);
        return std::make_unique<TemplateURL>(*data);
      });
}
}  // namespace

const CountryId kFranceCountryId = CountryId("FR");

class SearchEngineChoiceUtilsTest : public ::testing::Test {
 public:
  SearchEngineChoiceUtilsTest() {
    TemplateURLPrepopulateData::RegisterProfilePrefs(pref_service_.registry());
    regional_capabilities::prefs::RegisterProfilePrefs(
        pref_service_.registry());
    SearchEngineChoiceService::RegisterProfilePrefs(pref_service_.registry());
  }

  ~SearchEngineChoiceUtilsTest() override = default;

  PrefService* pref_service() { return &pref_service_; }
  base::HistogramTester histogram_tester_;

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

TEST_F(SearchEngineChoiceUtilsTest, ChoiceScreenDisplayState_ToDict) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_QWANT, SEARCH_ENGINE_DUCKDUCKGO,
                          SEARCH_ENGINE_GOOGLE},
      /*country_id=*/kFranceCountryId,
      /*is_current_default_search_presented=*/false,
      /*includes_non_regional_set_engine=*/false,
      /*selected_engine_index=*/1);

  base::Value::Dict dict = display_state.ToDict();
  EXPECT_THAT(
      *dict.FindList("search_engines"),
      testing::ElementsAre(SEARCH_ENGINE_QWANT, SEARCH_ENGINE_DUCKDUCKGO,
                           SEARCH_ENGINE_GOOGLE));
  EXPECT_EQ(dict.FindInt("country_id"), kFranceCountryId.Serialize());
  EXPECT_EQ(dict.FindBool("list_is_modified_by_current_default"), std::nullopt);
  EXPECT_EQ(dict.FindInt("selected_engine_index"), 1);
}

TEST_F(SearchEngineChoiceUtilsTest,
       ChoiceScreenDisplayState_ToDict_WithoutSelection) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_QWANT, SEARCH_ENGINE_DUCKDUCKGO,
                          SEARCH_ENGINE_GOOGLE},
      /*country_id=*/kFranceCountryId,
      /*is_current_default_search_presented=*/false,
      /*includes_non_regional_set_engine=*/false);

  base::Value::Dict dict = display_state.ToDict();
  EXPECT_THAT(
      *dict.FindList("search_engines"),
      testing::ElementsAre(SEARCH_ENGINE_QWANT, SEARCH_ENGINE_DUCKDUCKGO,
                           SEARCH_ENGINE_GOOGLE));
  EXPECT_EQ(dict.FindInt("country_id"), kFranceCountryId.Serialize());
  EXPECT_EQ(dict.FindBool("list_is_modified_by_current_default"), std::nullopt);
  EXPECT_FALSE(dict.contains("selected_engine_index"));
}

TEST_F(SearchEngineChoiceUtilsTest, ChoiceScreenDisplayState_FromDict) {
  base::Value::Dict dict;
  dict.Set("country_id", kFranceCountryId.Serialize());
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

  dict.Set("country_id", kFranceCountryId.Serialize());
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

TEST_F(SearchEngineChoiceUtilsTest, ChoiceScreenDisplayStateConstuction) {
  SearchTermsData search_terms_data;

  auto owned_template_urls = ToOwnedTemplateURLs(
      regional_capabilities::GetDefaultPrepopulatedEngines());
  ChoiceScreenData choice_screen_data(std::move(owned_template_urls),
                                      /* current_default_to_highlight=*/nullptr,
                                      CountryId(), search_terms_data);

  EXPECT_FALSE(
      choice_screen_data.display_state().is_current_default_search_presented);
  EXPECT_FALSE(
      choice_screen_data.display_state().includes_non_regional_set_engine);
}

TEST_F(SearchEngineChoiceUtilsTest,
       ChoiceScreenDisplayStateConstuction_DseHighlightProperties) {
  SearchTermsData search_terms_data;
  auto owned_template_urls = ToOwnedTemplateURLs(
      regional_capabilities::GetDefaultPrepopulatedEngines());
  const TemplateURL* current_default_to_highlight =
      owned_template_urls[0].get();

  ChoiceScreenData choice_screen_data(std::move(owned_template_urls),
                                      current_default_to_highlight, CountryId(),
                                      search_terms_data);

  EXPECT_TRUE(
      choice_screen_data.display_state().is_current_default_search_presented);
  EXPECT_FALSE(
      choice_screen_data.display_state().includes_non_regional_set_engine);
}

TEST_F(SearchEngineChoiceUtilsTest,
       ChoiceScreenDisplayStateConstuction_DseHighlightNonRegional) {
  SearchTermsData search_terms_data;

  auto owned_template_urls = ToOwnedTemplateURLs(
      regional_capabilities::GetDefaultPrepopulatedEngines());

  TemplateURLData template_url_data;
  template_url_data.id = 0;
  template_url_data.SetKeyword(u"custom");
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  auto current_default_to_highlight =
      std::make_unique<TemplateURL>(template_url_data);

  ChoiceScreenData choice_screen_data(std::move(owned_template_urls),
                                      current_default_to_highlight.get(),
                                      CountryId(), search_terms_data);

  EXPECT_TRUE(
      choice_screen_data.display_state().is_current_default_search_presented);
  EXPECT_TRUE(
      choice_screen_data.display_state().includes_non_regional_set_engine);
}

TEST_F(SearchEngineChoiceUtilsTest, GetChoiceCompletionMetadata_Success) {
  const base::Time now = base::Time::Now();
  const base::Version version = version_info::GetVersion();

  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      now.ToDeltaSinceWindowsEpoch().InSeconds());
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      version.GetString());
  pref_service()->SetInteger(
      prefs::kDefaultSearchProviderChoiceScreenCompletionProgram,
      regional_capabilities::SerializeProgram(
          regional_capabilities::Program::kTaiyaki));

  base::expected<ChoiceCompletionMetadata, ChoiceCompletionMetadata::ParseError>
      metadata = GetChoiceCompletionMetadata(*pref_service());

  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->timestamp.ToDeltaSinceWindowsEpoch().InSeconds(),
            now.ToDeltaSinceWindowsEpoch().InSeconds());
  EXPECT_EQ(metadata->version, version);
  EXPECT_EQ(metadata->serialized_program,
            regional_capabilities::SerializeProgram(
                regional_capabilities::Program::kTaiyaki));
}

TEST_F(SearchEngineChoiceUtilsTest,
       GetChoiceCompletionMetadata_Success_Legacy) {
  const base::Time now = base::Time::Now();
  const base::Version version = version_info::GetVersion();

  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      now.ToDeltaSinceWindowsEpoch().InSeconds());
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      version.GetString());

  base::expected<ChoiceCompletionMetadata, ChoiceCompletionMetadata::ParseError>
      metadata = GetChoiceCompletionMetadata(*pref_service());

  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->timestamp.ToDeltaSinceWindowsEpoch().InSeconds(),
            now.ToDeltaSinceWindowsEpoch().InSeconds());
  EXPECT_EQ(metadata->version, version);
  EXPECT_EQ(metadata->serialized_program,
            regional_capabilities::SerializeProgram(
                regional_capabilities::Program::kWaffle));
}

TEST_F(SearchEngineChoiceUtilsTest, GetChoiceCompletionMetadata_Error_Absent) {
  base::expected<ChoiceCompletionMetadata, ChoiceCompletionMetadata::ParseError>
      metadata = GetChoiceCompletionMetadata(*pref_service());
  EXPECT_FALSE(metadata.has_value());
  EXPECT_EQ(metadata.error(), ChoiceCompletionMetadata::ParseError::kAbsent);
}

TEST_F(SearchEngineChoiceUtilsTest,
       GetChoiceCompletionMetadata_Error_MissingVersion) {
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());

  base::expected<ChoiceCompletionMetadata, ChoiceCompletionMetadata::ParseError>
      metadata = GetChoiceCompletionMetadata(*pref_service());
  EXPECT_FALSE(metadata.has_value());
  EXPECT_EQ(metadata.error(),
            ChoiceCompletionMetadata::ParseError::kMissingVersion);
}

TEST_F(SearchEngineChoiceUtilsTest,
       GetChoiceCompletionMetadata_Error_InvalidVersion) {
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion, "invalid");

  base::expected<ChoiceCompletionMetadata, ChoiceCompletionMetadata::ParseError>
      metadata = GetChoiceCompletionMetadata(*pref_service());
  EXPECT_FALSE(metadata.has_value());
  EXPECT_EQ(metadata.error(),
            ChoiceCompletionMetadata::ParseError::kInvalidVersion);
}

TEST_F(SearchEngineChoiceUtilsTest,
       GetChoiceCompletionMetadata_Error_MissingTimestamp) {
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      version_info::GetVersion().GetString());
  base::expected<ChoiceCompletionMetadata, ChoiceCompletionMetadata::ParseError>
      metadata = GetChoiceCompletionMetadata(*pref_service());
  EXPECT_FALSE(metadata.has_value());
  EXPECT_EQ(metadata.error(),
            ChoiceCompletionMetadata::ParseError::kMissingTimestamp);
}

TEST_F(SearchEngineChoiceUtilsTest,
       GetChoiceCompletionMetadata_Error_NullTimestamp) {
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp, 0);
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      version_info::GetVersion().GetString());
  base::expected<ChoiceCompletionMetadata, ChoiceCompletionMetadata::ParseError>
      metadata = GetChoiceCompletionMetadata(*pref_service());
  EXPECT_FALSE(metadata.has_value());
  EXPECT_EQ(metadata.error(),
            ChoiceCompletionMetadata::ParseError::kNullTimestamp);
}

TEST_F(SearchEngineChoiceUtilsTest,
       GetChoiceCompletionMetadata_Error_InvalidProgram) {
  const base::Time now = base::Time::Now();
  const base::Version version = version_info::GetVersion();

  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      now.ToDeltaSinceWindowsEpoch().InSeconds());
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      version.GetString());
  pref_service()->SetInteger(
      prefs::kDefaultSearchProviderChoiceScreenCompletionProgram, -1);

  base::expected<ChoiceCompletionMetadata, ChoiceCompletionMetadata::ParseError>
      metadata = GetChoiceCompletionMetadata(*pref_service());
  EXPECT_FALSE(metadata.has_value());
  EXPECT_EQ(metadata.error(),
            ChoiceCompletionMetadata::ParseError::kInvalidProgram);
}

}  // namespace search_engines
