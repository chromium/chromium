// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"

#include "base/command_line.h"
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
#include "components/signin/public/base/signin_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

class SearchEngineChoiceUtilsTest : public ::testing::Test {
 public:
  SearchEngineChoiceUtilsTest() = default;
  ~SearchEngineChoiceUtilsTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(switches::kSearchEngineChoice);
    country_codes::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.registry()->RegisterInt64Pref(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp, 0);

    // Override the country checks to simulate being in Belgium.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "BE");

    InitMockPolicyService();
    CheckPoliciesInitialState();
  }

  policy::MockPolicyService& policy_service() { return policy_service_; }
  policy::PolicyMap& policy_map() { return policy_map_; }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

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
};

// Test that the choice screen doesn't get displayed if the profile is not
// regular.
TEST_F(SearchEngineChoiceUtilsTest, ShowChoiceScreenWithRegularProfile) {
  EXPECT_FALSE(search_engines::ShouldShowChoiceScreen(
      policy_service(), /*profile_properties=*/{
          .is_regular_profile = false, .pref_service = pref_service()}));
}

// Test that the choice screen gets displayed if the
// `DefaultSearchProviderEnabled` policy is not set.
TEST_F(SearchEngineChoiceUtilsTest, ShowChoiceScreenIfPoliciesAreNotSet) {
  EXPECT_TRUE(search_engines::ShouldShowChoiceScreen(
      policy_service(), /*profile_properties=*/{
          .is_regular_profile = true, .pref_service = pref_service()}));
}

// Test that the choice screen doesn't get displayed if the
// 'DefaultSearchProviderEnabled' policy is set to false.
TEST_F(SearchEngineChoiceUtilsTest, DoNotShowChoiceScreenIfPolicySetToFalse) {
  policy_map().Set(policy::key::kDefaultSearchProviderEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  EXPECT_FALSE(search_engines::ShouldShowChoiceScreen(
      policy_service(), /*profile_properties=*/{
          .is_regular_profile = true, .pref_service = pref_service()}));
}

// Test that the choice screen gets displayed if the
// 'DefaultSearchProviderEnabled' policy is set to true but the
// 'DefaultSearchProviderSearchURL' policy is not set.
TEST_F(SearchEngineChoiceUtilsTest,
       DoNotShowChoiceScreenIfPolicySetToTrueWithoutUrlSet) {
  policy_map().Set(policy::key::kDefaultSearchProviderEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  EXPECT_TRUE(search_engines::ShouldShowChoiceScreen(
      policy_service(), /*profile_properties=*/{
          .is_regular_profile = true, .pref_service = pref_service()}));
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
  EXPECT_FALSE(search_engines::ShouldShowChoiceScreen(
      policy_service(), /*profile_properties=*/{
          .is_regular_profile = true, .pref_service = pref_service()}));
}

// Test that the choice screen gets displayed if the
// `kDefaultSearchProviderChoiceScreenTimestamp` pref is not set. Setting this
// pref means that the user has made a search engine choice in the choice
// screen.
TEST_F(SearchEngineChoiceUtilsTest,
       ShowChoiceScreenIfTheTimestampPrefIsNotSet) {
  EXPECT_TRUE(search_engines::ShouldShowChoiceScreen(
      policy_service(),
      /*profile_properties=*/{.is_regular_profile = true,
                              .pref_service = pref_service()}));

  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  EXPECT_FALSE(search_engines::ShouldShowChoiceScreen(
      policy_service(),
      /*profile_properties=*/{.is_regular_profile = true,
                              .pref_service = pref_service()}));
}

// Ensure that the choice screen doesn't get displayed if the flag is disabled.
TEST_F(SearchEngineChoiceUtilsTest, DoNotShowChoiceScreenIfFlagIsDisabled) {
  feature_list()->Reset();
  feature_list()->InitAndDisableFeature(switches::kSearchEngineChoice);
  EXPECT_FALSE(search_engines::ShouldShowChoiceScreen(
      policy_service(), /*profile_properties=*/{
          .is_regular_profile = true, .pref_service = pref_service()}));
}

TEST_F(SearchEngineChoiceUtilsTest, GetSearchEngineChoiceCountryId) {
  const int kBelgiumCountryId =
      country_codes::CountryCharsToCountryID('B', 'E');

  // The test is set up to use the command line to simulate the country as being
  // Belgium.
  EXPECT_EQ(search_engines::GetSearchEngineChoiceCountryId(*pref_service()),
            kBelgiumCountryId);

  // When removing the command line flag, the default value is based on the
  // device locale.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  EXPECT_EQ(search_engines::GetSearchEngineChoiceCountryId(*pref_service()),
            country_codes::GetCurrentCountryID());

  // When the command line value is invalid, it is ignored.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "USA");
  EXPECT_EQ(search_engines::GetSearchEngineChoiceCountryId(*pref_service()),
            country_codes::GetCurrentCountryID());

  // Note that if the format matches (2-character strings), we might get a
  // country ID that is not valid/supported.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "??");
  EXPECT_EQ(search_engines::GetSearchEngineChoiceCountryId(*pref_service()),
            country_codes::CountryCharsToCountryID('?', '?'));

  // The value set from the pref is reflected otherwise.
  pref_service()->SetInteger(country_codes::kCountryIDAtInstall,
                             kBelgiumCountryId);
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  EXPECT_EQ(search_engines::GetSearchEngineChoiceCountryId(*pref_service()),
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
