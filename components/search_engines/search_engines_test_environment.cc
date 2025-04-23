// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engines_test_environment.h"

#include <utility>

#include "base/test/bind.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/regional_capabilities/regional_capabilities_test_utils.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_test_util.h"

namespace search_engines {

namespace {
SearchEnginesTestEnvironment::ServiceFactories CreateDefaultFactories(
    const SearchEnginesTestEnvironment::Deps& deps) {
  SearchEnginesTestEnvironment::ServiceFactories default_factories;

  default_factories.regional_capabilities_service_factory =
      base::BindRepeating([](SearchEnginesTestEnvironment& environment) {
        return regional_capabilities::CreateServiceWithFakeClient(
            environment.pref_service());
      });
  default_factories.search_engine_choice_service_factory =
      base::BindRepeating([](SearchEnginesTestEnvironment& environment) {
        return std::make_unique<SearchEngineChoiceService>(
            std::make_unique<FakeSearchEngineChoiceServiceClient>(),
            environment.pref_service(), &environment.local_state(),
            environment.regional_capabilities_service(),
            environment.prepopulate_data_resolver());
      });

  default_factories.template_url_service_factory = base::BindLambdaForTesting(
      [deps](SearchEnginesTestEnvironment& environment) {
        return std::make_unique<TemplateURLService>(
            environment.pref_service(),
            environment.search_engine_choice_service(),
            environment.prepopulate_data_resolver(),
            deps.template_url_service_initializer);
      });

  return default_factories;
}
}  // namespace

SearchEnginesTestEnvironment::ServiceFactories::ServiceFactories() = default;

SearchEnginesTestEnvironment::ServiceFactories::ServiceFactories(
    const ServiceFactories& other) = default;
SearchEnginesTestEnvironment::ServiceFactories&
SearchEnginesTestEnvironment::ServiceFactories::operator=(
    const ServiceFactories& other) = default;

SearchEnginesTestEnvironment::ServiceFactories::~ServiceFactories() = default;

SearchEnginesTestEnvironment::SearchEnginesTestEnvironment(
    const Deps& deps,
    const ServiceFactories& lazy_factories)
    : service_factories_overrides_(lazy_factories),
      default_factories_(CreateDefaultFactories(deps)) {
  local_state_ = deps.local_state;
  pref_service_ = deps.pref_service;

  if (!pref_service_) {
    owned_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    pref_service_ = owned_pref_service_.get();
    RegisterPrefsForTemplateURLService(pref_service_->registry());
  }

  if (!local_state_) {
    owned_local_state_ = std::make_unique<TestingPrefServiceSimple>();
    local_state_ = owned_local_state_.get();
    local_state_->registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, true);
    SearchEngineChoiceService::RegisterLocalStatePrefs(
        local_state_->registry());
  }
}

SearchEnginesTestEnvironment::~SearchEnginesTestEnvironment() = default;

void SearchEnginesTestEnvironment::Shutdown() {
  if (template_url_service_) {
    template_url_service_->Shutdown();
  }
}

sync_preferences::TestingPrefServiceSyncable&
SearchEnginesTestEnvironment::pref_service() {
  return *pref_service_;
}

sync_preferences::TestingPrefServiceSyncable&
SearchEnginesTestEnvironment::pref_service() const {
  return *pref_service_;
}

TestingPrefServiceSimple& SearchEnginesTestEnvironment::local_state() {
  return *local_state_;
}

regional_capabilities::RegionalCapabilitiesService&
SearchEnginesTestEnvironment::regional_capabilities_service() {
  if (!regional_capabilities_service_) {
    regional_capabilities_service_ =
        service_factories_overrides_.regional_capabilities_service_factory
            ? service_factories_overrides_.regional_capabilities_service_factory
                  .Run(*this)
            : default_factories_.regional_capabilities_service_factory.Run(
                  *this);
  }
  return *regional_capabilities_service_;
}

TemplateURLPrepopulateData::Resolver&
SearchEnginesTestEnvironment::prepopulate_data_resolver() {
  if (!prepopulate_data_resolver_) {
    prepopulate_data_resolver_ =
        std::make_unique<TemplateURLPrepopulateData::Resolver>(
            pref_service(), regional_capabilities_service());
  }
  return *prepopulate_data_resolver_;
}

SearchEngineChoiceService&
SearchEnginesTestEnvironment::search_engine_choice_service() {
  if (!search_engine_choice_service_) {
    search_engine_choice_service_ =
        service_factories_overrides_.search_engine_choice_service_factory
            ? service_factories_overrides_.search_engine_choice_service_factory
                  .Run(*this)
            : default_factories_.search_engine_choice_service_factory.Run(
                  *this);
  }
  return *search_engine_choice_service_;
}

TemplateURLService* SearchEnginesTestEnvironment::template_url_service() {
  if (!template_url_service_ && !released_template_url_service_) {
    template_url_service_ =
        service_factories_overrides_.template_url_service_factory
            ? service_factories_overrides_.template_url_service_factory.Run(
                  *this)
            : default_factories_.template_url_service_factory.Run(*this);
  }
  return template_url_service_.get();
}

const TemplateURLService* SearchEnginesTestEnvironment::template_url_service()
    const {
  return template_url_service_.get();
}

std::unique_ptr<TemplateURLService>
SearchEnginesTestEnvironment::ReleaseTemplateURLService() {
  // Lazily create the service, which should fail if a previous instance has
  // already been released.
  CHECK(template_url_service());
  released_template_url_service_ = true;
  return std::move(template_url_service_);
}

}  // namespace search_engines
