// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_utils.h"

#include <algorithm>
#include <optional>
#include <random>
#include <string>
#include <variant>

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/eea_countries_ids.h"
#include "components/regional_capabilities/program_settings.h"
#include "components/regional_capabilities/regional_capabilities_prefs.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#include "third_party/search_engines_data/resources/definitions/regional_settings.h"

namespace regional_capabilities {

namespace {

using country_codes::CountryId;
using TemplateURLPrepopulateData::PrepopulatedEngine;
using TemplateURLPrepopulateData::RegionalSettings;

// The number of search engines for each country falling into the "top"
// category.
constexpr size_t kTopSearchEnginesThreshold = 5;

// Returns regional settings appropriate for the provided country. If no
// specific regional settings are defined, returns the default settings.
const RegionalSettings& GetRegionalSettings(CountryId country_id) {
  auto iter = TemplateURLPrepopulateData::kRegionalSettings.find(country_id);
  if (iter == TemplateURLPrepopulateData::kRegionalSettings.end()) {
    // Fallback to default country.
    iter = TemplateURLPrepopulateData::kRegionalSettings.find(CountryId());
  }

  return *iter->second;
}

void ShufflePrepopulatedEngines(std::vector<const PrepopulatedEngine*>& engines,
                                PrefService& prefs) {
  std::default_random_engine generator;
  generator.seed(prefs::GetShuffleSeed(prefs));

  std::shuffle(engines.begin(), engines.end(), generator);
}

}  // namespace

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
  if (country_id == switches::kTaiyakiProgramOverride) {
    if (!IsClientCompatibleWithProgram(Program::kTaiyaki)) {
      // The unsupported flag is overriding the country to "invalid country".
      return country_codes::CountryId();
    }

    return RegionalProgramOverride::kTaiyaki;
  }

  return country_codes::CountryId(country_id);
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

std::vector<const PrepopulatedEngine*> GetPrepopulatedEngines(
    CountryId country_id,
    PrefService& prefs,
    SearchEngineListType search_engine_list_type) {
  const RegionalSettings& regional_settings = GetRegionalSettings(country_id);
  std::vector<const PrepopulatedEngine*> engines;

  switch (search_engine_list_type) {
    case SearchEngineListType::kTopN: {
      // Some regional lists can have more (e.g. EEA lists) or fewer (e.g. the
      // default) than 5 entries.
      size_t num_top_engines = std::min(regional_settings.search_engines.size(),
                                        kTopSearchEnginesThreshold);
      engines = base::ToVector(
          base::span(regional_settings.search_engines).first(num_top_engines));
      break;
    }
    case SearchEngineListType::kShuffled: {
      engines = base::ToVector(regional_settings.search_engines);
      ShufflePrepopulatedEngines(engines, prefs);
      break;
    }
  }

  CHECK(!engines.empty()) << "Unexpected PrepopulatedEngines to be empty. "
                             "SearchEngineListType might be invalid: "
                          << static_cast<int>(search_engine_list_type);

  return engines;
}

std::vector<const PrepopulatedEngine*> GetAllEeaRegionPrepopulatedEngines() {
  std::vector<const PrepopulatedEngine*> result;

  // We use a `flat_set` to filter out engines that have the same prepopulated
  // id. For example, `yahoo_fr` and `yahoo_de` have the same prepopulated id
  // because they point to the same search engine so we only want to record one
  // instance.
  base::flat_set<int> used_engines;
  for (CountryId eea_country_id :
       regional_capabilities::kEeaChoiceCountriesIds) {
    const auto& search_engines =
        GetRegionalSettings(eea_country_id).search_engines;

    for (const auto* search_engine : search_engines) {
      if (auto [_, added] = used_engines.emplace(search_engine->id); added) {
        result.push_back(search_engine);
      }
    }
  }

  return result;
}

std::vector<const PrepopulatedEngine*> GetDefaultPrepopulatedEngines() {
  return base::ToVector(GetRegionalSettings(CountryId()).search_engines);
}

}  // namespace regional_capabilities
