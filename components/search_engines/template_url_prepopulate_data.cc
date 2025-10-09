// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_prepopulate_data.h"

#include <algorithm>
#include <random>
#include <variant>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
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
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

namespace TemplateURLPrepopulateData {

// Helpers --------------------------------------------------------------------

namespace {

inline std::unique_ptr<TemplateURLData> PrepopulatedEngineToTemplateURLData(
    const PrepopulatedEngine* engine) {
  return TemplateURLDataFromPrepopulatedEngine(*engine);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SearchProviderOverrideStatus {
  // No preferences are available for `prefs::kSearchProviderOverrides`.
  kNoPref = 0,

  // The preferences for `prefs::kSearchProviderOverrides` do not contain valid
  // template URLs.
  kEmptyPref = 1,

  // The preferences for `prefs::kSearchProviderOverrides` contain valid
  // template URL(s).
  kPrefHasValidUrls = 2,

  kMaxValue = kPrefHasValidUrls
};

std::vector<std::unique_ptr<TemplateURLData>> GetOverriddenTemplateURLData(
    PrefService& prefs) {
  std::vector<std::unique_ptr<TemplateURLData>> t_urls;

  const base::Value::List& list =
      prefs.GetList(prefs::kSearchProviderOverrides);

  for (const base::Value& engine : list) {
    if (engine.is_dict()) {
      auto t_url = TemplateURLDataFromOverrideDictionary(engine.GetDict());
      if (t_url) {
        t_urls.push_back(std::move(t_url));
      }
    }
  }

  base::UmaHistogramEnumeration(
      "Search.SearchProviderOverrideStatus",
      !t_urls.empty() ? SearchProviderOverrideStatus::kPrefHasValidUrls
                      : (prefs.HasPrefPath(prefs::kSearchProviderOverrides)
                             ? SearchProviderOverrideStatus::kEmptyPref
                             : SearchProviderOverrideStatus::kNoPref));

  return t_urls;
}

std::unique_ptr<TemplateURLData> FindPrepopulatedEngineInternal(
    PrefService& prefs,
    const std::vector<const PrepopulatedEngine*>& regional_prepopulated_engines,
    int prepopulated_id,
    bool use_first_as_fallback) {
  // This could be more efficient. We load all URLs but keep only one.
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_engines =
      GetPrepopulatedEngines(prefs, regional_prepopulated_engines);
  CHECK(!prepopulated_engines.empty());

  for (auto& engine : prepopulated_engines) {
    if (engine->prepopulate_id == prepopulated_id) {
      return std::move(engine);
    }
  }

  if (use_first_as_fallback) {
    return std::move(prepopulated_engines[0]);
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
    const std::vector<const PrepopulatedEngine*>&
        regional_prepopulated_engines) {
  // Locate region-specific search engine first to avoid more thorough
  // scanning. In most cases this should offer the correct match.
  if (auto iter =
          std::ranges::find_if(regional_prepopulated_engines, engine_matcher);
      iter != regional_prepopulated_engines.end()) {
    return *iter;
  }

  // Fallback: just grab the first matching entry from the complete list.
  // This is fine as keywords are unique.
  if (auto iter = std::ranges::find_if(kAllEngines, engine_matcher);
      iter != kAllEngines.end()) {
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

int GetDataVersion(PrefService* prefs) {
  // Allow tests to override the local version.
  return (prefs && prefs->HasPrefPath(prefs::kSearchProviderOverridesVersion)) ?
      prefs->GetInteger(prefs::kSearchProviderOverridesVersion) :
      kCurrentDataVersion;
}

std::vector<std::unique_ptr<TemplateURLData>> GetPrepopulatedEngines(
    PrefService& prefs,
    const std::vector<const PrepopulatedEngine*>&
        regional_prepopulated_engines) {
  // If there is a set of search engines in the preferences file, it overrides
  // the built-in set.
  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      GetOverriddenTemplateURLData(prefs);
  if (!t_urls.empty()) {
    return t_urls;
  }

  return base::ToVector(regional_prepopulated_engines,
                        &PrepopulatedEngineToTemplateURLData);
}

std::unique_ptr<TemplateURLData> GetPrepopulatedEngine(
    PrefService& prefs,
    const std::vector<const PrepopulatedEngine*>& regional_prepopulated_engines,
    int prepopulated_id) {
  return FindPrepopulatedEngineInternal(prefs, regional_prepopulated_engines,
                                        prepopulated_id,
                                        /*use_first_as_fallback=*/false);
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
    const std::vector<const PrepopulatedEngine*>&
        regional_prepopulated_engines) {
  return GetPrepopulatedEngineFromBuiltInDataImpl(
      [prepopulated_id](const PrepopulatedEngine* engine) {
        return engine->id == prepopulated_id;
      },
      regional_prepopulated_engines);
}

const PrepopulatedEngine* GetPrepopulatedEngineFromBuiltInData(
    std::u16string_view keyword,
    const std::vector<const PrepopulatedEngine*>&
        regional_prepopulated_engines) {
  return GetPrepopulatedEngineFromBuiltInDataImpl(
      [keyword](const PrepopulatedEngine* engine) {
        return keyword == engine->keyword;
      },
      regional_prepopulated_engines);
}

std::unique_ptr<TemplateURLData> GetPrepopulatedEngineFromFullList(
    PrefService& prefs,
    const std::vector<const PrepopulatedEngine*>& regional_prepopulated_engines,
    int prepopulated_id) {
  // TODO(crbug.com/40940777): Refactor to better share code with
  // `GetPrepopulatedEngine()`.

  // If there is a set of search engines in the preferences file, we look for
  // the ID there first.
  for (std::unique_ptr<TemplateURLData>& data :
       GetOverriddenTemplateURLData(prefs)) {
    if (data->prepopulate_id == prepopulated_id) {
      return std::move(data);
    }
  }

  if (auto* matched_engine = GetPrepopulatedEngineFromBuiltInData(
          prepopulated_id, regional_prepopulated_engines);
      matched_engine) {
    return PrepopulatedEngineToTemplateURLData(matched_engine);
  }

  return {};
}

void ClearPrepopulatedEnginesInPrefs(PrefService* prefs) {
  if (!prefs)
    return;

  prefs->ClearPref(prefs::kSearchProviderOverrides);
  prefs->ClearPref(prefs::kSearchProviderOverridesVersion);
}

std::unique_ptr<TemplateURLData> GetPrepopulatedFallbackSearch(
    PrefService& prefs,
    const std::vector<const PrepopulatedEngine*>&
        regional_prepopulated_engines) {
  return FindPrepopulatedEngineInternal(prefs, regional_prepopulated_engines,
                                        google.id,
                                        /*use_first_as_fallback=*/true);
}

const base::span<const PrepopulatedEngine* const> GetAllPrepopulatedEngines() {
  return kAllEngines;
}


}  // namespace TemplateURLPrepopulateData
