// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/country_codes/country_codes.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_test_utils.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_metrics_service_accessor.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service_test_base.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

using ::country_codes::CountryId;
using ::search_engines::RepromptResult;
using ::search_engines::SearchEngineChoiceWipeReason;
using ::testing::NiceMock;

namespace search_engines {
namespace {

const CountryId kBelgiumCountryId = CountryId("BE");
const CountryId kUsaCountryId = CountryId("US");

TemplateURL::OwnedTemplateURLVector
OwnedTemplateURLVectorFromPrepopulatedEngines(
    const std::vector<const TemplateURLPrepopulateData::PrepopulatedEngine*>&
        engines) {
  TemplateURL::OwnedTemplateURLVector result;
  for (const TemplateURLPrepopulateData::PrepopulatedEngine* engine : engines) {
    result.push_back(std::make_unique<TemplateURL>(
        *TemplateURLDataFromPrepopulatedEngine(*engine)));
  }
  return result;
}

}  // namespace

class SearchEngineChoiceServiceTest : public SearchEngineChoiceServiceTestBase {
};

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(SearchEngineChoiceServiceTest, GuestSessionDsePropagation) {
  InitService({.force_reset = true,
               .is_profile_eligible_for_dse_guest_propagation = true});

  EXPECT_FALSE(local_state().HasPrefPath(
      prefs::kDefaultSearchProviderGuestModePrepopulatedId));
  EXPECT_FALSE(search_engine_choice_service()
                   .GetSavedSearchEngineBetweenGuestSessions()
                   .has_value());

  constexpr int prepopulated_id = 1;
  search_engine_choice_service().SetSavedSearchEngineBetweenGuestSessions(
      prepopulated_id);
  EXPECT_EQ(local_state().GetInt64(
                prefs::kDefaultSearchProviderGuestModePrepopulatedId),
            prepopulated_id);
  EXPECT_EQ(
      search_engine_choice_service().GetSavedSearchEngineBetweenGuestSessions(),
      prepopulated_id);

  // The guest DSE is not propagated to services that are not guest profiles.
  InitService({.force_reset = true,
               .is_profile_eligible_for_dse_guest_propagation = false});
  EXPECT_FALSE(search_engine_choice_service()
                   .GetSavedSearchEngineBetweenGuestSessions()
                   .has_value());

  // A new guest service propagates the DSE.
  InitService({.force_reset = true,
               .is_profile_eligible_for_dse_guest_propagation = true});
  EXPECT_EQ(
      search_engine_choice_service().GetSavedSearchEngineBetweenGuestSessions(),
      prepopulated_id);
}

TEST_F(SearchEngineChoiceServiceTest,
       UpdatesDefaultSearchEngineManagerForGuestMode) {
  InitService({.force_reset = true,
               .is_profile_eligible_for_dse_guest_propagation = true});

  DefaultSearchManager manager(pref_service(), &search_engine_choice_service(),
                               prepopulate_data_resolver(),
                               base::NullCallback());
  DefaultSearchManager::Source source;
  EXPECT_EQ(manager.GetDefaultSearchEngine(&source)->prepopulate_id, 1);
  EXPECT_EQ(source, DefaultSearchManager::Source::FROM_FALLBACK);

  // Test the changes in the SearchEngineChoiceService propagate to the DSE
  // manager.
  search_engine_choice_service().SetSavedSearchEngineBetweenGuestSessions(2);
  EXPECT_EQ(manager.GetDefaultSearchEngine(&source)->prepopulate_id, 2);
  EXPECT_EQ(source, DefaultSearchManager::Source::FROM_FALLBACK);
}
#endif

TEST_F(SearchEngineChoiceServiceTest, RecordChoiceMade) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  // Test that the choice is not recorded for countries outside the EEA region.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "US");

  const TemplateURL* default_search_engine =
      template_url_service().GetDefaultSearchProvider();
  EXPECT_EQ(default_search_engine->prepopulate_id(),
            TemplateURLPrepopulateData::google.id);

  search_engine_choice_service().RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());

  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 0);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 0);
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));

  // Revert to an EEA region country.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, kBelgiumCountryId.CountryCode());

  // Test that the choice is recorded if it wasn't previously done.
  search_engine_choice_service().RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);

  EXPECT_NEAR(pref_service()->GetInt64(
                  prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
              base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds(),
              /*abs_error=*/2);
  EXPECT_EQ(pref_service()->GetString(
                prefs::kDefaultSearchProviderChoiceScreenCompletionVersion),
            version_info::GetVersionNumber());

  // Set the pref to 5 so that we can know if it gets modified.
  const int kModifiedTimestamp = 5;
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      kModifiedTimestamp);

  // Test that the choice is not recorded if it was previously done.
  search_engine_choice_service().RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());
  EXPECT_EQ(pref_service()->GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
            kModifiedTimestamp);

  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);
}

TEST_F(SearchEngineChoiceServiceTest, RecordChoiceMade_ByLocation) {
  // Configure to an EEA region country.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, kBelgiumCountryId.CountryCode());
  EXPECT_EQ(template_url_service().GetDefaultSearchProvider()->prepopulate_id(),
            TemplateURLPrepopulateData::google.id);

  auto locations = {ChoiceMadeLocation::kChoiceScreen,
                    ChoiceMadeLocation::kSearchSettings,
                    ChoiceMadeLocation::kSearchEngineSettings};
  int expected_v1_records = 0;
  int expected_v2_records = 0;
  for (const ChoiceMadeLocation& choice_location : locations) {
    switch (choice_location) {
      case ChoiceMadeLocation::kChoiceScreen:
        // For the choice screen, the choice should be recorded in the both
        // histograms.
        expected_v1_records += 1;
        expected_v2_records += 1;
        break;

      case ChoiceMadeLocation::kSearchSettings:
      case ChoiceMadeLocation::kSearchEngineSettings:
        // For other locations, the choice should be recorded only in the legacy
        // histogram.
        expected_v1_records += 1;
        break;
      case ChoiceMadeLocation::kOther:
        NOTREACHED();  // Not an allowed value for `RecordChoiceMade()`.
    }

    search_engine_choice_service().RecordChoiceMade(choice_location,
                                                    &template_url_service());
    histogram_tester_.ExpectBucketCount(
        search_engines::
            kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
        SearchEngineType::SEARCH_ENGINE_GOOGLE, expected_v1_records);
    histogram_tester_.ExpectUniqueSample(
        search_engines::
            kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
        SearchEngineType::SEARCH_ENGINE_GOOGLE, expected_v2_records);
    WipeSearchEngineChoicePrefs(*pref_service(),
                                SearchEngineChoiceWipeReason::kCommandLineFlag);
  }
}

TEST_F(SearchEngineChoiceServiceTest, RecordChoiceMade_DistributionCustom) {
  // A distribution custom search engine will have a `prepopulate_id` > 1000.
  const int kDistributionCustomSearchEnginePrepopulateId = 1001;
  TemplateURLData template_url_data;
  template_url_data.prepopulate_id =
      kDistributionCustomSearchEnginePrepopulateId;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  // Revert to an EEA region country.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, kBelgiumCountryId.CountryCode());

  // Test that the choice is recorded if it wasn't previously done.
  search_engine_choice_service().RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_OTHER, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
      SearchEngineType::SEARCH_ENGINE_OTHER, 1);

  EXPECT_NEAR(pref_service()->GetInt64(
                  prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
              base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds(),
              /*abs_error=*/2);
  EXPECT_EQ(pref_service()->GetString(
                prefs::kDefaultSearchProviderChoiceScreenCompletionVersion),
            version_info::GetVersionNumber());
}

TEST_F(SearchEngineChoiceServiceTest, RecordChoiceMade_RemovedPrepopulated) {
  // We don't have a prepopulated search engine with ID 20, but at some point
  // in the past, we did. So some profiles might still have it.
  const int kRemovedSearchEnginePrepopulateId = 20;
  TemplateURLData template_url_data;
  template_url_data.prepopulate_id = kRemovedSearchEnginePrepopulateId;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  // Revert to an EEA region country.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, kBelgiumCountryId.CountryCode());

  // Test that the choice is recorded if it wasn't previously done.
  search_engine_choice_service().RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_OTHER, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
      SearchEngineType::SEARCH_ENGINE_OTHER, 1);

  EXPECT_NEAR(pref_service()->GetInt64(
                  prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
              base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds(),
              /*abs_error=*/2);
  EXPECT_EQ(pref_service()->GetString(
                prefs::kDefaultSearchProviderChoiceScreenCompletionVersion),
            version_info::GetVersionNumber());
}

TEST_F(SearchEngineChoiceServiceTest, MaybeRecordChoiceScreenDisplayState) {
  InitService({.variation_country_id = kBelgiumCountryId,
               .client_country_id = kBelgiumCountryId,
               .force_reset = true});
  ChoiceScreenData choice_screen_data(
      OwnedTemplateURLVectorFromPrepopulatedEngines(
          {&TemplateURLPrepopulateData::google,
           &TemplateURLPrepopulateData::bing,
           &TemplateURLPrepopulateData::yahoo}),
      kBelgiumCountryId, SearchTermsData());
  ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
  display_state.selected_engine_index = 2;

  base::HistogramTester histogram_tester;
  search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
      display_state);

  histogram_tester.ExpectUniqueSample(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 2, 1);
  histogram_tester.ExpectBucketCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, false,
      1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      SEARCH_ENGINE_GOOGLE, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 1),
      SEARCH_ENGINE_BING, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 2),
      SEARCH_ENGINE_YAHOO, 1);

  // There is no search engine shown at index 3, since we have only 3 options.
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 3),
      0);

  // We logged the display state, so we don't need to cache it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_ProfileCountryMismatch) {
  // The actual profile of the country does not matter, we are checking the
  // `ChoiceScreenData` country against the variations country.
  InitService({.variation_country_id = kBelgiumCountryId,
               .client_country_id = kUsaCountryId,
               .force_reset = true});
  ChoiceScreenData choice_screen_data(
      OwnedTemplateURLVectorFromPrepopulatedEngines(
          {&TemplateURLPrepopulateData::google,
           &TemplateURLPrepopulateData::bing,
           &TemplateURLPrepopulateData::yahoo}),
      kBelgiumCountryId, SearchTermsData());
  ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
  display_state.selected_engine_index = 2;

  base::HistogramTester histogram_tester;
  search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
      display_state);

  histogram_tester.ExpectUniqueSample(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 2, 1);
  histogram_tester.ExpectBucketCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, false,
      1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      SEARCH_ENGINE_GOOGLE, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 1),
      SEARCH_ENGINE_BING, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 2),
      SEARCH_ENGINE_YAHOO, 1);

  // There is no search engine shown at index 3, since we have only 3 options.
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 3),
      0);

  // We logged the display state, so we don't need to cache it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_NoopUnsupportedCountry) {
  auto engines = {&TemplateURLPrepopulateData::google,
                  &TemplateURLPrepopulateData::bing,
                  &TemplateURLPrepopulateData::yahoo};
  base::HistogramTester histogram_tester;

  {
    // Unknown country.
    InitService({.force_reset = true});
    ChoiceScreenData choice_screen_data(
        OwnedTemplateURLVectorFromPrepopulatedEngines(engines), CountryId(),
        SearchTermsData());
    ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
    display_state.selected_engine_index = 0;

    search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
        display_state);
  }

  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 0);
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, 0);

  // The choice is coming from a non-eea country and won't be logged, don't
  // cache it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

  {
    // Non-EEA variations country.
    InitService({.variation_country_id = kUsaCountryId, .force_reset = true});
    ChoiceScreenData choice_screen_data(
        OwnedTemplateURLVectorFromPrepopulatedEngines(engines), kUsaCountryId,
        SearchTermsData());
    ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
    display_state.selected_engine_index = 0;
    search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
        display_state);
  }

  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 0);
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, 0);

  // The choice is coming from a non-eea country and won't be logged, don't
  // cache it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_MismatchingCountry) {
  auto engines = {&TemplateURLPrepopulateData::google,
                  &TemplateURLPrepopulateData::bing,
                  &TemplateURLPrepopulateData::yahoo};
  base::HistogramTester histogram_tester;

  // Mismatch between the variations and choice screen data country.
  InitService({.variation_country_id = country_codes::CountryId("DE"),
               .force_reset = true});
  ChoiceScreenData choice_screen_data(
      OwnedTemplateURLVectorFromPrepopulatedEngines(engines), kBelgiumCountryId,
      SearchTermsData());
  ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
  display_state.selected_engine_index = 0;
  search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
      display_state);

  histogram_tester.ExpectBucketCount(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 0, 1);
  histogram_tester.ExpectBucketCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, true, 1);

  // None of the above should have logged the full list of indices.
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 1),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 2),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 3),
      0);

  // The choice screen state should be cached for a next chance later.
  EXPECT_TRUE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

  auto stored_display_state =
      ChoiceScreenDisplayState::FromDict(pref_service()->GetDict(
          prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
  EXPECT_EQ(stored_display_state->search_engines, display_state.search_engines);
  EXPECT_EQ(stored_display_state->country_id, display_state.country_id);
  EXPECT_EQ(stored_display_state->selected_engine_index,
            display_state.selected_engine_index);
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_OnServiceStartup) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

  base::HistogramTester histogram_tester;
  InitService({.variation_country_id = kBelgiumCountryId, .force_reset = true});

  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      SEARCH_ENGINE_GOOGLE, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 1),
      SEARCH_ENGINE_BING, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 2),
      SEARCH_ENGINE_YAHOO, 1);

  // These metrics are expected to have been already logged at the time we
  // cached the screen state.
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 0);
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, 0);

  // The choice screen state should now be cleared.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_OnServiceStartup_CountryMismatch) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

  base::HistogramTester histogram_tester;
  InitService({.force_reset = true});

  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 1),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 2),
      0);

  // These metrics are expected to have been already logged at the time we
  // cached the screen state.
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 0);
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, 0);

  // The choice screen state should stay around.
  EXPECT_TRUE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_OnServiceStartup_ChoicePrefCleared) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());

  base::HistogramTester histogram_tester;
  InitService({.force_reset = true});

  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, 0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 1),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 2),
      0);

  // Choice not marked done, so the service also clear the pending state.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_OnServiceStartup_UmaDisabled) {
  // Disable UMA reporting.
  SearchEngineChoiceMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookup(false);

  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());
  EXPECT_TRUE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

  InitService({.variation_country_id = kBelgiumCountryId, .force_reset = true});
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

  histogram_tester_.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      0);
  histogram_tester_.ExpectTotalCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, 0);
}

// Tests if choice screen completion date is not recorded if last choice date is
// unknown.
TEST_F(SearchEngineChoiceServiceTest, IgnoresChoiceScreenCompletionDateRecord) {
  base::HistogramTester histogram_tester;
  search_engine_choice_service();
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceCompletedOnMonthHistogram, 0);
}

// Tests if choice screen completion date is recorded.
TEST_F(SearchEngineChoiceServiceTest,
       RecordsChoiceScreenCompletionDateHistogram) {
  base::HistogramTester histogram_tester;

  // April 18, 2025, 13:30 Europe/Warsaw. What is specific about this timestamp
  // (in windows epoch seconds) is that in every known timezone,
  // this was April 2025.
  int64_t windows_epoch_timestamp = 13388103000;

  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion, "1.0.0.0");
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      windows_epoch_timestamp);

  search_engine_choice_service();
  histogram_tester.ExpectUniqueSample(
      kSearchEngineChoiceCompletedOnMonthHistogram, 202504, 1);
}

// Test that the user is not reprompted if the reprompt parameter is not a valid
// JSON string.
TEST_F(SearchEngineChoiceServiceTest, NoRepromptForSyntaxError) {
  // Set the reprompt parameters with invalid syntax.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTrigger,
      {{switches::kSearchEngineChoiceTriggerRepromptParams.name, "Foo"}});
  ASSERT_EQ("Foo", switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  // Initialize the preference with some previous choice.
  int64_t kPreviousTimestamp = 1;
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      kPreviousTimestamp);
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion, "1.0.0.0");

  // Trigger the creation of the service, which should check for the reprompt.
  search_engine_choice_service();

  // The user should not be reprompted.
  EXPECT_EQ(kPreviousTimestamp,
            pref_service()->GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceWipeReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptWildcardHistogram, 0);
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceRepromptHistogram,
      RepromptResult::kInvalidDictionary, 1);
}

// Test that the user is not reprompted when no persisted metadata indicates any
// choice-related activity took place.
TEST_F(SearchEngineChoiceServiceTest, NoOpWhenNoChoiceMetadataPresent) {
  ASSERT_EQ(switches::kSearchEngineChoiceNoRepromptString,
            switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  // Trigger the creation of the service, which should check for the reprompt.
  search_engine_choice_service();

  // The user should not be reprompted, no associated histogram recorded.
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceWipeReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptWildcardHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptHistogram, 0);
}

// Test that the user is not reprompted by default.
TEST_F(SearchEngineChoiceServiceTest, NoRepromptByDefault) {
  ASSERT_EQ(switches::kSearchEngineChoiceNoRepromptString,
            switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  // Initialize the preference with some previous choice.
  int64_t kPreviousTimestamp = 1;
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      kPreviousTimestamp);
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion, "1.0.0.0");

  // Trigger the creation of the service, which should check for the reprompt.
  search_engine_choice_service();

  // The user should not be reprompted.
  EXPECT_EQ(kPreviousTimestamp,
            pref_service()->GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceWipeReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptWildcardHistogram, 0);
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceRepromptHistogram,
      RepromptResult::kNoReprompt, 1);
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceRepromptHistogram,
      RepromptResult::kInvalidDictionary, 0);
}

// The user is reprompted if the version preference is missing.
TEST_F(SearchEngineChoiceServiceTest, RepromptForMissingChoiceVersion) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTrigger,
      {{switches::kSearchEngineChoiceTriggerRepromptParams.name, "{}"}});
  ASSERT_EQ("{}", switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  // Initialize the timestamp, but not the version.
  int64_t kPreviousTimestamp = 1;
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      kPreviousTimestamp);

  // Trigger the creation of the service, which should check for the reprompt.
  search_engine_choice_service();

  // The user should be reprompted.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceWipeReasonHistogram,
      SearchEngineChoiceWipeReason::kMissingMetadataVersion, 1);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptWildcardHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptHistogram, 0);
}

// The user is reprompted if the timestamp preference is missing.
TEST_F(SearchEngineChoiceServiceTest, RepromptForMissingTimestamp) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTrigger,
      {{switches::kSearchEngineChoiceTriggerRepromptParams.name, "{}"}});
  ASSERT_EQ("{}", switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  // Initialize the version, but not the timestamp.
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion, "1.0.0.0");

  // Trigger the creation of the service, which should check for the reprompt.
  search_engine_choice_service();

  // The user should be reprompted.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceWipeReasonHistogram,
      SearchEngineChoiceWipeReason::kInvalidMetadata, 1);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptWildcardHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptHistogram, 0);
}

struct DeviceRestoreTestParam {
  std::string test_suffix;
  bool restore_detected_in_current_session;
  bool choice_predates_restore;
  bool is_feature_enabled;
  bool is_invalidation_retroactive;
  bool expect_choice_info_wipe;
};

class SearchEngineChoiceServiceDeviceRestoreTest
    : public SearchEngineChoiceServiceTest,
      public testing::WithParamInterface<DeviceRestoreTestParam> {
 public:
  SearchEngineChoiceServiceDeviceRestoreTest() {
    if (GetParam().is_feature_enabled) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          switches::kInvalidateSearchEngineChoiceOnDeviceRestoreDetection,
          {{switches::kInvalidateChoiceOnRestoreIsRetroactive.name,
            GetParam().is_invalidation_retroactive ? "true" : "false"}});
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          switches::kInvalidateSearchEngineChoiceOnDeviceRestoreDetection);
    }
  }

  static std::string GetTestSuffix(
      const testing::TestParamInfo<DeviceRestoreTestParam>& info) {
    return info.param.test_suffix;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SearchEngineChoiceServiceDeviceRestoreTest,
    ::testing::ValuesIn({
        DeviceRestoreTestParam{.test_suffix = "WipeForPreexistingChoice",
                               .restore_detected_in_current_session = true,
                               .choice_predates_restore = true,
                               .is_feature_enabled = true,
                               .is_invalidation_retroactive = false,
                               .expect_choice_info_wipe = true},
        DeviceRestoreTestParam{.test_suffix = "WipeForRetroactiveDetection",
                               .restore_detected_in_current_session = false,
                               .choice_predates_restore = true,
                               .is_feature_enabled = true,
                               .is_invalidation_retroactive = true,
                               .expect_choice_info_wipe = true},
        DeviceRestoreTestParam{.test_suffix = "NoWipeForLateDetection",
                               .restore_detected_in_current_session = false,
                               .choice_predates_restore = true,
                               .is_feature_enabled = true,
                               .is_invalidation_retroactive = false,
                               .expect_choice_info_wipe = false},
        DeviceRestoreTestParam{.test_suffix = "NoWipeForNewChoice",
                               .restore_detected_in_current_session = true,
                               .choice_predates_restore = false,
                               .is_feature_enabled = true,
                               .is_invalidation_retroactive = false,
                               .expect_choice_info_wipe = false},
        DeviceRestoreTestParam{.test_suffix = "NoWipeForFeatureDisabled",
                               .restore_detected_in_current_session = true,
                               .choice_predates_restore = true,
                               .is_feature_enabled = false,
                               .is_invalidation_retroactive = false,
                               .expect_choice_info_wipe = false},
    }),
    &SearchEngineChoiceServiceDeviceRestoreTest::GetTestSuffix);

TEST_P(SearchEngineChoiceServiceDeviceRestoreTest, RepromptOnRestoreDetection) {
  ASSERT_EQ(switches::kSearchEngineChoiceNoRepromptString,
            switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  SetChoiceCompletionMetadata(*pref_service(),
                              {base::Time::Now(), base::Version("1.0.0.0")});

  // Trigger the creation of the service, which should check for the reprompt.
  InitService({
      .force_reset = true,
      .restore_detected_in_current_session =
          GetParam().restore_detected_in_current_session,
      .choice_predates_restore = GetParam().choice_predates_restore,
  });

  if (GetParam().expect_choice_info_wipe) {
    EXPECT_FALSE(pref_service()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
    histogram_tester_.ExpectUniqueSample(
        search_engines::kSearchEngineChoiceWipeReasonHistogram,
        SearchEngineChoiceWipeReason::kDeviceRestored, 1);
    histogram_tester_.ExpectTotalCount(
        search_engines::kSearchEngineChoiceRepromptHistogram, 0);
  } else {
    EXPECT_TRUE(pref_service()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
    histogram_tester_.ExpectTotalCount(
        search_engines::kSearchEngineChoiceWipeReasonHistogram, 0);
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceRepromptHistogram,
        RepromptResult::kNoReprompt, 1);
  }

  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptWildcardHistogram, 0);
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceRepromptHistogram,
      RepromptResult::kInvalidDictionary, 0);
}

struct RepromptTestParam {
  // Whether the user should be reprompted or not.
  std::optional<SearchEngineChoiceWipeReason> wipe_reason;
  // Internal results of the reprompt computation.
  std::optional<RepromptResult> wildcard_result;
  std::optional<RepromptResult> country_result;
  // Version of the choice.
  const std::string_view choice_version;
  // The reprompt params, as sent by the server.  Use `CURRENT_VERSION` for the
  // current Chrome version, and `FUTURE_VERSION` for a future version.
  const char* param_dict;
};

class SearchEngineChoiceUtilsParamTest
    : public SearchEngineChoiceServiceTest,
      public testing::WithParamInterface<RepromptTestParam> {};

TEST_P(SearchEngineChoiceUtilsParamTest, Reprompt) {
  // Set the reprompt parameters.
  std::string param_dict = base::CollapseWhitespaceASCII(
      GetParam().param_dict, /*trim_sequences_with_line_breaks=*/true);
  base::ReplaceFirstSubstringAfterOffset(&param_dict, /*start_offset=*/0,
                                         "CURRENT_VERSION",
                                         version_info::GetVersionNumber());
  base::Version future_version(
      {version_info::GetMajorVersionNumberAsInt() + 5u, 2, 3, 4});
  ASSERT_TRUE(future_version.IsValid());
  base::ReplaceFirstSubstringAfterOffset(&param_dict, /*start_offset=*/0,
                                         "FUTURE_VERSION",
                                         future_version.GetString());

  std::string feature_param_value = param_dict.empty() ? "{}" : param_dict;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTrigger,
      {{switches::kSearchEngineChoiceTriggerRepromptParams.name,
        feature_param_value}});
  ASSERT_EQ(feature_param_value,
            switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  // Initialize the preference with some previous choice.
  int64_t kPreviousTimestamp = 1;
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      kPreviousTimestamp);
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      GetParam().choice_version);

  // Trigger the creation of the service, which should check for the reprompt.
  search_engine_choice_service();

  if (GetParam().wipe_reason) {
    // User is reprompted, prefs were wiped.
    EXPECT_FALSE(pref_service()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
    EXPECT_FALSE(pref_service()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
    histogram_tester_.ExpectUniqueSample(
        search_engines::kSearchEngineChoiceWipeReasonHistogram,
        *GetParam().wipe_reason, 1);
  } else {
    // User is not reprompted, prefs were not wiped.
    EXPECT_EQ(
        kPreviousTimestamp,
        pref_service()->GetInt64(
            prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
    EXPECT_EQ(GetParam().choice_version,
              pref_service()->GetString(
                  prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
    histogram_tester_.ExpectTotalCount(
        search_engines::kSearchEngineChoiceWipeReasonHistogram, 0);
  }

  int total_reprompt_record_count = 0;
  if (GetParam().wildcard_result) {
    ++total_reprompt_record_count;
    histogram_tester_.ExpectUniqueSample(
        search_engines::kSearchEngineChoiceRepromptWildcardHistogram,
        *GetParam().wildcard_result, 1);
    EXPECT_GE(histogram_tester_.GetBucketCount(
                  search_engines::kSearchEngineChoiceRepromptHistogram,
                  *GetParam().wildcard_result),
              1);
  } else {
    histogram_tester_.ExpectTotalCount(
        search_engines::kSearchEngineChoiceRepromptWildcardHistogram, 0);
  }
  if (GetParam().country_result) {
    ++total_reprompt_record_count;
    histogram_tester_.ExpectUniqueSample(
        search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram,
        *GetParam().country_result, 1);
    EXPECT_GE(histogram_tester_.GetBucketCount(
                  search_engines::kSearchEngineChoiceRepromptHistogram,
                  *GetParam().country_result),
              1);
  } else {
    histogram_tester_.ExpectTotalCount(
        search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram, 0);
  }
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptHistogram,
      total_reprompt_record_count);
}

constexpr RepromptTestParam kRepromptTestParams[] = {
    // Reprompt all countries with the wildcard.
    {SearchEngineChoiceWipeReason::kFinchBasedReprompt,
     RepromptResult::kReprompt, RepromptResult::kNoDictionaryKey, "1.0.0.0",
     R"( {"*":"1.0.0.1"} )"},
    // Reprompt works with all version components.
    {SearchEngineChoiceWipeReason::kFinchBasedReprompt,
     RepromptResult::kReprompt, RepromptResult::kNoDictionaryKey, "1.0.0.100",
     R"( {"*":"1.0.1.0"} )"},
    {SearchEngineChoiceWipeReason::kFinchBasedReprompt,
     RepromptResult::kReprompt, RepromptResult::kNoDictionaryKey, "1.0.200.0",
     R"( {"*":"1.1.0.0"} )"},
    {SearchEngineChoiceWipeReason::kFinchBasedReprompt,
     RepromptResult::kReprompt, RepromptResult::kNoDictionaryKey, "1.300.0.0",
     R"( {"*":"2.0.0.0"} )"},
    {SearchEngineChoiceWipeReason::kFinchBasedReprompt,
     RepromptResult::kReprompt, RepromptResult::kNoDictionaryKey, "10.10.1.1",
     R"( {"*":"30.45.678.9100"} )"},
    // Reprompt a specific country.
    {SearchEngineChoiceWipeReason::kFinchBasedReprompt, std::nullopt,
     RepromptResult::kReprompt, "1.0.0.0", R"( {"BE":"1.0.0.1"} )"},
    // Reprompt for params inclusive of current version
    {SearchEngineChoiceWipeReason::kFinchBasedReprompt, std::nullopt,
     RepromptResult::kReprompt, "1.0.0.0", R"( {"BE":"CURRENT_VERSION"} )"},
    // Reprompt when the choice version is malformed.
    {SearchEngineChoiceWipeReason::kInvalidMetadataVersion, std::nullopt,
     std::nullopt, "Blah", ""},
    // Reprompt when both the country and the wild card are specified, as long
    // as one of them qualifies.
    {SearchEngineChoiceWipeReason::kFinchBasedReprompt, std::nullopt,
     RepromptResult::kReprompt, "1.0.0.0",
     R"( {"*":"1.0.0.1","BE":"1.0.0.1"} )"},
    {SearchEngineChoiceWipeReason::kFinchBasedReprompt, std::nullopt,
     RepromptResult::kReprompt, "1.0.0.0",
     R"( {"*":"FUTURE_VERSION","BE":"1.0.0.1"} )"},
    // Still works with irrelevant parameters for other countries.
    {SearchEngineChoiceWipeReason::kFinchBasedReprompt, std::nullopt,
     RepromptResult::kReprompt, "1.0.0.0",
     R"(
       {
         "FR":"FUTURE_VERSION",
         "INVALID_COUNTRY":"INVALID_VERSION",
         "US":"FUTURE_VERSION",
         "BE":"1.0.0.1"
       } )"},

    // Don't reprompt when the choice was made in the current version.
    {std::nullopt, RepromptResult::kRecentChoice,
     RepromptResult::kNoDictionaryKey, version_info::GetVersionNumber(),
     "{\"*\":\"CURRENT_VERSION\"}"},
    // Don't reprompt when the choice was recent enough.
    {std::nullopt, RepromptResult::kRecentChoice,
     RepromptResult::kNoDictionaryKey, "2.0.0.0", R"( {"*":"1.0.0.1"} )"},
    // Don't reprompt for another country.
    {std::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"FR":"1.0.0.1"} )"},
    {std::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"US":"1.0.0.1"} )"},
    {std::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"XX":"1.0.0.1"} )"},
    {std::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0",
     R"( {"INVALID_COUNTRY":"1.0.0.1"} )"},
    {std::nullopt, std::nullopt, RepromptResult::kChromeTooOld, "1.0.0.0",
     R"( {"FR":"1.0.0.1","BE":"FUTURE_VERSION"} )"},
    // Don't reprompt for future versions.
    {std::nullopt, RepromptResult::kChromeTooOld,
     RepromptResult::kNoDictionaryKey, "1.0.0.0",
     R"( {"*":"FUTURE_VERSION"} )"},
    // Wildcard is overridden by specific country.
    {std::nullopt, std::nullopt, RepromptResult::kChromeTooOld, "1.0.0.0",
     R"( {"*":"1.0.0.1","BE":"FUTURE_VERSION"} )"},
    // Combination of right version for wrong country and wrong version for
    // right country.
    {std::nullopt, std::nullopt, RepromptResult::kChromeTooOld, "2.0.0.0",
     R"(
       {
          "*":"1.1.0.0",
          "BE":"FUTURE_VERSION",
          "FR":"2.0.0.1"
        } )"},
    // Empty dictionary.
    {std::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", "{}"},
    // Empty parameter.
    {std::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", ""},
    // Wrong number of components.
    {std::nullopt, RepromptResult::kInvalidVersion,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"*":"2.0"} )"},
    // Wildcard in version.
    {std::nullopt, RepromptResult::kInvalidVersion,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"*":"2.0.0.*"} )"},
};

INSTANTIATE_TEST_SUITE_P(,
                         SearchEngineChoiceUtilsParamTest,
                         ::testing::ValuesIn(kRepromptTestParams));

}  // namespace search_engines
