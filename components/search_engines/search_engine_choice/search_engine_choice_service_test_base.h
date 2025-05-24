// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_TEST_BASE_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_TEST_BASE_H_

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace search_engines {

struct InitServiceArgs {
  country_codes::CountryId variation_country_id;
  country_codes::CountryId client_country_id;
  bool force_reset = false;
  bool is_profile_eligible_for_dse_guest_propagation = false;
  bool restore_detected_in_current_session = false;
  bool choice_predates_restore = false;
};

class SearchEngineChoiceServiceTestBase : public ::testing::Test {
 public:
  SearchEngineChoiceServiceTestBase();

  ~SearchEngineChoiceServiceTestBase() override;

  void InitService(InitServiceArgs args = {});
  void ResetServices();

  sync_preferences::TestingPrefServiceSyncable* pref_service();
  TemplateURLService& template_url_service();
  search_engines::SearchEngineChoiceService& search_engine_choice_service();
  TestingPrefServiceSimple& local_state();
  TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver();

  policy::MockPolicyService& policy_service() { return policy_service_; }
  policy::PolicyMap& policy_map() { return policy_map_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  // Public to reduce churn in existing tests.
  base::HistogramTester histogram_tester_;

 protected:
  SearchEnginesTestEnvironment& GetOrInitEnvironment(InitServiceArgs args = {});
  virtual void PopulateLazyFactories(
      SearchEnginesTestEnvironment::ServiceFactories& lazy_factories,
      InitServiceArgs args);
  virtual void FinalizeEnvironmentInit();
  void InitMockPolicyService();

  // Test that the `DefaultSearchProviderEnabled` and
  // `DefaultSearchProviderSearchURL` policies are not initially set.
  void CheckPoliciesInitialState();

  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestingPrefServiceSimple local_state_;
  testing::NiceMock<policy::MockPolicyService> policy_service_;
  policy::PolicyMap policy_map_;
  std::unique_ptr<SearchEnginesTestEnvironment>
      search_engines_test_environment_;
};
}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_TEST_BASE_H_
