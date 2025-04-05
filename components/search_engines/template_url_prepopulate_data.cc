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
#include "components/regional_capabilities/eea_countries_ids.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#include "third_party/search_engines_data/resources/definitions/regional_settings.h"

namespace TemplateURLPrepopulateData {

// Helpers --------------------------------------------------------------------

namespace {
// The number of search engines for each country falling into the "top"
// category.
constexpr size_t kTopSearchEnginesThreshold = 5;

inline std::unique_ptr<TemplateURLData> PrepopulatedEngineToTemplateURLData(
    const TemplateURLPrepopulateData::PrepopulatedEngine* engine) {
  return TemplateURLDataFromPrepopulatedEngine(*engine);
}

// Returns regional settings appropriate for the current country. If no specific
// regional settings are defined, returns the default settings.
const RegionalSettings& GetRegionalSettings(CountryId country_id) {
  auto iter = kRegionalSettings.find(country_id);
  if (iter == kRegionalSettings.end()) {
    // Fallback to default country.
    iter = kRegionalSettings.find(CountryId());
  }

  return *iter->second;
}

std::vector<std::unique_ptr<TemplateURLData>>
GetPrepopulatedEnginesForEeaRegionCountries(CountryId country_id,
                                            PrefService& prefs) {
  CHECK(regional_capabilities::IsEeaCountry(country_id));

  uint64_t profile_seed = prefs.GetInt64(
      prefs::kDefaultSearchProviderChoiceScreenRandomShuffleSeed);
  // Ensure that the generated seed is not 0 to avoid accidental re-seeding
  // and re-shuffle next time we call this.
  while (profile_seed == 0) {
    profile_seed = base::RandUint64();
    prefs.SetInt64(prefs::kDefaultSearchProviderChoiceScreenRandomShuffleSeed,
                   profile_seed);
  }

  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      base::ToVector(GetRegionalSettings(country_id).search_engines,
                     &PrepopulatedEngineToTemplateURLData);

  std::default_random_engine generator;
  generator.seed(profile_seed);
  std::shuffle(t_urls.begin(), t_urls.end(), generator);

  CHECK_LE(t_urls.size(), kMaxEeaPrepopulatedEngines);
  return t_urls;
}

std::vector<std::unique_ptr<TemplateURLData>> GetPrepopulatedTemplateURLData(
    CountryId country_id,
    PrefService& prefs) {
  if (regional_capabilities::HasSearchEngineCountryListOverride()) {
    auto country_override =
        std::get<regional_capabilities::SearchEngineCountryListOverride>(
            regional_capabilities::GetSearchEngineCountryOverride().value());

    switch (country_override) {
      case regional_capabilities::SearchEngineCountryListOverride::kEeaAll:
        return GetAllEeaRegionPrepopulatedEngines();
      case regional_capabilities::SearchEngineCountryListOverride::kEeaDefault:
        return GetDefaultPrepopulatedEngines();
    }
  }

  if (regional_capabilities::IsEeaCountry(country_id)) {
    return GetPrepopulatedEnginesForEeaRegionCountries(country_id, prefs);
  }

  const auto& engines = GetRegionalSettings(country_id).search_engines;
  size_t num_top_engines = std::min(engines.size(), kTopSearchEnginesThreshold);
  return base::ToVector(base::span(engines).first(num_top_engines),
                        &PrepopulatedEngineToTemplateURLData);
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
    CountryId country_id,
    int prepopulated_id,
    bool use_first_as_fallback) {
  // This could be more efficient. We load all URLs but keep only one.
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_engines =
      GetPrepopulatedEngines(prefs, country_id);
  if (prepopulated_engines.empty()) {
    // Not expected to be a real possibility, branch to be removed when this is
    // verified.
    NOTREACHED(base::NotFatalUntil::M132);
    return nullptr;
  }

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

}  // namespace

// Global functions -----------------------------------------------------------

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  country_codes::RegisterProfilePrefs(registry);
  registry->RegisterListPref(prefs::kSearchProviderOverrides);
  registry->RegisterIntegerPref(prefs::kSearchProviderOverridesVersion, -1);
  registry->RegisterInt64Pref(
      prefs::kDefaultSearchProviderChoiceScreenRandomShuffleSeed, 0);
}

int GetDataVersion(PrefService* prefs) {
  // Allow tests to override the local version.
  return (prefs && prefs->HasPrefPath(prefs::kSearchProviderOverridesVersion)) ?
      prefs->GetInteger(prefs::kSearchProviderOverridesVersion) :
      kCurrentDataVersion;
}

std::vector<std::unique_ptr<TemplateURLData>> GetPrepopulatedEngines(
    PrefService& prefs,
    CountryId country_id) {
  // If there is a set of search engines in the preferences file, it overrides
  // the built-in set.
  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      GetOverriddenTemplateURLData(prefs);
  if (!t_urls.empty()) {
    return t_urls;
  }

  return GetPrepopulatedTemplateURLData(country_id, prefs);
}

std::unique_ptr<TemplateURLData> GetPrepopulatedEngine(PrefService& prefs,
                                                       CountryId country_id,
                                                       int prepopulated_id) {
  return FindPrepopulatedEngineInternal(prefs, country_id, prepopulated_id,
                                        /*use_first_as_fallback=*/false);
}

#if BUILDFLAG(IS_ANDROID)

std::vector<std::unique_ptr<TemplateURLData>> GetLocalPrepopulatedEngines(
    const std::string& country_code,
    PrefService& prefs) {
  CountryId country_id(country_code);
  if (!country_id.IsValid()) {
    LOG(ERROR) << "Unknown country code specified: " << country_code;
    return std::vector<std::unique_ptr<TemplateURLData>>();
  }

  return GetPrepopulatedTemplateURLData(country_id, prefs);
}

#endif

std::unique_ptr<TemplateURLData> GetPrepopulatedEngineFromFullList(
    PrefService& prefs,
    CountryId country_id,
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

  auto engine_matcher = [&](const PrepopulatedEngine* engine) {
    return engine->id == prepopulated_id;
  };

  // We look in the profile country's prepopulated set first. This is intended
  // to help using the right entry for the case where we have multiple ones in
  // the full list that share a same prepopulated id.
  const auto& engines = GetRegionalSettings(country_id).search_engines;
  if (auto iter = std::ranges::find_if(engines, engine_matcher);
      iter != engines.end()) {
    return PrepopulatedEngineToTemplateURLData(*iter);
  }

  // Fallback: just grab the first matching entry from the complete list.
  // In case of IDs shared across multiple entries, we might be returning
  // the wrong one for the profile country. We can look into better
  // heuristics in future work.
  if (auto iter = std::ranges::find_if(kAllEngines, engine_matcher);
      iter != kAllEngines.end()) {
    return PrepopulatedEngineToTemplateURLData(*iter);
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
    CountryId country_id) {
  return FindPrepopulatedEngineInternal(prefs, country_id, google.id,
                                        /*use_first_as_fallback=*/true);
}

const base::span<const PrepopulatedEngine* const> GetAllPrepopulatedEngines() {
  return kAllEngines;
}

std::vector<std::unique_ptr<TemplateURLData>>
GetAllEeaRegionPrepopulatedEngines() {
  std::vector<std::unique_ptr<TemplateURLData>> result;

  // We use a `flat_set` to filter out engines that have the same prepopulated
  // id. For example, `yahoo_fr` and `yahoo_de` have the same prepopulated id
  // because they point to the same search engine so we only want to record one
  // instance.
  base::flat_set<int> used_engines;
  for (CountryId eea_country_id :
       regional_capabilities::kEeaChoiceCountriesIds) {
    const auto& search_engines =
        GetRegionalSettings(eea_country_id).search_engines;

    for (const auto* engine : search_engines) {
      if (auto [_, added] = used_engines.emplace(engine->id); added) {
        result.push_back(PrepopulatedEngineToTemplateURLData(engine));
      }
    }
  }

  return result;
}

std::vector<std::unique_ptr<TemplateURLData>> GetDefaultPrepopulatedEngines() {
  return base::ToVector(GetRegionalSettings(CountryId()).search_engines,
                        &PrepopulatedEngineToTemplateURLData);
}

}  // namespace TemplateURLPrepopulateData
