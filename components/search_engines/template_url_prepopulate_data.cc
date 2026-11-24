// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_prepopulate_data.h"

#include <algorithm>
#include <random>
#include <string_view>
#include <variant>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "components/country_codes/country_codes.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/program_settings.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

namespace TemplateURLPrepopulateData {

// Helpers --------------------------------------------------------------------

namespace {

inline std::unique_ptr<TemplateURLData> PrepopulatedEngineToTemplateURLData(
    const PrepopulatedEngine* engine) {
  return engine ? TemplateURLDataFromPrepopulatedEngine(*engine) : nullptr;
}

std::vector<std::unique_ptr<TemplateURLData>>
GetOverriddenTemplateURLDataNoMetrics(const PrefService& prefs) {
  std::vector<std::unique_ptr<TemplateURLData>> t_urls;

  const base::ListValue& list = prefs.GetList(prefs::kSearchProviderOverrides);

  for (const base::Value& engine : list) {
    if (engine.is_dict()) {
      auto t_url = TemplateURLDataFromOverrideDictionary(engine.GetDict());
      if (t_url) {
        t_urls.push_back(std::move(t_url));
      }
    }
  }

  return t_urls;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SearchProviderOverrideStatus)
enum class SearchProviderOverrideStatus {
  // No preferences are available for `prefs::kSearchProviderOverrides`.
  kNoPref = 0,

  // The preferences for `prefs::kSearchProviderOverrides` do not contain valid
  // template URLs.
  kEmptyPref = 1,

  // The preferences for `prefs::kSearchProviderOverrides` contain valid
  // template URL(s).
  kPrefHasValidUrls = 2,

  // Deprecated: used when `kIgnoreSearchProviderOverrides` was enabled and the
  // pref `kSearchProviderOverrides` was present.
  kDeprecatedIgnoredPref = 3,

  kMaxValue = kDeprecatedIgnoredPref,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/search/enums.xml:SearchProviderOverrideStatus)

const PrepopulatedEngine* FindPrepopulatedEngineInternal(
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    int prepopulated_id,
    bool use_first_as_fallback) {
  // The regional list should never be empty because we always fall back to a
  // default non-empty regional list, even when `use_first_as_fallback` is
  // false.
  CHECK(!regional_prepopulated_engines.empty());

  for (const PrepopulatedEngine* engine : regional_prepopulated_engines) {
    if (engine->id == prepopulated_id) {
      return engine;
    }
  }

  if (use_first_as_fallback) {
    return regional_prepopulated_engines[0];
  }

  return nullptr;
}

// Returns the first prepopulated search engine matching `engine_matcher` from
// the full list of known prepopulated search engines, or `nullptr` if it's not
// known there.
//
// - `engine_matcher` is a predicate accepting a `const PrepopulatedEngine*` and
//   returning `bool`, that will be passed to `std::ranges::find_if()`.
// - `regional_prepopulated_engines` is optional, and when present, is used to
//   ensure we prioritise returning a search engine relevant for the given
//   country, for cases where the matcher could be associated with multiple
//   country-specific variants.
template <typename EngineMatcher>
constexpr const PrepopulatedEngine* GetPrepopulatedEngineFromBuiltInDataImpl(
    EngineMatcher engine_matcher,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    const std::vector<raw_ptr<const PrepopulatedEngine>>& regional_variants) {
  // Locate region-specific search engine first to avoid more thorough
  // scanning. In most cases this should offer the correct match.
  if (auto iter =
          std::ranges::find_if(regional_prepopulated_engines, engine_matcher);
      iter != regional_prepopulated_engines.end()) {
    return *iter;
  }

  // Check regional variants next.
  if (auto iter = std::ranges::find_if(regional_variants, engine_matcher);
      iter != regional_variants.end()) {
    return *iter;
  }

  // Fallback: just grab the first matching entry from the complete list.
  const auto& all_engines = regional_capabilities::GetAllPrepopulatedEngines();
  if (auto iter = std::ranges::find_if(all_engines, engine_matcher);
      iter != all_engines.end()) {
    return *iter;
  }

  return nullptr;
}

}  // namespace

// Global functions -----------------------------------------------------------

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kSearchProviderOverrides);
  registry->RegisterIntegerPref(prefs::kSearchProviderOverridesVersion, -1);
}

void RecordSearchProviderOverrideStatus(const PrefService& prefs) {
  SearchProviderOverrideStatus status = SearchProviderOverrideStatus::kNoPref;
  if (prefs.HasPrefPath(prefs::kSearchProviderOverrides)) {
    status = GetOverriddenTemplateURLDataNoMetrics(prefs).empty()
                 ? SearchProviderOverrideStatus::kEmptyPref
                 : SearchProviderOverrideStatus::kPrefHasValidUrls;
  }
  base::UmaHistogramEnumeration("Search.SearchProviderOverrideStatus2", status);
}

int GetDataVersion(PrefService* prefs) {
  return kCurrentDataVersion;
}

std::unique_ptr<TemplateURLData> GetPrepopulatedEngine(
    PrefService& prefs,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    int prepopulated_id) {
  return PrepopulatedEngineToTemplateURLData(FindPrepopulatedEngineInternal(
      regional_prepopulated_engines, prepopulated_id,
      /*use_first_as_fallback=*/false));
}

#if BUILDFLAG(IS_ANDROID)

std::vector<std::unique_ptr<TemplateURLData>> GetLocalPrepopulatedEngines(
    const std::string& country_code,
    PrefService& prefs) {
  country_codes::CountryId country_id(country_code);
  if (!country_id.IsValid()) {
    LOG(ERROR) << "Unknown country code specified: " << country_code;
    return std::vector<std::unique_ptr<TemplateURLData>>();
  }

  return base::ToVector(regional_capabilities::GetPrepopulatedEngines(
                            country_id, prefs,
                            regional_capabilities::SearchEngineListType::kTopN),
                        &PrepopulatedEngineToTemplateURLData);
}

#endif

const PrepopulatedEngine* GetPrepopulatedEngineFromBuiltInData(
    int prepopulated_id,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    const std::vector<raw_ptr<const PrepopulatedEngine>>& regional_variants) {
  return GetPrepopulatedEngineFromBuiltInDataImpl(
      [prepopulated_id](const PrepopulatedEngine* engine) {
        return engine->id == prepopulated_id;
      },
      regional_prepopulated_engines, regional_variants);
}

const PrepopulatedEngine* GetPrepopulatedEngineFromBuiltInData(
    std::u16string_view keyword,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    const std::vector<raw_ptr<const PrepopulatedEngine>>& regional_variants) {
  return GetPrepopulatedEngineFromBuiltInDataImpl(
      [keyword](const PrepopulatedEngine* engine) {
        return keyword == engine->keyword;
      },
      regional_prepopulated_engines, regional_variants);
}

const PrepopulatedEngine* GetPrepopulatedEngineFromBuiltInData(
    SearchEngineType engine_type,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    const std::vector<raw_ptr<const PrepopulatedEngine>>& regional_variants) {
  return GetPrepopulatedEngineFromBuiltInDataImpl(
      [engine_type](const PrepopulatedEngine* engine) {
        return engine->type == engine_type;
      },
      regional_prepopulated_engines, regional_variants);
}

std::unique_ptr<TemplateURLData> GetPrepopulatedEngineFromFullList(
    PrefService& prefs,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    const std::vector<raw_ptr<const PrepopulatedEngine>>& regional_variants,
    int prepopulated_id) {
  // TODO(crbug.com/531675190): Refactor to better share code with
  // `GetPrepopulatedEngine()`.
  return PrepopulatedEngineToTemplateURLData(
      GetPrepopulatedEngineFromBuiltInData(
          prepopulated_id, regional_prepopulated_engines, regional_variants));
}

std::unique_ptr<TemplateURLData> GetPrepopulatedEngineFromFullList(
    PrefService& prefs,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines,
    const std::vector<raw_ptr<const PrepopulatedEngine>>& regional_variants,
    std::u16string_view keyword) {
  // TODO(crbug.com/531675190): Refactor to better share code with
  // `GetPrepopulatedEngine()`.
  return PrepopulatedEngineToTemplateURLData(
      GetPrepopulatedEngineFromBuiltInData(
          keyword, regional_prepopulated_engines, regional_variants));
}

std::vector<raw_ptr<const PrepopulatedEngine>> GetAllPrepopulatedEngines(
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines) {
  std::vector<raw_ptr<const PrepopulatedEngine>> engines;
  base::flat_set<int> seen_ids;

  for (const PrepopulatedEngine* engine : regional_prepopulated_engines) {
    if (seen_ids.insert(engine->id).second) {
      engines.push_back(engine);
    }
  }

  // Engines that no regional list references are not offered.
  // TODO(crbug.com/446637115): With migration enabled, migration targets are
  // not listed outside regions whose list includes them, and a pre-migration ID
  // falls back to its first definition referenced by a regional list.
  // TODO(crbug.com/446637115): `GetPrepopulatedEngineFromFullList()` also
  // consults regional variants, and this function does not. With shadow
  // variants enabled, an engine could be listed with one definition and added
  // with another. Resolve this as part of that feature.
  for (const PrepopulatedEngine* engine :
       regional_capabilities::GetPrepopulatedEnginesInRegionalLists()) {
    if (seen_ids.insert(engine->id).second) {
      engines.push_back(engine);
    }
  }

  return engines;
}

std::unique_ptr<TemplateURLData> GetPrepopulatedFallbackSearch(
    PrefService& prefs,
    const std::vector<raw_ptr<const PrepopulatedEngine>>&
        regional_prepopulated_engines) {
  return PrepopulatedEngineToTemplateURLData(
      FindPrepopulatedEngineInternal(regional_prepopulated_engines, google.id,
                                     /*use_first_as_fallback=*/true));
}

}  // namespace TemplateURLPrepopulateData
