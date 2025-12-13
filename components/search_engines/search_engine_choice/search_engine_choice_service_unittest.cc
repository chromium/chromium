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
#include "components/regional_capabilities/program_settings.h"
#include "components/regional_capabilities/regional_capabilities_metrics.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_test_utils.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engine_choice/buildflags.h"
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
using regional_capabilities::CheckHistogramExpectation;
using regional_capabilities::ExpectHistogramBucket;
using regional_capabilities::ExpectHistogramNever;
using regional_capabilities::FunnelStage;
using regional_capabilities::HistogramExpectation;
using ::search_engines::RepromptResult;
using ::search_engines::SearchEngineChoiceWipeReason;
using ::testing::NiceMock;

namespace search_engines {
namespace {

const CountryId kBelgiumCountryId = CountryId("BE");

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
          kPumaSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 0);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 0);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kPumaSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
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
          kPumaSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kPumaSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
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
          kPumaSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kPumaSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);
}

TEST_F(SearchEngineChoiceServiceTest, RecordChoiceMade_ByLocation_Waffle) {
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
    histogram_tester_.ExpectBucketCount(
        search_engines::
            kPumaSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
        SearchEngineType::SEARCH_ENGINE_GOOGLE, expected_v1_records);
    histogram_tester_.ExpectUniqueSample(
        search_engines::
            kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
        SearchEngineType::SEARCH_ENGINE_GOOGLE, expected_v2_records);
    histogram_tester_.ExpectBucketCount(
        search_engines::
            kPumaSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
        SearchEngineType::SEARCH_ENGINE_GOOGLE, expected_v2_records);
    WipeSearchEngineChoicePrefs(*pref_service(),
                                SearchEngineChoiceWipeReason::kCommandLineFlag);
  }
}

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
TEST_F(SearchEngineChoiceServiceTest, RecordChoiceMade_ByLocation_Taiyaki) {
  if (!regional_capabilities::IsClientCompatibleWithProgram(
          regional_capabilities::Program::kTaiyaki)) {
    GTEST_SKIP();
  }

  base::test::ScopedFeatureList feature_list{switches::kTaiyaki};

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, switches::kTaiyakiProgramOverride);
  EXPECT_EQ(template_url_service().GetDefaultSearchProvider()->prepopulate_id(),
            TemplateURLPrepopulateData::google.id);

  auto locations = {ChoiceMadeLocation::kChoiceScreen,
                    ChoiceMadeLocation::kSearchSettings,
                    ChoiceMadeLocation::kSearchEngineSettings};
  for (const ChoiceMadeLocation& choice_location : locations) {
    base::HistogramTester scoped_histogram_tester;
    int expected_v1_records = 0;
    int expected_v2_records = 0;
    bool expect_choice_prefs_presence = false;
    switch (choice_location) {
      case ChoiceMadeLocation::kChoiceScreen:
        // For the choice screen, the choice should be recorded in the both
        // histograms.
        expected_v1_records = 1;
        expected_v2_records = 1;
        expect_choice_prefs_presence = true;
        break;

      case ChoiceMadeLocation::kSearchSettings:
      case ChoiceMadeLocation::kSearchEngineSettings:
        // For other locations, the choice should not be recorded.
        break;
      case ChoiceMadeLocation::kOther:
        NOTREACHED();  // Not an allowed value for `RecordChoiceMade()`.
    }

    search_engine_choice_service().RecordChoiceMade(choice_location,
                                                    &template_url_service());
    scoped_histogram_tester.ExpectBucketCount(
        search_engines::
            kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
        SearchEngineType::SEARCH_ENGINE_GOOGLE, expected_v1_records);
    scoped_histogram_tester.ExpectBucketCount(
        search_engines::
            kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
        SearchEngineType::SEARCH_ENGINE_GOOGLE, expected_v2_records);

    EXPECT_EQ(
        expect_choice_prefs_presence,
        pref_service()->HasPrefPath(
            prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));

    WipeSearchEngineChoicePrefs(*pref_service(),
                                SearchEngineChoiceWipeReason::kCommandLineFlag);
  }
}
#endif

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
  histogram_tester_.ExpectBucketCount(
      search_engines::
          kPumaSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_OTHER, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
      SearchEngineType::SEARCH_ENGINE_OTHER, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kPumaSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
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
  histogram_tester_.ExpectBucketCount(
      search_engines::
          kPumaSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_OTHER, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
      SearchEngineType::SEARCH_ENGINE_OTHER, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::
          kPumaSearchEngineChoiceScreenDefaultSearchEngineType2Histogram,
      SearchEngineType::SEARCH_ENGINE_OTHER, 1);

  EXPECT_NEAR(pref_service()->GetInt64(
                  prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
              base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds(),
              /*abs_error=*/2);
  EXPECT_EQ(pref_service()->GetString(
                prefs::kDefaultSearchProviderChoiceScreenCompletionVersion),
            version_info::GetVersionNumber());
}

#if BUILDFLAG(CHOICE_SCREEN_IN_CHROME)
// TODO(https://crbug.com/465088221): The code covered in these tests is
// irrelevant on Android. Investigate some better way to not include it in the
// build, maybe by splitting the service across platforms?
class SearchEngineChoiceServiceDisplayStateRecordTest
    : public SearchEngineChoiceServiceTest {
 public:
  void SetUp() override {
    SearchEngineChoiceServiceTest::SetUp();
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kSearchEngineChoiceCountry);
  }

  // Based on the max observed list at the moment the test was written. If the
  // max ever exceeds this, consider some dynamic way to set it based on the
  // actual data.
  static constexpr size_t kMaxRegionalListSize = 8u;

  static constexpr CountryId kUsaCountryId = CountryId("US");

  static TemplateURL::OwnedTemplateURLVector
  OwnedTemplateURLVectorFromPrepopulatedEngines(
      const std::vector<const TemplateURLPrepopulateData::PrepopulatedEngine*>&
          engines) {
    TemplateURL::OwnedTemplateURLVector result;
    for (const TemplateURLPrepopulateData::PrepopulatedEngine* engine :
         engines) {
      result.push_back(std::make_unique<TemplateURL>(
          *TemplateURLDataFromPrepopulatedEngine(*engine)));
    }
    return result;
  }

  struct DisplayStateRecordExpectations {
    HistogramExpectation country_mismatch;
    HistogramExpectation selected_index;
    HistogramExpectation display_state_status;
    std::vector<HistogramExpectation> impression_at_index;
  };

  void CheckExpectations(base::HistogramTester& histogram_tester,
                         DisplayStateRecordExpectations expectations,
                         const base::Location& location = FROM_HERE) {
    CheckHistogramExpectation(
        histogram_tester,
        kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram,
        expectations.country_mismatch, location);

    CheckHistogramExpectation(
        histogram_tester, kSearchEngineChoiceScreenSelectedEngineIndexHistogram,
        expectations.selected_index, location);
    CheckHistogramExpectation(
        histogram_tester, kPumaSearchChoiceScreenSelectedEngineIndexHistogram,
        expectations.selected_index, location);

    CheckHistogramExpectation(
        histogram_tester,
        "Search.ChoicePrefsCheck.PendingChoiceScreenDisplayStateStatus",
        expectations.display_state_status, location);

    ASSERT_LE(expectations.impression_at_index.size(), kMaxRegionalListSize);
    for (size_t i = 0; i < kMaxRegionalListSize; ++i) {
      if (i < expectations.impression_at_index.size()) {
        CheckHistogramExpectation(
            histogram_tester,
            base::StringPrintf(
                kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, i),
            expectations.impression_at_index[i], location);
      } else {
        // No expectation passed, let's assume it should not be recorded.
        CheckHistogramExpectation(
            histogram_tester,
            base::StringPrintf(
                kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, i),
            ExpectHistogramNever(), location);
      }
    }
  }
};

TEST_F(SearchEngineChoiceServiceDisplayStateRecordTest, Record) {
  InitService({.variation_country_id = kBelgiumCountryId,
               .client_country_id = kBelgiumCountryId,
               .force_reset = true});
  ChoiceScreenData choice_screen_data(
      OwnedTemplateURLVectorFromPrepopulatedEngines(
          {&TemplateURLPrepopulateData::google,
           &TemplateURLPrepopulateData::bing,
           &TemplateURLPrepopulateData::yahoo}),
      /*current_default_to_highlight=*/nullptr, kBelgiumCountryId,
      SearchTermsData());
  ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
  display_state.selected_engine_index = 2;

  base::HistogramTester histogram_tester;
  search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
      display_state);

  CheckExpectations(
      histogram_tester,
      {.country_mismatch = ExpectHistogramBucket(false),
       .selected_index = ExpectHistogramBucket(2),
       .display_state_status = ExpectHistogramNever(),
       .impression_at_index = {ExpectHistogramBucket(SEARCH_ENGINE_GOOGLE),
                               ExpectHistogramBucket(SEARCH_ENGINE_BING),
                               ExpectHistogramBucket(SEARCH_ENGINE_YAHOO)}});

  // We logged the display state, so we don't need to cache it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

#if BUILDFLAG(IS_IOS)
TEST_F(SearchEngineChoiceServiceDisplayStateRecordTest, Record_Taiyaki) {
  if (!regional_capabilities::IsClientCompatibleWithProgram(
          regional_capabilities::Program::kTaiyaki)) {
    GTEST_SKIP();
  }

  const CountryId kJapanCountryId = CountryId("JP");
  base::test::ScopedFeatureList feature_list{switches::kTaiyaki};

  InitService({.variation_country_id = kJapanCountryId,
               .client_country_id = kJapanCountryId,
               .force_reset = true});
  ChoiceScreenData choice_screen_data(
      OwnedTemplateURLVectorFromPrepopulatedEngines(
          {&TemplateURLPrepopulateData::google,
           &TemplateURLPrepopulateData::bing,
           &TemplateURLPrepopulateData::yahoo}),
      /*current_default_to_highlight=*/nullptr, kJapanCountryId,
      SearchTermsData());
  ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
  display_state.selected_engine_index = 2;

  base::HistogramTester histogram_tester;
  search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
      display_state);

  CheckExpectations(
      histogram_tester,
      {.country_mismatch = ExpectHistogramBucket(false),
       .selected_index = ExpectHistogramBucket(2),
       .display_state_status = ExpectHistogramNever(),
       .impression_at_index = {ExpectHistogramBucket(SEARCH_ENGINE_GOOGLE),
                               ExpectHistogramBucket(SEARCH_ENGINE_BING),
                               ExpectHistogramBucket(SEARCH_ENGINE_YAHOO)}});

  // We logged the display state, so we don't need to cache it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}
#endif  // BUILDFLAG(IS_IOS)

TEST_F(SearchEngineChoiceServiceDisplayStateRecordTest,
       RecordNoop_UnsupportedCountry) {
  auto engines = {&TemplateURLPrepopulateData::google,
                  &TemplateURLPrepopulateData::bing,
                  &TemplateURLPrepopulateData::yahoo};
  base::HistogramTester histogram_tester;

  {
    // Unknown country.
    InitService({.force_reset = true});
    ChoiceScreenData choice_screen_data(
        OwnedTemplateURLVectorFromPrepopulatedEngines(engines),
        /*current_default_to_highlight=*/nullptr, CountryId(),
        SearchTermsData());
    ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
    display_state.selected_engine_index = 0;

    search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
        display_state);
  }

  CheckExpectations(histogram_tester,
                    {.country_mismatch = ExpectHistogramNever(),
                     .selected_index = ExpectHistogramNever(),
                     .display_state_status = ExpectHistogramNever(),
                     .impression_at_index = {}});

  // The choice is coming from a non-eea country and won't be logged, don't
  // cache it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

  {
    // Non-EEA variations country.
    InitService({.variation_country_id = kUsaCountryId, .force_reset = true});
    ChoiceScreenData choice_screen_data(
        OwnedTemplateURLVectorFromPrepopulatedEngines(engines),
        /*current_default_to_highlight=*/nullptr, kUsaCountryId,
        SearchTermsData());
    ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
    display_state.selected_engine_index = 0;
    search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
        display_state);
  }

  CheckExpectations(histogram_tester,
                    {.country_mismatch = ExpectHistogramNever(),
                     .selected_index = ExpectHistogramNever(),
                     .display_state_status = ExpectHistogramNever(),
                     .impression_at_index = {}});

  // The choice is coming from a non-eea country and won't be logged, don't
  // cache it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceDisplayStateRecordTest,
       RecordPostponed_VariationsCountryMismatch) {
  base::HistogramTester histogram_tester;

  // Mismatch between the variations and choice screen data country.
  InitService({.variation_country_id = country_codes::CountryId("DE"),
               .client_country_id = kBelgiumCountryId,
               .force_reset = true});
  ChoiceScreenData choice_screen_data(
      OwnedTemplateURLVectorFromPrepopulatedEngines(
          {&TemplateURLPrepopulateData::google,
           &TemplateURLPrepopulateData::bing,
           &TemplateURLPrepopulateData::yahoo}),
      /*current_default_to_highlight=*/nullptr, kBelgiumCountryId,
      SearchTermsData());
  ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
  display_state.selected_engine_index = 0;
  search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
      display_state);

  CheckExpectations(histogram_tester,
                    {.country_mismatch = ExpectHistogramBucket(true),
                     .selected_index = ExpectHistogramBucket(0),
                     .display_state_status = ExpectHistogramNever(),
                     .impression_at_index = {}});

  // The choice screen state should be cached for a next chance later.
  ASSERT_TRUE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

  auto stored_display_state =
      ChoiceScreenDisplayState::FromDict(pref_service()->GetDict(
          prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
  EXPECT_EQ(stored_display_state->search_engines, display_state.search_engines);
  EXPECT_EQ(stored_display_state->country_id, display_state.country_id);
  EXPECT_EQ(stored_display_state->selected_engine_index,
            display_state.selected_engine_index);
}

TEST_F(SearchEngineChoiceServiceDisplayStateRecordTest, RecordFromCache) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*is_current_default_search_presented=*/false,
      /*includes_non_regional_set_engine=*/false,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

  base::HistogramTester histogram_tester;
  InitService({.variation_country_id = kBelgiumCountryId,
               .client_country_id = kBelgiumCountryId,
               .force_reset = true});

  CheckExpectations(
      histogram_tester,
      {.country_mismatch = ExpectHistogramNever(),
       .selected_index = ExpectHistogramNever(),
       .display_state_status = ExpectHistogramBucket(2 /* kUploaded */),
       .impression_at_index = {ExpectHistogramBucket(SEARCH_ENGINE_GOOGLE),
                               ExpectHistogramBucket(SEARCH_ENGINE_BING),
                               ExpectHistogramBucket(SEARCH_ENGINE_YAHOO)}});

  // The choice screen state should now be cleared.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceDisplayStateRecordTest,
       RecordFromCacheSkipped_ProfileCountryMismatch) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*is_current_default_search_presented=*/false,
      /*includes_non_regional_set_engine=*/false,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

  base::HistogramTester histogram_tester;
  InitService({.variation_country_id = kBelgiumCountryId,
               .client_country_id = kUsaCountryId,
               .force_reset = true});

  CheckExpectations(
      histogram_tester,
      {.country_mismatch = ExpectHistogramNever(),
       .selected_index = ExpectHistogramNever(),
       .display_state_status = ExpectHistogramBucket(3 /* kStayPending */),
       .impression_at_index = {}});

  // The choice screen state should still be pending.
  EXPECT_TRUE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceDisplayStateRecordTest,
       RecordFromCache_ProfileRegionMatch) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*is_current_default_search_presented=*/false,
      /*includes_non_regional_set_engine=*/false,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

  base::HistogramTester histogram_tester;
  InitService({.variation_country_id = kBelgiumCountryId,
               .client_country_id = CountryId("FR"),
               .force_reset = true});

  CheckExpectations(
      histogram_tester,
      {.country_mismatch = ExpectHistogramNever(),
       .selected_index = ExpectHistogramNever(),
       .display_state_status = ExpectHistogramBucket(2 /* kUploaded */),
       .impression_at_index = {ExpectHistogramBucket(SEARCH_ENGINE_GOOGLE),
                               ExpectHistogramBucket(SEARCH_ENGINE_BING),
                               ExpectHistogramBucket(SEARCH_ENGINE_YAHOO)}});

  // The choice screen state should now be cleared.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceDisplayStateRecordTest,
       RecordFromCacheSkipped_VariationsCountryMismatch) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*is_current_default_search_presented=*/false,
      /*includes_non_regional_set_engine=*/false,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

  base::HistogramTester histogram_tester;
  InitService({.variation_country_id = kUsaCountryId,
               .client_country_id = kBelgiumCountryId,
               .force_reset = true});

  CheckExpectations(
      histogram_tester,
      {.country_mismatch = ExpectHistogramNever(),
       .selected_index = ExpectHistogramNever(),
       .display_state_status = ExpectHistogramBucket(3 /* kStayPending */),
       .impression_at_index = {}});

  // The choice screen state should stay around.
  EXPECT_TRUE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceDisplayStateRecordTest,
       RecordFromCacheCancelled_MissingChoiceCompletedPref) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*is_current_default_search_presented=*/false,
      /*includes_non_regional_set_engine=*/false,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());

  base::HistogramTester histogram_tester;
  InitService({.force_reset = true});

  CheckExpectations(
      histogram_tester,
      {.country_mismatch = ExpectHistogramNever(),
       .selected_index = ExpectHistogramNever(),
       .display_state_status = ExpectHistogramBucket(1 /* kTimedOut */),
       .impression_at_index = {}});

  // Choice not marked done, so the service also clears the pending state.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceDisplayStateRecordTest,
       RecordFromCacheCancelled_TimedOut) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*is_current_default_search_presented=*/false,
      /*includes_non_regional_set_engine=*/false,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());

  auto metadata = CreateChoiceCompletionMetadataWithProgram(
      regional_capabilities::SerializeProgram(
          regional_capabilities::Program::kWaffle));
  metadata.timestamp = base::Time::Now() - base::Days(28);
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service(),
                                                            metadata);

  base::HistogramTester histogram_tester;
  InitService({.force_reset = true});

  CheckExpectations(
      histogram_tester,
      {.country_mismatch = ExpectHistogramNever(),
       .selected_index = ExpectHistogramNever(),
       .display_state_status = ExpectHistogramBucket(1 /* kTimedOut */),
       .impression_at_index = {}});

  // Pending state timed out, so the service also clears it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceDisplayStateRecordTest,
       RecordFromCacheCancelled_ParseError) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/CountryId(),  // <= Causes the error
      /*is_current_default_search_presented=*/false,
      /*includes_non_regional_set_engine=*/false,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());

  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

  base::HistogramTester histogram_tester;
  InitService({.force_reset = true});

  CheckExpectations(
      histogram_tester,
      {.country_mismatch = ExpectHistogramNever(),
       .selected_index = ExpectHistogramNever(),
       .display_state_status = ExpectHistogramBucket(0 /* kParseError */),
       .impression_at_index = {}});

  // Pending state is invalid, so the service also clears it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceDisplayStateRecordTest,
       RecordFromCacheCancelled_UmaDisabled) {
  // Disable UMA reporting.
  local_state().SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);

  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*is_current_default_search_presented=*/false,
      /*includes_non_regional_set_engine=*/false,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());
  EXPECT_TRUE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

  InitService({.variation_country_id = kBelgiumCountryId,
               .client_country_id = kBelgiumCountryId,
               .force_reset = true});
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

  CheckExpectations(histogram_tester_,
                    {.country_mismatch = ExpectHistogramNever(),
                     .selected_index = ExpectHistogramNever(),
                     .display_state_status = ExpectHistogramNever(),
                     .impression_at_index = {}});
}
#endif  // BUILDFLAG(CHOICE_SCREEN_IN_CHROME)

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

// Tests if choice screen completion date is recorded.
TEST_F(SearchEngineChoiceServiceTest,
       RecordsChoiceScreenCompletionDateBefore2022Histogram) {
  base::HistogramTester histogram_tester;

  // July 1993. What is specific about this timestamp (in windows epoch seconds)
  // is that it is before 2022.
  int64_t windows_epoch_timestamp = 12388103000;

  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion, "1.0.0.0");
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      windows_epoch_timestamp);

  search_engine_choice_service();
  histogram_tester.ExpectUniqueSample(
      kSearchEngineChoiceCompletedOnMonthHistogram, 100001, 1);
}

// Tests if choice screen completion date is recorded.
TEST_F(SearchEngineChoiceServiceTest,
       RecordsChoiceScreenCompletionDateAfter2050Histogram) {
  base::HistogramTester histogram_tester;

  // December 2056. What is specific about this timestamp (in windows epoch
  // seconds) is that it is after 2050.
  int64_t windows_epoch_timestamp = 14388103000;

  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion, "1.0.0.0");
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      windows_epoch_timestamp);

  search_engine_choice_service();
  histogram_tester.ExpectUniqueSample(
      kSearchEngineChoiceCompletedOnMonthHistogram, 300001, 1);
}

// Test that the user is not reprompted if the reprompt parameter is not a valid
// JSON string.
TEST_F(SearchEngineChoiceServiceTest, NoRepromptForSyntaxError) {
  // Set the reprompt parameters with invalid syntax.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTriggerReprompt,
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
      switches::kSearchEngineChoiceTriggerReprompt,
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
      switches::kSearchEngineChoiceTriggerReprompt,
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

class SearchEngineChoiceServiceChoiceScreenDataTest
    : public SearchEngineChoiceServiceTest {
 public:
  void SetUp() override {
    SearchEngineChoiceServiceTest::SetUp();
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kSearchEngineChoiceCountry);
  }

  void OverrideProgramAndSetHighlightSettingTo(bool enabled) {
    test_program_settings_.choice_screen_eligibility_config
        ->highlight_current_default = enabled;
    regional_capabilities_service().SetCacheForTesting(CountryId("FR"),
                                                       test_program_settings_);
  }

  void SetDefaultSearchProvider(
      const TemplateURLPrepopulateData::PrepopulatedEngine& engine) {
    std::unique_ptr<TemplateURLData> data =
        TemplateURLDataFromPrepopulatedEngine(engine);
    auto template_url = std::make_unique<TemplateURL>(*data);
    template_url_service().SetUserSelectedDefaultSearchProvider(
        template_url.get());
  }

 private:
  // Cached in the instance because as we override the program settings by
  // reference, the reference would otherwise become invalid. We initialize it
  // from a copy of the Waffle settings.
  regional_capabilities::ProgramSettings test_program_settings_ =
      regional_capabilities::GetSettingsForProgram(
          regional_capabilities::Program::kWaffle);
};

TEST_F(SearchEngineChoiceServiceChoiceScreenDataTest, DseHighlight) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kCurrentDseHighlightOnChoiceScreenSupport};

  OverrideProgramAndSetHighlightSettingTo(true);

  auto& test_dse = TemplateURLPrepopulateData::google;
  SetDefaultSearchProvider(test_dse);

  // Calling `SearchEngineChoiceService::GetChoiceScreenData()` through the
  // associated method from `template_url_service` which sets up the right
  // arguments.
  auto choice_screen_data = template_url_service().GetChoiceScreenData();

  // The current default to highlight should be available, and point to an entry
  // in the list owned by the `choice_screen_data`.
  EXPECT_EQ(
      choice_screen_data->current_default_to_highlight()->prepopulate_id(),
      test_dse.id);
  bool found_current_dse = false;
  for (const auto& engine : choice_screen_data->search_engines()) {
    if (engine->prepopulate_id() == test_dse.id) {
      EXPECT_EQ(engine.get(),
                choice_screen_data->current_default_to_highlight());
      found_current_dse = true;
      break;
    }
  }
  ASSERT_TRUE(found_current_dse);
  EXPECT_TRUE(
      choice_screen_data->display_state().is_current_default_search_presented);
  EXPECT_FALSE(
      choice_screen_data->display_state().includes_non_regional_set_engine);
}

TEST_F(SearchEngineChoiceServiceChoiceScreenDataTest,
       DseHighlight_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      switches::kCurrentDseHighlightOnChoiceScreenSupport);
  OverrideProgramAndSetHighlightSettingTo(true);

  auto& test_dse = TemplateURLPrepopulateData::google;
  SetDefaultSearchProvider(test_dse);

  // Calling `SearchEngineChoiceService::GetChoiceScreenData()` through the
  // associated method from `template_url_service` which sets up the right
  // arguments.
  auto choice_screen_data = template_url_service().GetChoiceScreenData();

  // No engine should be provided for highlighting.
  EXPECT_EQ(choice_screen_data->current_default_to_highlight(), nullptr);
  EXPECT_FALSE(
      choice_screen_data->display_state().is_current_default_search_presented);
  EXPECT_FALSE(
      choice_screen_data->display_state().includes_non_regional_set_engine);
}

TEST_F(SearchEngineChoiceServiceChoiceScreenDataTest,
       DseHighlight_DisabledInProgram) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kCurrentDseHighlightOnChoiceScreenSupport};
  OverrideProgramAndSetHighlightSettingTo(false);

  auto& test_dse = TemplateURLPrepopulateData::google;
  SetDefaultSearchProvider(test_dse);

  // Calling `SearchEngineChoiceService::GetChoiceScreenData()` through the
  // associated method from `template_url_service` which sets up the right
  // arguments.
  auto choice_screen_data = template_url_service().GetChoiceScreenData();

  // No engine should be provided for highlighting.
  EXPECT_EQ(choice_screen_data->current_default_to_highlight(), nullptr);
  EXPECT_FALSE(
      choice_screen_data->display_state().is_current_default_search_presented);
  EXPECT_FALSE(
      choice_screen_data->display_state().includes_non_regional_set_engine);
}

TEST_F(SearchEngineChoiceServiceChoiceScreenDataTest,
       DseHighlight_OffRegionDefault) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kCurrentDseHighlightOnChoiceScreenSupport};
  OverrideProgramAndSetHighlightSettingTo(true);

  auto& test_dse = TemplateURLPrepopulateData::naver;
  SetDefaultSearchProvider(test_dse);

  // Calling `SearchEngineChoiceService::GetChoiceScreenData()` through the
  // associated method from `template_url_service` which sets up the right
  // arguments.
  auto choice_screen_data = template_url_service().GetChoiceScreenData();

  // No engine should be provided for highlighting.
  EXPECT_EQ(choice_screen_data->current_default_to_highlight(), nullptr);
  EXPECT_FALSE(
      choice_screen_data->display_state().is_current_default_search_presented);
  EXPECT_FALSE(
      choice_screen_data->display_state().includes_non_regional_set_engine);
}

class SearchEngineChoiceServiceWipeOnMissingDSETest
    : public SearchEngineChoiceServiceTest,
      public testing::WithParamInterface<bool> {
 public:
  SearchEngineChoiceServiceWipeOnMissingDSETest() {
    scoped_feature_list_.InitWithFeatureState(
        switches::kWipeChoicePrefsOnMissingDefaultSearchEngine,
        IsFeatureEnabled());
  }

  bool IsFeatureEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(SearchEngineChoiceServiceWipeOnMissingDSETest, WipeOnMissingDSE) {
  {
    // Set up services and make some DSE choice.
    std::unique_ptr<TemplateURLData> data =
        TemplateURLDataFromPrepopulatedEngine(
            TemplateURLPrepopulateData::google);
    auto template_url = std::make_unique<TemplateURL>(*data);
    template_url_service().SetUserSelectedDefaultSearchProvider(
        template_url.get(), ChoiceMadeLocation::kChoiceScreen);

    // Check that choice prefs are present.
    EXPECT_TRUE(pref_service()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
    EXPECT_TRUE(pref_service()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
    EXPECT_TRUE(pref_service()->HasPrefPath(
        DefaultSearchManager::kDefaultSearchProviderDataPrefName));

    ResetServices();
  }

  // Remove the DSE pref.
  pref_service()->ClearPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);

  // Instantiating the service should wipe the choice prefs when the feature is
  // enabled.
  search_engine_choice_service();

  if (IsFeatureEnabled()) {
    EXPECT_FALSE(pref_service()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
    EXPECT_FALSE(pref_service()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
    histogram_tester_.ExpectUniqueSample(
        search_engines::kSearchEngineChoiceWipeReasonHistogram,
        SearchEngineChoiceWipeReason::kMissingDefaultSearchEngine, 1);
    histogram_tester_.ExpectUniqueSample(
        "Search.ChoicePrefsCheck.WipeOnMissingDse", true, 1);
  } else {
    EXPECT_TRUE(pref_service()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
    EXPECT_TRUE(pref_service()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
    histogram_tester_.ExpectTotalCount(
        search_engines::kSearchEngineChoiceWipeReasonHistogram, 0);
    histogram_tester_.ExpectUniqueSample(
        "Search.ChoicePrefsCheck.WipeOnMissingDse", false, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         SearchEngineChoiceServiceWipeOnMissingDSETest,
                         ::testing::Bool());

struct DeviceRestoreTestParam {
  std::string test_suffix;
  bool restore_detected_in_current_session;
  bool choice_predates_restore;
  bool is_feature_enabled;
  bool is_invalidation_retroactive;
  bool expect_invalidation_timestamp;
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
                               .expect_invalidation_timestamp = true},
        DeviceRestoreTestParam{.test_suffix = "WipeForRetroactiveDetection",
                               .restore_detected_in_current_session = false,
                               .choice_predates_restore = true,
                               .is_feature_enabled = true,
                               .is_invalidation_retroactive = true,
                               .expect_invalidation_timestamp = true},
        DeviceRestoreTestParam{.test_suffix = "NoWipeForLateDetection",
                               .restore_detected_in_current_session = false,
                               .choice_predates_restore = true,
                               .is_feature_enabled = true,
                               .is_invalidation_retroactive = false,
                               .expect_invalidation_timestamp = false},
        DeviceRestoreTestParam{.test_suffix = "NoWipeForNewChoice",
                               .restore_detected_in_current_session = true,
                               .choice_predates_restore = false,
                               .is_feature_enabled = true,
                               .is_invalidation_retroactive = false,
                               .expect_invalidation_timestamp = false},
        DeviceRestoreTestParam{.test_suffix = "NoWipeForFeatureDisabled",
                               .restore_detected_in_current_session = true,
                               .choice_predates_restore = true,
                               .is_feature_enabled = false,
                               .is_invalidation_retroactive = false,
                               .expect_invalidation_timestamp = false},
    }),
    &SearchEngineChoiceServiceDeviceRestoreTest::GetTestSuffix);

TEST_P(SearchEngineChoiceServiceDeviceRestoreTest, RepromptOnRestoreDetection) {
  ASSERT_EQ(switches::kSearchEngineChoiceNoRepromptString,
            switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  SetChoiceCompletionMetadata(*pref_service(),
                              {base::Time::Now(), base::Version("1.0.0.0"),
                               regional_capabilities::SerializeProgram(
                                   regional_capabilities::Program::kWaffle)});
  ASSERT_TRUE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));

  // Trigger the creation of the service, which should check for the reprompt.
  InitService({
      .force_reset = true,
      .restore_detected_in_current_session =
          GetParam().restore_detected_in_current_session,
      .choice_predates_restore = GetParam().choice_predates_restore,
  });

  auto static_eligibility =
      search_engine_choice_service().GetStaticChoiceScreenConditions(
          policy_service(), template_url_service());
  search_engine_choice_service().RecordProfileLoadEligibility(
      static_eligibility);
#if BUILDFLAG(IS_IOS)
  search_engine_choice_service().RecordLegacyStaticEligibility(
      static_eligibility);
#endif  // BUILDFLAG(IS_IOS)

  search_engine_choice_service().RecordTriggeringEligibility(
      search_engine_choice_service().GetDynamicChoiceScreenConditions(
          template_url_service()));

  EXPECT_TRUE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceRepromptHistogram,
      RepromptResult::kNoReprompt, 1);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceWipeReasonHistogram, 0);
  EXPECT_EQ(pref_service()->HasPrefPath(
                prefs::kDefaultSearchProviderChoiceInvalidationTimestamp),
            GetParam().expect_invalidation_timestamp);

  SearchEngineChoiceScreenConditions expected_eligibility_condition =
#if !BUILDFLAG(CHOICE_SCREEN_IN_CHROME)
      SearchEngineChoiceScreenConditions::kUnsupportedBrowserType;
#else
      GetParam().expect_invalidation_timestamp
          ? SearchEngineChoiceScreenConditions::kEligible
          : SearchEngineChoiceScreenConditions::kAlreadyCompleted;
#endif
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      expected_eligibility_condition, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::kPumaSearchChoiceScreenProfileInitConditionsHistogram,
      expected_eligibility_condition, 1);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Eligibility",
      expected_eligibility_condition, 1);
  histogram_tester_.ExpectUniqueSample(
      "PUMA.RegionalCapabilities.FunnelStage.Eligibility",
      expected_eligibility_condition, 1);
  if (GetParam().restore_detected_in_current_session &&
      GetParam().is_feature_enabled) {
    histogram_tester_.ExpectUniqueSample(
        search_engines::kChoiceScreenProfileInitConditionsPostRestoreHistogram,
        expected_eligibility_condition, 1);
  } else {
    histogram_tester_.ExpectTotalCount(
        search_engines::kChoiceScreenProfileInitConditionsPostRestoreHistogram,
        0);
  }
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenNavigationConditionsHistogram,
      expected_eligibility_condition, 1);
  histogram_tester_.ExpectUniqueSample(
      search_engines::kPumaSearchChoiceScreenNavigationConditionsHistogram,
      expected_eligibility_condition, 1);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Triggering",
      expected_eligibility_condition, 1);
  histogram_tester_.ExpectUniqueSample(
      "PUMA.RegionalCapabilities.FunnelStage.Triggering",
      expected_eligibility_condition, 1);
  if (GetParam().restore_detected_in_current_session &&
      GetParam().is_feature_enabled) {
    histogram_tester_.ExpectUniqueSample(
        search_engines::kChoiceScreenNavigationConditionsPostRestoreHistogram,
        expected_eligibility_condition, 1);
  } else {
    histogram_tester_.ExpectTotalCount(
        search_engines::kChoiceScreenNavigationConditionsPostRestoreHistogram,
        0);
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
      switches::kSearchEngineChoiceTriggerReprompt,
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

struct FunnelTestParam {
  std::string test_suffix;
  SearchEngineChoiceScreenConditions condition;
  HistogramExpectation expected_if_static;
  HistogramExpectation expected_if_dynamic;
};

class SearchEngineChoiceServiceFunnelTest
    : public SearchEngineChoiceServiceTest,
      public testing::WithParamInterface<FunnelTestParam> {
 public:
  static std::string GetTestSuffix(
      const testing::TestParamInfo<FunnelTestParam>& info) {
    return info.param.test_suffix;
  }
};

TEST_P(SearchEngineChoiceServiceFunnelTest, RecordsFunnelStage) {
  InitService({.force_reset = true});

  {
    base::HistogramTester scoped_histogram_tester;
    search_engine_choice_service().RecordProfileLoadEligibility(
        GetParam().condition);
    CheckHistogramExpectation(scoped_histogram_tester,
                              "RegionalCapabilities.FunnelStage.Reported",
                              GetParam().expected_if_static);
    CheckHistogramExpectation(scoped_histogram_tester,
                              "PUMA.RegionalCapabilities.FunnelStage.Reported",
                              GetParam().expected_if_static);
  }

  {
    base::HistogramTester scoped_histogram_tester;
    search_engine_choice_service().RecordTriggeringEligibility(
        GetParam().condition);
    CheckHistogramExpectation(scoped_histogram_tester,
                              "RegionalCapabilities.FunnelStage.Reported",
                              GetParam().expected_if_dynamic);
    CheckHistogramExpectation(scoped_histogram_tester,
                              "PUMA.RegionalCapabilities.FunnelStage.Reported",
                              GetParam().expected_if_dynamic);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SearchEngineChoiceServiceFunnelTest,
    testing::ValuesIn<FunnelTestParam>({
        {.test_suffix = "NotInRegionalScope",
         .condition = SearchEngineChoiceScreenConditions::kNotInRegionalScope,
         .expected_if_static =
             ExpectHistogramUnique(FunnelStage::kNotInRegionalScope),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotInRegionalScope)},

        {.test_suffix = "AlreadyCompleted",
         .condition = SearchEngineChoiceScreenConditions::kAlreadyCompleted,
         .expected_if_static =
             ExpectHistogramUnique(FunnelStage::kAlreadyCompleted),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kAlreadyCompleted)},

        {.test_suffix = "Eligible",
         .condition = SearchEngineChoiceScreenConditions::kEligible,
         .expected_if_static = ExpectHistogramNever(),
         .expected_if_dynamic = ExpectHistogramUnique(FunnelStage::kEligible)},

        {.test_suffix = "HasCustomSearchEngine",
         .condition =
             SearchEngineChoiceScreenConditions::kHasCustomSearchEngine,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "SearchProviderOverride",
         .condition =
             SearchEngineChoiceScreenConditions::kSearchProviderOverride,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "ControlledByPolicy",
         .condition = SearchEngineChoiceScreenConditions::kControlledByPolicy,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "ProfileOutOfScope",
         .condition = SearchEngineChoiceScreenConditions::kProfileOutOfScope,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "ExtensionControlled",
         .condition = SearchEngineChoiceScreenConditions::kExtensionControlled,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "UnsupportedBrowserType",
         .condition =
             SearchEngineChoiceScreenConditions::kUnsupportedBrowserType,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "FeatureSuppressed",
         .condition = SearchEngineChoiceScreenConditions::kFeatureSuppressed,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "SuppressedByOtherDialog",
         .condition =
             SearchEngineChoiceScreenConditions::kSuppressedByOtherDialog,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "BrowserWindowTooSmall",
         .condition =
             SearchEngineChoiceScreenConditions::kBrowserWindowTooSmall,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "HasDistributionCustomSearchEngine",
         .condition = SearchEngineChoiceScreenConditions::
             kHasDistributionCustomSearchEngine,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "HasRemovedPrepopulatedSearchEngine",
         .condition = SearchEngineChoiceScreenConditions::
             kHasRemovedPrepopulatedSearchEngine,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "HasNonGoogleSearchEngine",
         .condition =
             SearchEngineChoiceScreenConditions::kHasNonGoogleSearchEngine,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "AppStartedByExternalIntent",
         .condition =
             SearchEngineChoiceScreenConditions::kAppStartedByExternalIntent,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "AlreadyBeingShown",
         .condition = SearchEngineChoiceScreenConditions::kAlreadyBeingShown,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
        {.test_suffix = "UsingPersistedGuestSessionChoice",
         .condition = SearchEngineChoiceScreenConditions::
             kUsingPersistedGuestSessionChoice,
         .expected_if_static = ExpectHistogramUnique(FunnelStage::kNotEligible),
         .expected_if_dynamic =
             ExpectHistogramUnique(FunnelStage::kNotEligible)},
    }),
    &SearchEngineChoiceServiceFunnelTest::GetTestSuffix);

}  // namespace search_engines
