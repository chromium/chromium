// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engines_test_environment.h"

#include <utility>

#include "components/metrics/metrics_pref_names.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_test_util.h"

namespace search_engines {

SearchEnginesTestEnvironment::SearchEnginesTestEnvironment(const Deps& deps) {
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

    // This is needed to prevent the code in
    // `SearchEngineChoiceService::GetCountryIdInternal` from calling the
    // Android specific code that requires JNI initialization.
    pref_service_->SetInteger(country_codes::kCountryIDAtInstall,
                              country_codes::CountryCharsToCountryID('U', 'S'));
  }

  search_engine_choice_service_ = std::make_unique<SearchEngineChoiceService>(
      *pref_service_, local_state_
      ,
      /*is_profile_eligible_for_dse_guest_propagation=*/false
  );

  template_url_service_ = std::make_unique<TemplateURLService>(
      *pref_service_, *search_engine_choice_service_,
      deps.template_url_service_initializer);
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

SearchEngineChoiceService&
SearchEnginesTestEnvironment::search_engine_choice_service() {
  return *search_engine_choice_service_;
}

TemplateURLService* SearchEnginesTestEnvironment::template_url_service() {
  return template_url_service_.get();
}

const TemplateURLService* SearchEnginesTestEnvironment::template_url_service()
    const {
  return template_url_service_.get();
}

std::unique_ptr<TemplateURLService>
SearchEnginesTestEnvironment::ReleaseTemplateURLService() {
  return std::move(template_url_service_);
}

SearchEnginesTestEnvironment::~SearchEnginesTestEnvironment() = default;

}  // namespace search_engines
