// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

class PrefService;
class TemplateURLService;
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
// If |default_search_provider_index| is non-null, it is set to the index of the
// default search provider within the returned vector.
// `include_current_default` should be true and `template_url_service` should be
// non-null if we want the current default search engine to be present at the
// top of the returned list if it's not already there.
std::vector<std::unique_ptr<TemplateURLData>> GetPrepopulatedEngines(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    size_t* default_search_provider_index,
    bool include_current_default = false,
    TemplateURLService* template_url_service = nullptr);

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

// Returns the default search provider specified by the prepopulate data, which
// may be NULL.
// If |prefs| is NULL, any search provider overrides from the preferences are
// not used.
// `search_engine_choice_service` is used for obtaining the country code and
// shouldn't be null outside of tests.
std::unique_ptr<TemplateURLData> GetPrepopulatedDefaultSearch(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service);

// Test Utilities -------------------------------------------------------------

// Returns all prepopulated engines for all locales. Used only by tests.
std::vector<const PrepopulatedEngine*> GetAllPrepopulatedEngines();

const std::vector<raw_ptr<const PrepopulatedEngine>>
GetPrepopulationSetFromCountryIDForTesting(int country_id);

}  // namespace TemplateURLPrepopulateData

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
