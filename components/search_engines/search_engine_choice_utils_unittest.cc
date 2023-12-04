// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using search_engines::RepromptResult;
using search_engines::WipeSearchEngineChoiceReason;
using ::testing::NiceMock;

namespace search_engines {

class SearchEngineChoiceUtilsTest : public ::testing::Test {
 public:
  SearchEngineChoiceUtilsTest()
      : template_url_service_(/*initializers=*/nullptr, /*count=*/0) {
    feature_list_.InitAndEnableFeature(switches::kSearchEngineChoice);
    country_codes::RegisterProfilePrefs(pref_service_.registry());
    TemplateURLService::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.registry()->RegisterListPref(prefs::kSearchProviderOverrides);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kDefaultSearchProviderChoicePending, false);

    // Override the country checks to simulate being in Belgium.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "BE");

    InitMockPolicyService();
    CheckPoliciesInitialState();
  }

  ~SearchEngineChoiceUtilsTest() override = default;

  void VerifyWillShowChoiceScreen(
      const policy::PolicyService& policy_service,
      const search_engines::ProfileProperties& profile_properties,
      TemplateURLService& template_url_service) {
    PrefService& prefs = CHECK_DEREF(profile_properties.pref_service.get());
    EXPECT_TRUE(search_engines::ShouldShowUpdatedSettings(prefs));
    EXPECT_TRUE(search_engines::ShouldShowChoiceScreen(
        policy_service, profile_properties, &template_url_service));
  }

  void VerifyEligibleButWillNotShowChoiceScreen(
      const policy::PolicyService& policy_service,
      const search_engines::ProfileProperties& profile_properties,
      TemplateURLService& template_url_service) {
    PrefService& prefs = CHECK_DEREF(profile_properties.pref_service.get());
    EXPECT_TRUE(search_engines::ShouldShowUpdatedSettings(prefs));
    EXPECT_FALSE(search_engines::ShouldShowChoiceScreen(
        policy_service, profile_properties, &template_url_service));
  }

  void VerifyNotEligibleAndWillNotShowChoiceScreen(
      const policy::PolicyService& policy_service,
      const search_engines::ProfileProperties& profile_properties,
      TemplateURLService& template_url_service) {
    PrefService& prefs = CHECK_DEREF(profile_properties.pref_service.get());
    EXPECT_FALSE(search_engines::ShouldShowUpdatedSettings(prefs));
    EXPECT_FALSE(search_engines::ShouldShowChoiceScreen(
        policy_service, profile_properties, &template_url_service));
  }

  policy::MockPolicyService& policy_service() { return policy_service_; }
  policy::PolicyMap& policy_map() { return policy_map_; }
  PrefService* pref_service() { return &pref_service_; }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  TemplateURLService& template_url_service() { return template_url_service_; }
  base::HistogramTester histogram_tester_;

 private:
  void InitMockPolicyService() {
    ON_CALL(policy_service_, GetPolicies(::testing::Eq(policy::PolicyNamespace(
                                 policy::POLICY_DOMAIN_CHROME, std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));
  }

  // Test that the `DefaultSearchProviderEnabled` and
  // `DefaultSearchProviderSearchURL` policies are not initially set.
  void CheckPoliciesInitialState() {
    const auto& policies = policy_service().GetPolicies(
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));

    const auto* default_search_provider_enabled = policies.GetValue(
        policy::key::kDefaultSearchProviderEnabled, base::Value::Type::BOOLEAN);
    const auto* default_search_provider_search_url =
        policies.GetValue(policy::key::kDefaultSearchProviderSearchURL,
                          base::Value::Type::STRING);

    ASSERT_FALSE(default_search_provider_enabled);
    ASSERT_FALSE(default_search_provider_search_url);
  }

  NiceMock<policy::MockPolicyService> policy_service_;
  policy::PolicyMap policy_map_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  base::test::ScopedFeatureList feature_list_;
  TemplateURLService template_url_service_;
};

// Test that the choice screen doesn't get displayed if the profile is not
// regular.
TEST_F(SearchEngineChoiceUtilsTest,
       DoNotShowChoiceScreenWithNotRegularProfile) {
  VerifyEligibleButWillNotShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = false, .pref_service = pref_service()},
      template_url_service());
}

// Test that the choice screen gets displayed if the
// `DefaultSearchProviderEnabled` policy is not set.
TEST_F(SearchEngineChoiceUtilsTest, ShowChoiceScreenIfPoliciesAreNotSet) {
  VerifyWillShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());

  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::kEligible, 1);
}

// Test that the choice screen does not get displayed if the provider list is
// overridden in the intial_preferences file.
TEST_F(SearchEngineChoiceUtilsTest,
       DoNotShowChoiceScreenWithProviderListOverride) {
  base::Value::List override_list;
  pref_service()->SetList(prefs::kSearchProviderOverrides,
                          override_list.Clone());

  VerifyEligibleButWillNotShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::
          kSearchProviderOverride,
      1);
}

// Test that the choice screen doesn't get displayed if the
// 'DefaultSearchProviderEnabled' policy is set to false.
TEST_F(SearchEngineChoiceUtilsTest, DoNotShowChoiceScreenIfPolicySetToFalse) {
  policy_map().Set(policy::key::kDefaultSearchProviderEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  VerifyEligibleButWillNotShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::kControlledByPolicy,
      1);
}

// Test that the choice screen gets displayed if the
// 'DefaultSearchProviderEnabled' policy is set to true but the
// 'DefaultSearchProviderSearchURL' policy is not set.
TEST_F(SearchEngineChoiceUtilsTest,
       DoNotShowChoiceScreenIfPolicySetToTrueWithoutUrlSet) {
  policy_map().Set(policy::key::kDefaultSearchProviderEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  VerifyWillShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::kEligible, 1);
}

// Test that the choice screen doesn't get displayed if the
// 'DefaultSearchProviderEnabled' policy is set to true and the
// DefaultSearchProviderSearchURL' is set.
TEST_F(SearchEngineChoiceUtilsTest,
       ShowChoiceScreenIfPolicySetToTrueWithUrlSet) {
  policy_map().Set(policy::key::kDefaultSearchProviderEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policy_map().Set(policy::key::kDefaultSearchProviderSearchURL,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value("test"), nullptr);
  VerifyEligibleButWillNotShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::kControlledByPolicy,
      1);
}

// Test that the choice screen gets displayed if and only if the
// `kDefaultSearchProviderChoiceScreenTimestamp` pref is not set. Setting this
// pref means that the user has made a search engine choice in the choice
// screen.
TEST_F(SearchEngineChoiceUtilsTest,
       ShowChoiceScreenIfTheTimestampPrefIsNotSet) {
  VerifyWillShowChoiceScreen(
      policy_service(),
      /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::kEligible, 1);

  search_engines::RecordChoiceMade(
      pref_service(), search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());
  VerifyEligibleButWillNotShowChoiceScreen(
      policy_service(),
      /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::kAlreadyCompleted, 1);
}

// Test that there is a regional condition controlling eligibility.
TEST_F(SearchEngineChoiceUtilsTest, DoNotShowChoiceScreenIfCountryOutOfScope) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "US");
  VerifyNotEligibleAndWillNotShowChoiceScreen(
      policy_service(),
      /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::kNotInRegionalScope,
      1);
}

// Test that the choice screen does get displayed even if completed if the
// command line argument for forcing it is set.
TEST_F(SearchEngineChoiceUtilsTest, ShowChoiceScreenWithForceCommandLineFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceSearchEngineChoiceScreen);
  search_engines::RecordChoiceMade(
      pref_service(), search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());

  VerifyWillShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());
}

// Ensure that the choice screen doesn't get displayed if the flag is disabled.
TEST_F(SearchEngineChoiceUtilsTest, DoNotShowChoiceScreenIfFlagIsDisabled) {
  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {}, {switches::kSearchEngineChoice, switches::kSearchEngineChoiceFre});
  VerifyNotEligibleAndWillNotShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());
}

TEST_F(SearchEngineChoiceUtilsTest, ShowChoiceScreenWithTriggerFeature) {
  // SearchEngineChoiceTrigger is enabled and not set to trigger only for
  // tagged profiles: the dialog should trigger, regardless of the state of
  // the other feature flags.
  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {switches::kSearchEngineChoiceTrigger},
      {switches::kSearchEngineChoice, switches::kSearchEngineChoiceFre});
  VerifyWillShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());

  // When the param is set and the profile untagged, the dialog will not be
  // displayed.
  base::FieldTrialParams tagged_only_params{
      {switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.name, "true"}};
  feature_list()->Reset();
  feature_list()->InitWithFeaturesAndParameters(
      {{switches::kSearchEngineChoiceTrigger, tagged_only_params}},
      {switches::kSearchEngineChoice, switches::kSearchEngineChoiceFre});
  VerifyEligibleButWillNotShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());

  // When the profile is tagged, the dialog can also be displayed.
  pref_service()->SetBoolean(prefs::kDefaultSearchProviderChoicePending, true);
  VerifyWillShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());
}

// Test that the choice screen does not get displayed if the command line
// argument for disabling it is set.
TEST_F(SearchEngineChoiceUtilsTest,
       DoNotShowChoiceScreenWithDisableCommandLineFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableSearchEngineChoiceScreen);
  VerifyEligibleButWillNotShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());
}

TEST_F(SearchEngineChoiceUtilsTest, GetSearchEngineChoiceCountryId) {
  const int kBelgiumCountryId =
      country_codes::CountryCharsToCountryID('B', 'E');

  // The test is set up to use the command line to simulate the country as being
  // Belgium.
  EXPECT_EQ(search_engines::GetSearchEngineChoiceCountryId(pref_service()),
            kBelgiumCountryId);

  // When removing the command line flag, the default value is based on the
  // device locale.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  EXPECT_EQ(search_engines::GetSearchEngineChoiceCountryId(pref_service()),
            country_codes::GetCurrentCountryID());

  // When the command line value is invalid, it is ignored.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "USA");
  EXPECT_EQ(search_engines::GetSearchEngineChoiceCountryId(pref_service()),
            country_codes::GetCurrentCountryID());

  // Note that if the format matches (2-character strings), we might get a
  // country ID that is not valid/supported.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "??");
  EXPECT_EQ(search_engines::GetSearchEngineChoiceCountryId(pref_service()),
            country_codes::CountryCharsToCountryID('?', '?'));

  // The value set from the pref is reflected otherwise.
  pref_service()->SetInteger(country_codes::kCountryIDAtInstall,
                             kBelgiumCountryId);
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  EXPECT_EQ(search_engines::GetSearchEngineChoiceCountryId(pref_service()),
            kBelgiumCountryId);
}

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

TEST_F(SearchEngineChoiceUtilsTest,
       DoNotShowChoiceScreenIfUserHasCustomSearchEngineSetAsDefault) {
  // A custom search engine will have a `prepopulate_id` of 0.
  const int kCustomSearchEnginePrepopulateId = 0;
  TemplateURLData template_url_data;
  template_url_data.prepopulate_id = kCustomSearchEnginePrepopulateId;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));
  VerifyEligibleButWillNotShowChoiceScreen(
      policy_service(), /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = pref_service()},
      template_url_service());
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::
          kHasCustomSearchEngine,
      1);
}

TEST_F(SearchEngineChoiceUtilsTest, RecordChoiceMade) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  // Test that the choice is not recorded for countries outside the EEA region.
  const int kUnitedStatesCountryId =
      country_codes::CountryCharsToCountryID('U', 'S');
  pref_service()->SetInteger(country_codes::kCountryIDAtInstall,
                             kUnitedStatesCountryId);

  const TemplateURL* default_search_engine =
      template_url_service().GetDefaultSearchProvider();
  EXPECT_EQ(default_search_engine->prepopulate_id(),
            TemplateURLPrepopulateData::google.id);

  search_engines::RecordChoiceMade(
      pref_service(), search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());

  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 0);
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));

  // Revert to an EEA region country.
  const int kBelgiumCountryId =
      country_codes::CountryCharsToCountryID('B', 'E');
  pref_service()->SetInteger(country_codes::kCountryIDAtInstall,
                             kBelgiumCountryId);

  // Test that the choice is recorded if it wasn't previously done.
  search_engines::RecordChoiceMade(
      pref_service(), search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
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
  search_engines::RecordChoiceMade(
      pref_service(), search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());
  EXPECT_EQ(pref_service()->GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
            kModifiedTimestamp);

  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);
}

TEST_F(SearchEngineChoiceUtilsTest, IsChoiceScreenFlagEnabled) {
  feature_list()->Reset();
  feature_list()->InitWithFeaturesAndParameters(
      /*enabled_features=*/{}, /*disabled_features=*/{
          switches::kSearchEngineChoiceTrigger,
          switches::kSearchEngineChoiceFre,
          switches::kSearchEngineChoice,
      });

  EXPECT_FALSE(IsChoiceScreenFlagEnabled(ChoicePromo::kAny));
  EXPECT_FALSE(IsChoiceScreenFlagEnabled(ChoicePromo::kFre));
  EXPECT_FALSE(IsChoiceScreenFlagEnabled(ChoicePromo::kDialog));

  feature_list()->Reset();
  feature_list()->InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{switches::kSearchEngineChoiceTrigger,
        {
            {switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.name,
             "false"},
        }}},
      /*disabled_features=*/{
          switches::kSearchEngineChoiceFre,
          switches::kSearchEngineChoice,
      });

  EXPECT_TRUE(IsChoiceScreenFlagEnabled(ChoicePromo::kAny));
  EXPECT_TRUE(IsChoiceScreenFlagEnabled(ChoicePromo::kFre));
  EXPECT_TRUE(IsChoiceScreenFlagEnabled(ChoicePromo::kDialog));

  feature_list()->Reset();
  feature_list()->InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{switches::kSearchEngineChoiceTrigger,
        {
            {switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.name,
             "true"},
        }}},
      /*disabled_features=*/{
          switches::kSearchEngineChoiceFre,
          switches::kSearchEngineChoice,
      });

  EXPECT_TRUE(IsChoiceScreenFlagEnabled(ChoicePromo::kAny));
  EXPECT_TRUE(IsChoiceScreenFlagEnabled(ChoicePromo::kFre));
#if BUILDFLAG(IS_IOS)
  EXPECT_FALSE(IsChoiceScreenFlagEnabled(ChoicePromo::kDialog));
#else
  EXPECT_TRUE(IsChoiceScreenFlagEnabled(ChoicePromo::kDialog));
#endif
}

// Test that the user is not reprompted is the reprompt parameter is not a valid
// JSON string.
TEST_F(SearchEngineChoiceUtilsTest, NoRepromptForSyntaxError) {
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
  base::Version choice_version({1, 2, 3, 4});
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      choice_version.GetString());

  // The user should not be reprompted.
  search_engines::PreprocessPrefsForReprompt(*pref_service());

  EXPECT_EQ(kPreviousTimestamp,
            pref_service()->GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  EXPECT_EQ(choice_version.GetString(),
            pref_service()->GetString(
                prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
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

// The user is reprompted if the version preference is missing.
TEST_F(SearchEngineChoiceUtilsTest, RepromptForMissingChoiceVersion) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSearchEngineChoiceTrigger};

  // Initialize the timestamp, but not the version.
  int64_t kPreviousTimestamp = 1;
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      kPreviousTimestamp);

  // The user should be reprompted.
  search_engines::PreprocessPrefsForReprompt(*pref_service());

  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceWipeReasonHistogram,
      WipeSearchEngineChoiceReason::kMissingChoiceVersion, 1);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptWildcardHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptHistogram, 0);
}

struct RepromptTestParam {
  // Whether the user should be reprompted or not.
  absl::optional<WipeSearchEngineChoiceReason> wipe_reason;
  // Internal results of the reprompt computation.
  absl::optional<RepromptResult> wildcard_result;
  absl::optional<RepromptResult> country_result;
  // Version of the choice.
  const base::StringPiece choice_version;
  // The reprompt params, as sent by the server.  Use `CURRENT_VERSION` for the
  // current Chrome version, and `FUTURE_VERSION` for a future version.
  const char* param_dict;
};

class SearchEngineChoiceUtilsParamTest
    : public SearchEngineChoiceUtilsTest,
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

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTrigger,
      {{switches::kSearchEngineChoiceTriggerRepromptParams.name, param_dict}});
  ASSERT_EQ(param_dict.empty() ? "{}" : param_dict,
            switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  // Initialize the preference with some previous choice.
  int64_t kPreviousTimestamp = 1;
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      kPreviousTimestamp);
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      GetParam().choice_version);

  // Check whether the user is reprompted.
  search_engines::PreprocessPrefsForReprompt(*pref_service());

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
    {WipeSearchEngineChoiceReason::kReprompt, RepromptResult::kReprompt,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"*":"1.0.0.1"} )"},
    // Reprompt works with all version components.
    {WipeSearchEngineChoiceReason::kReprompt, RepromptResult::kReprompt,
     RepromptResult::kNoDictionaryKey, "1.0.0.100", R"( {"*":"1.0.1.0"} )"},
    {WipeSearchEngineChoiceReason::kReprompt, RepromptResult::kReprompt,
     RepromptResult::kNoDictionaryKey, "1.0.200.0", R"( {"*":"1.1.0.0"} )"},
    {WipeSearchEngineChoiceReason::kReprompt, RepromptResult::kReprompt,
     RepromptResult::kNoDictionaryKey, "1.300.0.0", R"( {"*":"2.0.0.0"} )"},
    {WipeSearchEngineChoiceReason::kReprompt, RepromptResult::kReprompt,
     RepromptResult::kNoDictionaryKey, "10.10.1.1",
     R"( {"*":"30.45.678.9100"} )"},
    // Reprompt a specific country.
    {WipeSearchEngineChoiceReason::kReprompt, absl::nullopt,
     RepromptResult::kReprompt, "1.0.0.0", R"( {"BE":"1.0.0.1"} )"},
    // Reprompt for params inclusive of current version
    {WipeSearchEngineChoiceReason::kReprompt, absl::nullopt,
     RepromptResult::kReprompt, "1.0.0.0", R"( {"BE":"CURRENT_VERSION"} )"},
    // Reprompt when the choice version is malformed.
    {WipeSearchEngineChoiceReason::kInvalidChoiceVersion, absl::nullopt,
     absl::nullopt, "Blah", ""},
    // Reprompt when both the country and the wild card are specified, as long
    // as one of them qualifies.
    {WipeSearchEngineChoiceReason::kReprompt, absl::nullopt,
     RepromptResult::kReprompt, "1.0.0.0",
     R"( {"*":"1.0.0.1","BE":"1.0.0.1"} )"},
    {WipeSearchEngineChoiceReason::kReprompt, absl::nullopt,
     RepromptResult::kReprompt, "1.0.0.0",
     R"( {"*":"FUTURE_VERSION","BE":"1.0.0.1"} )"},
    // Still works with irrelevant parameters for other countries.
    {WipeSearchEngineChoiceReason::kReprompt, absl::nullopt,
     RepromptResult::kReprompt, "1.0.0.0",
     R"(
       {
         "FR":"FUTURE_VERSION",
         "INVALID_COUNTRY":"INVALID_VERSION",
         "US":"FUTURE_VERSION",
         "BE":"1.0.0.1"
       } )"},

    // Don't reprompt when the choice was made in the current version.
    {absl::nullopt, RepromptResult::kRecentChoice,
     RepromptResult::kNoDictionaryKey, version_info::GetVersionNumber(),
     "{\"*\":\"CURRENT_VERSION\"}"},
    // Don't reprompt when the choice was recent enough.
    {absl::nullopt, RepromptResult::kRecentChoice,
     RepromptResult::kNoDictionaryKey, "2.0.0.0", R"( {"*":"1.0.0.1"} )"},
    // Don't reprompt for another country.
    {absl::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"FR":"1.0.0.1"} )"},
    {absl::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"US":"1.0.0.1"} )"},
    {absl::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"XX":"1.0.0.1"} )"},
    {absl::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0",
     R"( {"INVALID_COUNTRY":"1.0.0.1"} )"},
    {absl::nullopt, absl::nullopt, RepromptResult::kChromeTooOld, "1.0.0.0",
     R"( {"FR":"1.0.0.1","BE":"FUTURE_VERSION"} )"},
    // Don't reprompt for future versions.
    {absl::nullopt, RepromptResult::kChromeTooOld,
     RepromptResult::kNoDictionaryKey, "1.0.0.0",
     R"( {"*":"FUTURE_VERSION"} )"},
    // Wildcard is overridden by specific country.
    {absl::nullopt, absl::nullopt, RepromptResult::kChromeTooOld, "1.0.0.0",
     R"( {"*":"1.0.0.1","BE":"FUTURE_VERSION"} )"},
    // Combination of right version for wrong country and wrong version for
    // right country.
    {absl::nullopt, absl::nullopt, RepromptResult::kChromeTooOld, "2.0.0.0",
     R"(
       {
          "*":"1.1.0.0",
          "BE":"FUTURE_VERSION",
          "FR":"2.0.0.1"
        } )"},
    // Empty dictionary.
    {absl::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", "{}"},
    // Empty parameter.
    {absl::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", ""},
    // Wrong number of components.
    {absl::nullopt, RepromptResult::kInvalidVersion,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"*":"2.0"} )"},
    // Wildcard in version.
    {absl::nullopt, RepromptResult::kInvalidVersion,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"*":"2.0.0.*"} )"},
};

INSTANTIATE_TEST_SUITE_P(,
                         SearchEngineChoiceUtilsParamTest,
                         ::testing::ValuesIn(kRepromptTestParams));

}  // namespace search_engines
