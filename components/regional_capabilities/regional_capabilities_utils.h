// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_UTILS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_UTILS_H_

#include <optional>
#include <variant>
#include <vector>

class PrefService;

namespace country_codes {
class CountryId;
}

namespace TemplateURLPrepopulateData {
struct PrepopulatedEngine;
}

namespace regional_capabilities {

enum class SearchEngineListType;
enum class Program;

// Override of the regional program set from the
// `switches::kSearchEngineChoiceCountry` command line flag.
enum class RegionalProgramOverride {
  // Display the program for Japan
  kTaiyaki,
};

// Override of the search engines list set from the
// `switches::kSearchEngineChoiceCountry` command line flag.
enum class SearchEngineCountryListOverride {
  // Display all the search engines used in the EEA region.
  kEeaAll,
  // Display the search engines that we default to when the country is unknown.
  kEeaDefault,
};

// The state of the search engine choice country command line override.
// See `switches::kSearchEngineChoiceCountry`.
using SearchEngineCountryOverride =
    std::variant<country_codes::CountryId,
                 RegionalProgramOverride,
                 SearchEngineCountryListOverride>;

// Gets the search engine country command line override.
// Returns an int if the country id is passed to the command line or a
// `SearchEngineCountryListOverride` if the special values of
// `kDefaultListCountryOverride` or `kEeaListCountryOverride` are passed.
std::optional<SearchEngineCountryOverride> GetSearchEngineCountryOverride();

// Returns whether the search engine list is overridden in the command line to
// a special value that forces the program and the engines list, instead of
// simply to a given country.
bool HasSearchEngineCountryListOverride();

// Returns the prepopulated engines for the given country.
std::vector<const TemplateURLPrepopulateData::PrepopulatedEngine*>
GetPrepopulatedEngines(country_codes::CountryId country_id,
                       PrefService& prefs,
                       SearchEngineListType search_engine_list_type);

// Returns all the prepopulated engines that are used in the EEA region.
std::vector<const TemplateURLPrepopulateData::PrepopulatedEngine*>
GetAllEeaRegionPrepopulatedEngines();

// Returns the set of search engines that is used when the country is unknown.
std::vector<const TemplateURLPrepopulateData::PrepopulatedEngine*>
GetDefaultPrepopulatedEngines();

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_UTILS_H_
