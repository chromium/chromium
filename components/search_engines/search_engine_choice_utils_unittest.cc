// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"

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
#include "testing/gtest/include/gtest/gtest.h"

namespace search_engines {

class SearchEngineChoiceUtilsTest : public ::testing::Test {
 public:
  SearchEngineChoiceUtilsTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        switches::kSearchEngineChoiceTrigger,
        {{switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.name,
          "false"}});

    TemplateURLPrepopulateData::RegisterProfilePrefs(pref_service_.registry());
  }

  ~SearchEngineChoiceUtilsTest() override = default;

  PrefService* pref_service() { return &pref_service_; }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  base::HistogramTester histogram_tester_;

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  base::test::ScopedFeatureList feature_list_;
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
}

TEST_F(SearchEngineChoiceUtilsTest, IsChoiceScreenFlagEnabled) {
  feature_list()->Reset();
  feature_list()->InitAndDisableFeature(switches::kSearchEngineChoiceTrigger);

  EXPECT_FALSE(IsChoiceScreenFlagEnabled(ChoicePromo::kAny));
  EXPECT_FALSE(IsChoiceScreenFlagEnabled(ChoicePromo::kFre));
  EXPECT_FALSE(IsChoiceScreenFlagEnabled(ChoicePromo::kDialog));

  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTrigger,
      {{switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.name,
        "false"}});

  EXPECT_TRUE(IsChoiceScreenFlagEnabled(ChoicePromo::kAny));
  EXPECT_TRUE(IsChoiceScreenFlagEnabled(ChoicePromo::kFre));
  EXPECT_TRUE(IsChoiceScreenFlagEnabled(ChoicePromo::kDialog));

  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTrigger,
      {{switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.name,
        "true"}});

  EXPECT_TRUE(IsChoiceScreenFlagEnabled(ChoicePromo::kAny));
  EXPECT_TRUE(IsChoiceScreenFlagEnabled(ChoicePromo::kFre));
#if BUILDFLAG(IS_IOS)
  EXPECT_FALSE(IsChoiceScreenFlagEnabled(ChoicePromo::kDialog));
#else
  EXPECT_TRUE(IsChoiceScreenFlagEnabled(ChoicePromo::kDialog));
#endif
}

}  // namespace search_engines
