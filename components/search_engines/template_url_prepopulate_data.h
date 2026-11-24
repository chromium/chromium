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
#include "components/search_engines/search_engine_type.h"

class PrefService;
struct TemplateURLData;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace TemplateURLPrepopulateData {

struct PrepopulatedEngine;

extern const int kMaxPrepopulatedEngineID;

// The maximum number of prepopulated search engines that can be returned in
// any of the EEA countries by `Resolver::GetPrepopulatedEngines()`.
//
// Note: If this is increased, please also increase the declared variant count
// for the `Search.ChoiceScreenShowedEngineAt.Index{Index}` histogram.
// TODO(crbug.com/408932087): Investigate moving it to the file that actually
// populates these, `//c/regional_capabilities/r*c*_util.cc`.
inline constexpr size_t kMaxEeaPrepopulatedEngines = 8;

// The maximum number of prepopulated search engines that can be returned in
// the rest of the world by `Resolver::GetPrepopulatedEngines()`.
// TODO(crbug.com/408932087): Investigate deduping it with the constant
// `kTopSearchEnginesThreshold` in `//c/regional_capabilities/r*c*_util.cc`.
inline constexpr size_t kMaxRowPrepopulatedEngines = 5;

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Records `Search.SearchProviderOverrideStatus2` reflecting the state of
// `prefs::kSearchProviderOverrides` in `prefs`.
void RecordSearchProviderOverrideStatus(const PrefService& prefs);

// Returns the current version of the prepopulate data, so callers can know when
// they need to re-merge.
int GetDataVersion(PrefService* prefs);

// Returns the prepopulated search engine with the given `prepopulated_id`
// from `regional_prepopulated_engines`, or `nullptr` if it's not known there.
std::unique_ptr<TemplateURLData> GetPrepopulatedEngine(
    PrefService& prefs,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    int prepopulated_id);

// Returns the prepopulated search engine with the given `prepopulated_id`
// from the full list of known prepopulated search engines, or `nullptr` if
// it's not known there.
// The region-specific list is used to ensure we prioritise returning a search
// engine relevant for the given country, for cases where the `prepopulated_id`
// could be associated with multiple country-specific variants.
std::unique_ptr<TemplateURLData> GetPrepopulatedEngineFromFullList(
    PrefService& prefs,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    const std::vector<raw_ptr<const PrepopulatedEngine>>& regional_variants,
    int prepopulated_id);

// Returns the prepopulated search engine with the given `keyword`
// from the full list of known prepopulated search engines, or `nullptr` if
// it's not known there.
// The region-specific list is used to ensure we prioritise returning a search
// engine relevant for the given country, for cases where the `keyword`
// could be associated with multiple country-specific variants.
std::unique_ptr<TemplateURLData> GetPrepopulatedEngineFromFullList(
    PrefService& prefs,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    const std::vector<raw_ptr<const PrepopulatedEngine>>& regional_variants,
    std::u16string_view keyword);

// Returns the engines from `regional_prepopulated_engines`, followed by the
// other prepopulated engines that at least one regional list references (see
// `regional_capabilities::GetPrepopulatedEnginesInRegionalLists()`),
// deduplicated by ID. When several definitions share an ID, the one from
// `regional_prepopulated_engines` is preferred, matching
// `GetPrepopulatedEngineFromFullList()`.
std::vector<raw_ptr<const PrepopulatedEngine>> GetAllPrepopulatedEngines(
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines);

// Returns the prepopulated search engine with the given `prepopulated_id`
// from the full list of known prepopulated search engines, or `nullptr` if
// it's not known there.
// The region-specific list is used to ensure we prioritise returning a search
// engine relevant for the given country, for cases where the `prepopulated_id`
// could be associated with multiple country-specific variants.
const PrepopulatedEngine* GetPrepopulatedEngineFromBuiltInData(
    int prepopulated_id,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    const std::vector<raw_ptr<const PrepopulatedEngine>>& regional_variants);

// Returns the prepopulated search engine with the given `keyword`
// from the full list of known prepopulated search engines, or `nullptr` if
// it's not known there.
const PrepopulatedEngine* GetPrepopulatedEngineFromBuiltInData(
    std::u16string_view keyword,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    const std::vector<raw_ptr<const PrepopulatedEngine>>& regional_variants);

// Returns the prepopulated search engine with the given `engine_type`
// from the full list of known prepopulated search engines, or `nullptr` if
// it's not known there.
// The region-specific list is used to ensure we prioritise returning a search
// engine relevant for the given country, for cases where the `engine_type`
// could be associated with multiple country-specific variants.
// Important: Unlike other functions in this file, it does not look for the
// potential presence of search providers overrides. Use with caution.
const PrepopulatedEngine* GetPrepopulatedEngineFromBuiltInData(
    SearchEngineType engine_type,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    const std::vector<raw_ptr<const PrepopulatedEngine>>& regional_variants);

#if BUILDFLAG(IS_ANDROID)
// Returns the prepopulated URLs associated with `country_code`.
// `country_code` is a two-character uppercase ISO 3166-1 country code.
// `prefs` is the main profile's preferences.
std::vector<std::unique_ptr<TemplateURLData>> GetLocalPrepopulatedEngines(
    const std::string& country_code,
    PrefService& prefs);
#endif

// Returns the fallback default search provider, currently hardcoded to be
// Google, or whichever one is the first of the list if Google is not in the
// list of prepopulated search engines.
// The region-specific list is used to ensure we prioritise returning a search
// engine relevant for the given country, for cases where the `prepopulated_id`
// could be associated with multiple country-specific variants.
// May return `nullptr` if for some reason there are no prepopulated search
// engines available.
std::unique_ptr<TemplateURLData> GetPrepopulatedFallbackSearch(
    PrefService& prefs,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines);

}  // namespace TemplateURLPrepopulateData

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_H_
