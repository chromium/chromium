// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

class PrefService;
struct TemplateURLData;

namespace search_engines {
class SearchEngineChoiceService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace TemplateURLPrepopulateData {

struct PrepopulatedEngine;

extern const int kMaxPrepopulatedEngineID;

// The maximum number of prepopulated search engines that can be returned in
// any of the EEA countries by `GetPrepopulatedEngines()`.
//
// Note: If this is increased, please also increase the declared variant count
// for the `Search.ChoiceScreenShowedEngineAt.Index{Index}` histogram.
inline constexpr size_t kMaxEeaPrepopulatedEngines = 8;

// The maximum number of prepopulated search engines that can be returned in
// in the rest of the world by `GetPrepopulatedEngines()`.
inline constexpr size_t kMaxRowPrepopulatedEngines = 5;

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns the current version of the prepopulate data, so callers can know when
// they need to re-merge. If the prepopulate data comes from the preferences
// file then it returns the version specified there.
int GetDataVersion(PrefService* prefs);

// Returns the prepopulated URLs for the current country.
// `search_engine_choice_service` is used for obtaining the country code and
// shouldn't be null outside of tests.
std::vector<std::unique_ptr<TemplateURLData>> GetPrepopulatedEngines(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service);

// Returns the prepopulated search engine with the given |prepopulated_id|
// from the profile country's known prepopulated search engines, or `nullptr`
// if it's not known there.
// `search_engine_choice_service` is used for obtaining the country code and
// shouldn't be null outside of tests.
std::unique_ptr<TemplateURLData> GetPrepopulatedEngine(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    int prepopulated_id);

// Returns the prepopulated search engine with the given |prepopulated_id|
// from the full list of known prepopulated search engines, or `nullptr` if
// it's not known there.
// `search_engine_choice_service` is used for obtaining the country code and
// shouldn't be null outside of tests.
std::unique_ptr<TemplateURLData> GetPrepopulatedEngineFromFullList(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    int prepopulated_id);

#if BUILDFLAG(IS_ANDROID)
// Returns the prepopulated URLs associated with `country_code`.
// `country_code` is a two-character uppercase ISO 3166-1 country code.
// `prefs` is the main profile's preferences.
std::vector<std::unique_ptr<TemplateURLData>> GetLocalPrepopulatedEngines(
    const std::string& country_code,
    PrefService& prefs);
#endif

// Removes prepopulated engines and their version stored in user prefs.
void ClearPrepopulatedEnginesInPrefs(PrefService* prefs);

// Returns the fallback default search provider, currently hardcoded to be
// Google, or whichever one is the first of the list if Google is not in the
// list of prepopulated search engines.
// Search provider overrides are read from `prefs`, so they won't be used if
// it's null.
// `search_engine_choice_service` is used for obtaining the country code and
// shouldn't be null outside of tests.
// May return `nullptr` if for some reason there are no prepopulated search
// engines available.
std::unique_ptr<TemplateURLData> GetPrepopulatedFallbackSearch(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service);

// Returns all prepopulated engines for all locales.
const base::span<const PrepopulatedEngine* const> GetAllPrepopulatedEngines();

// Returns all the prepopulated engines that are used in the EEA region.
std::vector<std::unique_ptr<TemplateURLData>>
GetAllEeaRegionPrepopulatedEngines();

// Returns the set of search engines that is used when the country is unknown.
std::vector<std::unique_ptr<TemplateURLData>> GetDefaultPrepopulatedEngines();

// Test Utilities -------------------------------------------------------------

const std::vector<raw_ptr<const PrepopulatedEngine>>
GetPrepopulationSetFromCountryIDForTesting(int country_id);

}  // namespace TemplateURLPrepopulateData

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
