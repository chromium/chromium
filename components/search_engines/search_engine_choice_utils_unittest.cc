// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

class SearchEngineChoiceUtilsTest : public ::testing::Test {
 public:
  SearchEngineChoiceUtilsTest()
      : template_url_service_(/*initializers=*/nullptr, /*count=*/0) {}
  ~SearchEngineChoiceUtilsTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(switches::kSearchEngineChoice);
    country_codes::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.registry()->RegisterInt64Pref(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp, 0);
    pref_service_.registry()->RegisterListPref(prefs::kSearchProviderOverrides);

    // Override the country checks to simulate being in Belgium.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "BE");

    InitMockPolicyService();
    CheckPoliciesInitialState();
  }

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
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }
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
  TestingPrefServiceSimple pref_service_;
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

  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
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
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());

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
