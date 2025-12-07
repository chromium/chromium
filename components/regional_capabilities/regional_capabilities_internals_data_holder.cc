// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_internals_data_holder.h"

#include "base/check_is_test.h"
#include "components/regional_capabilities/access/country_access_reason.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/webui/regional_capabilities_internals/constants.h"

namespace regional_capabilities {

InternalsDataHolder::InternalsDataHolder(
    RegionalCapabilitiesService& regional_capabilities) {
  std::string active_program_name = "Unknown Program";
  switch (regional_capabilities.GetActiveProgramSettings().program) {
    case Program::kDefault:
      active_program_name = "Default";
      break;
    case Program::kTaiyaki:
      active_program_name = "Taiyaki";
      break;
    case Program::kWaffle:
      active_program_name = "Waffle";
      break;
  }
  data_.insert_or_assign(kActiveProgramNameKey, active_program_name);

  data_.insert_or_assign(
      kActiveCountryCodeKey,
      regional_capabilities.GetCountryIdInternal().CountryCode());

  // DO_NOT_SUBMIT: Ensure this doesn't cause histograms to be recorded first.
  data_.insert_or_assign(
      kPrefsCountryCodeKey,
      regional_capabilities.GetPersistedCountryId().CountryCode());
}

InternalsDataHolder::~InternalsDataHolder() = default;

InternalsDataHolder::InternalsDataHolder(const InternalsDataHolder& other) =
    default;

InternalsDataHolder& InternalsDataHolder::operator=(
    const InternalsDataHolder& other) = default;

bool InternalsDataHolder::operator==(const InternalsDataHolder& other) const {
  return data_ == other.data_;
}

const base::flat_map<std::string, std::string>&
InternalsDataHolder::GetRestricted(CountryAccessKey access_key) const {
  // TODO(crbug.com/328040066): Record access to UMA.
  return data_;
}

const base::flat_map<std::string, std::string>&
InternalsDataHolder::GetForTesting() const {
  CHECK_IS_TEST();
  return data_;
}

}  // namespace regional_capabilities
