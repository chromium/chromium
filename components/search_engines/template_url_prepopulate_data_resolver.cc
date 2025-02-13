// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_prepopulate_data_resolver.h"

#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/template_url_prepopulate_data.h"

namespace TemplateURLPrepopulateData {

Resolver::Resolver(
    PrefService& prefs,
    search_engines::SearchEngineChoiceService& search_engine_choice_service)
    : profile_prefs_(prefs),
      search_engine_choice_service_(search_engine_choice_service) {}

std::vector<std::unique_ptr<TemplateURLData>> Resolver::GetPrepopulatedEngines()
    const {
  return TemplateURLPrepopulateData::GetPrepopulatedEngines(
      &profile_prefs_.get(), &search_engine_choice_service_.get());
}

std::unique_ptr<TemplateURLData> Resolver::GetPrepopulatedEngine(
    int prepopulated_id) const {
  return TemplateURLPrepopulateData::GetPrepopulatedEngine(
      &profile_prefs_.get(), &search_engine_choice_service_.get(),
      prepopulated_id);
}

std::unique_ptr<TemplateURLData> Resolver::GetEngineFromFullList(
    int prepopulated_id) const {
  return TemplateURLPrepopulateData::GetPrepopulatedEngineFromFullList(
      &profile_prefs_.get(), &search_engine_choice_service_.get(),
      prepopulated_id);
}

std::unique_ptr<TemplateURLData> Resolver::GetFallbackSearch() const {
  return TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
      &profile_prefs_.get(), &search_engine_choice_service_.get());
}

}  // namespace TemplateURLPrepopulateData
