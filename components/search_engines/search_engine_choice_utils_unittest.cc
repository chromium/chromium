// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"
#include "base/test/scoped_feature_list.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

class SearchEngineChoiceUtilsTest : public ::testing::Test {
 public:
  SearchEngineChoiceUtilsTest() = default;
  ~SearchEngineChoiceUtilsTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(switches::kSearchEngineChoice);
    pref_service_.registry()->RegisterInt64Pref(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp, 0);

    InitMockPolicyService();
    CheckPoliciesInitialState();
  }

  policy::MockPolicyService& policy_service() { return policy_service_; }
  policy::PolicyMap& policy_map() { return policy_map_; }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

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
