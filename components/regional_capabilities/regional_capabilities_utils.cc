// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_utils.h"

#include <optional>
#include <string>
#include <variant>

#include "base/command_line.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/search_engines_switches.h"

namespace regional_capabilities {

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
