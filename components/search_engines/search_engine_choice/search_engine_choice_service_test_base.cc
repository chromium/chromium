// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice/search_engine_choice_service_test_base.h"

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/test/bind.h"
#include "components/country_codes/country_codes.h"
#include "components/metrics/cloned_install_detector.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/regional_capabilities/regional_capabilities_prefs.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_test_utils.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_metrics_service_accessor.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"

namespace search_engines {
namespace {
using country_codes::CountryId;

}  // namespace

SearchEngineChoiceServiceTestBase::SearchEngineChoiceServiceTestBase::
    SearchEngineChoiceServiceTestBase() {
  TemplateURLService::RegisterProfilePrefs(pref_service_.registry());
  DefaultSearchManager::RegisterProfilePrefs(pref_service_.registry());
  TemplateURLPrepopulateData::RegisterProfilePrefs(pref_service_.registry());
  SearchEngineChoiceService::RegisterProfilePrefs(pref_service_.registry());
  regional_capabilities::prefs::RegisterProfilePrefs(pref_service_.registry());
  local_state_.registry()->RegisterBooleanPref(
      metrics::prefs::kMetricsReportingEnabled, true);
  local_state_.registry()->RegisterInt64Pref(
      prefs::kDefaultSearchProviderGuestModePrepopulatedId, 0);
  metrics::ClonedInstallDetector::RegisterPrefs(local_state_.registry());

  // Override the country checks to simulate being in Belgium.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "BE");

  // Metrics reporting is disabled for non-branded builds.
  SearchEngineChoiceMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookup(true);

  InitMockPolicyService();
  CheckPoliciesInitialState();
}

SearchEngineChoiceServiceTestBase::~SearchEngineChoiceServiceTestBase() {
  ResetServices();
}

void SearchEngineChoiceServiceTestBase::InitService(InitServiceArgs args) {
  GetOrInitEnvironment(args);

  // Call it to ensure it gets created.
  search_engine_choice_service();
}

void SearchEngineChoiceServiceTestBase::ResetServices() {
  if (search_engines_test_environment_) {
    search_engines_test_environment_->Shutdown();
    search_engines_test_environment_.reset();
  }
}

sync_preferences::TestingPrefServiceSyncable*
SearchEngineChoiceServiceTestBase::pref_service() {
  return &GetOrInitEnvironment().pref_service();
}
TemplateURLService& SearchEngineChoiceServiceTestBase::template_url_service() {
  return CHECK_DEREF(GetOrInitEnvironment().template_url_service());
}
search_engines::SearchEngineChoiceService&
SearchEngineChoiceServiceTestBase::search_engine_choice_service() {
  return GetOrInitEnvironment().search_engine_choice_service();
}
TestingPrefServiceSimple& SearchEngineChoiceServiceTestBase::local_state() {
  return GetOrInitEnvironment().local_state();
}
TemplateURLPrepopulateData::Resolver&
SearchEngineChoiceServiceTestBase::prepopulate_data_resolver() {
  return GetOrInitEnvironment().prepopulate_data_resolver();
}
regional_capabilities::RegionalCapabilitiesService&
SearchEngineChoiceServiceTestBase::regional_capabilities_service() {
  return GetOrInitEnvironment().regional_capabilities_service();
}
policy::ManagementService&
SearchEngineChoiceServiceTestBase::management_service() {
  return GetOrInitEnvironment().management_service();
}

SearchEnginesTestEnvironment&
SearchEngineChoiceServiceTestBase::GetOrInitEnvironment(InitServiceArgs args) {
  if (args.force_reset) {
    ResetServices();
  }

  if (!search_engines_test_environment_) {
    SearchEnginesTestEnvironment::ServiceFactories lazy_factories;
    PopulateLazyFactories(lazy_factories, args);

    search_engines_test_environment_ =
        std::make_unique<SearchEnginesTestEnvironment>(
            SearchEnginesTestEnvironment::Deps{.pref_service = &pref_service_,
                                               .local_state = &local_state_},
            lazy_factories);

    FinalizeEnvironmentInit();
  }
  return *search_engines_test_environment_.get();
}

void SearchEngineChoiceServiceTestBase::PopulateLazyFactories(
    SearchEnginesTestEnvironment::ServiceFactories& lazy_factories,
    InitServiceArgs args) {
  lazy_factories.regional_capabilities_service_factory =
      base::BindLambdaForTesting(
          [args](SearchEnginesTestEnvironment& environment) {
            return regional_capabilities::CreateServiceWithFakeClient(
                environment.pref_service(), args.variation_country_id,
                args.client_country_id, CountryId());
          });
  lazy_factories.search_engine_choice_service_factory =
      SearchEnginesTestEnvironment::GetSearchEngineChoiceServiceFactory(
          /*skip_init=*/false,
          /*client_factory=*/base::BindLambdaForTesting([args]() {
            std::unique_ptr<SearchEngineChoiceService::Client> client =
                std::make_unique<FakeSearchEngineChoiceServiceClient>(
                    args.variation_country_id,
                    args.is_profile_eligible_for_dse_guest_propagation,
                    args.restore_detected_in_current_session,
                    args.choice_predates_restore);
            return client;
          }));
}

void SearchEngineChoiceServiceTestBase::FinalizeEnvironmentInit() {
  // No-op by default.
}

void SearchEngineChoiceServiceTestBase::InitMockPolicyService() {
  ON_CALL(policy_service_, GetPolicies(::testing::Eq(policy::PolicyNamespace(
                               policy::POLICY_DOMAIN_CHROME, std::string()))))
      .WillByDefault(::testing::ReturnRef(policy_map_));
}

// Test that the `DefaultSearchProviderEnabled` and
// `DefaultSearchProviderSearchURL` policies are not initially set.
void SearchEngineChoiceServiceTestBase::CheckPoliciesInitialState() {
  const auto& policies = policy_service().GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));

  const auto* default_search_provider_enabled = policies.GetValue(
      policy::key::kDefaultSearchProviderEnabled, base::Value::Type::BOOLEAN);
  const auto* default_search_provider_search_url = policies.GetValue(
      policy::key::kDefaultSearchProviderSearchURL, base::Value::Type::STRING);

  ASSERT_FALSE(default_search_provider_enabled);
  ASSERT_FALSE(default_search_provider_search_url);
}
}  // namespace search_engines
