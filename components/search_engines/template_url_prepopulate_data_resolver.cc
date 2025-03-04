// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_prepopulate_data_resolver.h"

#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/access/country_access_reason.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"

using regional_capabilities::CountryAccessKey;
using regional_capabilities::CountryAccessReason;

namespace TemplateURLPrepopulateData {

Resolver::Resolver(
    PrefService& prefs,
    regional_capabilities::RegionalCapabilitiesService& regional_capabilities)
    : profile_prefs_(prefs), regional_capabilities_(regional_capabilities) {}

std::vector<std::unique_ptr<TemplateURLData>> Resolver::GetPrepopulatedEngines()
    const {
  return TemplateURLPrepopulateData::GetPrepopulatedEngines(
      profile_prefs_.get(),
      // TODO(crbug.com/328040066): Refactor the `TemplateURLPrepopulateData`
      // helpers to accept `CountryIdHolder` and extract the raw country ID
      // only where it needs to be used.
      regional_capabilities_->GetCountryId().GetRestricted(
          CountryAccessKey<Resolver>(
              CountryAccessReason::kTemplateURLPrepopulateDataResolution)));
}

std::unique_ptr<TemplateURLData> Resolver::GetPrepopulatedEngine(
    int prepopulated_id) const {
  return TemplateURLPrepopulateData::GetPrepopulatedEngine(
      profile_prefs_.get(),
      // TODO(crbug.com/328040066): Refactor the `TemplateURLPrepopulateData`
      // helpers to accept `CountryIdHolder` and extract the raw country ID
      // only where it needs to be used.
      regional_capabilities_->GetCountryId().GetRestricted(
          CountryAccessKey<Resolver>(
              CountryAccessReason::kTemplateURLPrepopulateDataResolution)),
      prepopulated_id);
}

std::unique_ptr<TemplateURLData> Resolver::GetEngineFromFullList(
    int prepopulated_id) const {
  return TemplateURLPrepopulateData::GetPrepopulatedEngineFromFullList(
      profile_prefs_.get(),
      // TODO(crbug.com/328040066): Refactor the `TemplateURLPrepopulateData`
      // helpers to accept `CountryIdHolder` and extract the raw country ID
      // only where it needs to be used.
      regional_capabilities_->GetCountryId().GetRestricted(
          CountryAccessKey<Resolver>(
              CountryAccessReason::kTemplateURLPrepopulateDataResolution)),
      prepopulated_id);
}

std::unique_ptr<TemplateURLData> Resolver::GetFallbackSearch() const {
  return TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
      profile_prefs_.get(),
      // TODO(crbug.com/328040066): Refactor the `TemplateURLPrepopulateData`
      // helpers to accept `CountryIdHolder` and extract the raw country ID
      // only where it needs to be used.
      regional_capabilities_->GetCountryId().GetRestricted(
          CountryAccessKey<Resolver>(
              CountryAccessReason::kTemplateURLPrepopulateDataResolution)));
}

}  // namespace TemplateURLPrepopulateData
