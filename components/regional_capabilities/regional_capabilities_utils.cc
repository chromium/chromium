// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_utils.h"

#include <optional>
#include <string>
#include <variant>

#include "base/command_line.h"
#include "components/regional_capabilities/eea_countries_ids.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"

namespace regional_capabilities {

bool IsEeaCountry(int country_id) {
  // The `HasSearchEngineCountryListOverride()` check is here for compatibility
  // with the way EEA presence was checked from `search_engines`. But it should
  // logically be done only when the EEA presence is checked specifically for
  // the current profile country.
  // TODO(crbug.com/328040066): Move this check to
  // `RegionalCapabilitiesService::IsInEeaCountry()`.
  return HasSearchEngineCountryListOverride()
             ? true
             : kEeaChoiceCountriesIds.contains(country_id);
}

std::optional<SearchEngineCountryOverride> GetSearchEngineCountryOverride() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kSearchEngineChoiceCountry)) {
    return std::nullopt;
  }

  std::string country_id =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSearchEngineChoiceCountry);

  if (country_id == switches::kDefaultListCountryOverride) {
    return SearchEngineCountryListOverride::kEeaDefault;
  }
  if (country_id == switches::kEeaListCountryOverride) {
    return SearchEngineCountryListOverride::kEeaAll;
  }
  return country_codes::CountryStringToCountryID(country_id);
}

bool HasSearchEngineCountryListOverride() {
  std::optional<SearchEngineCountryOverride> country_override =
      GetSearchEngineCountryOverride();
  if (!country_override.has_value()) {
    return false;
  }

  return std::holds_alternative<SearchEngineCountryListOverride>(
      country_override.value());
}

}  // namespace regional_capabilities
