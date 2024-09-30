// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_prepopulate_data.h"

#include <algorithm>
#include <random>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "components/country_codes/country_codes.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/eea_countries_ids.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/version_info/version_info.h"

namespace TemplateURLPrepopulateData {

// Helpers --------------------------------------------------------------------

namespace {

#include "components/search_engines/search_engine_countries-inc.cc"

std::vector<std::unique_ptr<TemplateURLData>>
GetPrepopulatedEnginesForEeaRegionCountries(int country_id,
                                            PrefService* prefs) {
  CHECK(search_engines::IsEeaChoiceCountry(country_id));

  uint64_t profile_seed;
  if (prefs) {
    profile_seed = prefs->GetInt64(
        prefs::kDefaultSearchProviderChoiceScreenRandomShuffleSeed);
    int seed_version_number = prefs->GetInteger(
        prefs::kDefaultSearchProviderChoiceScreenShuffleMilestone);
    int current_version_number = version_info::GetMajorVersionNumberAsInt();
    // Ensure that the generated seed is not 0 to avoid accidental re-seeding
    // and re-shuffle on every chrome update.
    while (profile_seed == 0 || current_version_number != seed_version_number) {
      profile_seed = base::RandUint64();
      prefs->SetInt64(
          prefs::kDefaultSearchProviderChoiceScreenRandomShuffleSeed,
          profile_seed);
      prefs->SetInteger(
          prefs::kDefaultSearchProviderChoiceScreenShuffleMilestone,
          current_version_number);
      seed_version_number = current_version_number;
    }
  } else {
    // TODO(crbug.com/40287734): Avoid passing null prefs and unbranch the code.
    CHECK_IS_TEST();
    // Choosing a fixed magic number to ensure a stable shuffle in tests too.
    profile_seed = 42;
  }

  std::vector<std::unique_ptr<TemplateURLData>> t_urls = base::ToVector(
      GetPrepopulationSetFromCountryID(country_id),
      [](const EngineAndTier& entry) {
        return TemplateURLDataFromPrepopulatedEngine(*entry.search_engine);
      });

  std::default_random_engine generator;
  generator.seed(profile_seed);
  std::shuffle(t_urls.begin(), t_urls.end(), generator);

  CHECK_LE(t_urls.size(), kMaxEeaPrepopulatedEngines);
  return t_urls;
}

std::vector<std::unique_ptr<TemplateURLData>> GetPrepopulatedTemplateURLData(
    int country_id,
    PrefService* prefs) {
  std::vector<std::unique_ptr<TemplateURLData>> t_urls;

  if (!prefs) {
    // Possible only in tests.
    // TODO(crbug.com/40287734): Update tests and remove associated branches.
    CHECK_IS_TEST();
  } else if (search_engines::IsEeaChoiceCountry(country_id)) {
    if (search_engines::HasSearchEngineCountryListOverride()) {
      auto country_override =
          absl::get<search_engines::SearchEngineCountryListOverride>(
              search_engines::GetSearchEngineCountryOverride().value());

      switch (country_override) {
        case search_engines::SearchEngineCountryListOverride::kEeaAll:
          return GetAllEeaRegionPrepopulatedEngines();
        case search_engines::SearchEngineCountryListOverride::kEeaDefault:
          return GetDefaultPrepopulatedEngines();
      }
    }
    return GetPrepopulatedEnginesForEeaRegionCountries(country_id, prefs);
  }

  std::vector<EngineAndTier> engines =
      GetPrepopulationSetFromCountryID(country_id);
  for (const EngineAndTier& engine : engines) {
    if (engine.tier == SearchEngineTier::kTopEngines) {
      t_urls.push_back(
          TemplateURLDataFromPrepopulatedEngine(*engine.search_engine));
    }
  }
  return t_urls;
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
    PrefService* prefs) {
  std::vector<std::unique_ptr<TemplateURLData>> t_urls;
  if (!prefs)
    return t_urls;

  const base::Value::List& list =
      prefs->GetList(prefs::kSearchProviderOverrides);

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
                      : (prefs->HasPrefPath(prefs::kSearchProviderOverrides)
                             ? SearchProviderOverrideStatus::kEmptyPref
                             : SearchProviderOverrideStatus::kNoPref));

  return t_urls;
}

std::unique_ptr<TemplateURLData> FindPrepopulatedEngineInternal(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    int prepopulated_id,
    bool use_first_as_fallback) {
  // This could be more efficient. We load all URLs but keep only one.
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_engines =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          prefs, search_engine_choice_service);
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
  registry->RegisterIntegerPref(
      prefs::kDefaultSearchProviderChoiceScreenShuffleMilestone, 0);
  registry->RegisterBooleanPref(
      prefs::kDefaultSearchProviderKeywordsUseExtendedList, false);
}

int GetDataVersion(PrefService* prefs) {
  // Allow tests to override the local version.
  return (prefs && prefs->HasPrefPath(prefs::kSearchProviderOverridesVersion)) ?
      prefs->GetInteger(prefs::kSearchProviderOverridesVersion) :
      kCurrentDataVersion;
}

std::vector<std::unique_ptr<TemplateURLData>> GetPrepopulatedEngines(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service) {
  // If there is a set of search engines in the preferences file, it overrides
  // the built-in set.
  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      GetOverriddenTemplateURLData(prefs);
  if (!t_urls.empty()) {
    return t_urls;
  }

  int country_id;
  if (search_engine_choice_service) {
    country_id = search_engine_choice_service->GetCountryId();
  } else {
    // `search_engine_choice_service` (and `prefs`) can be null in tests.
    // TODO(crbug.com/40287734): Make sure `prefs` and
    // `search_engine_choice_service` are always not null.
    CHECK_IS_TEST();
    country_id = country_codes::GetCurrentCountryID();
  }

  return GetPrepopulatedTemplateURLData(country_id, prefs);
}

std::unique_ptr<TemplateURLData> GetPrepopulatedEngine(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    int prepopulated_id) {
  return FindPrepopulatedEngineInternal(prefs, search_engine_choice_service,
                                        prepopulated_id,
                                        /*use_first_as_fallback=*/false);
}

#if BUILDFLAG(IS_ANDROID)

std::vector<std::unique_ptr<TemplateURLData>> GetLocalPrepopulatedEngines(
    const std::string& country_code,
    PrefService& prefs) {
  int country_id = country_codes::CountryStringToCountryID(country_code);
  if (country_id == country_codes::kCountryIDUnknown) {
    LOG(ERROR) << "Unknown country code specified: " << country_code;
    return std::vector<std::unique_ptr<TemplateURLData>>();
  }

  return GetPrepopulatedTemplateURLData(country_id, &prefs);
}

#endif

std::unique_ptr<TemplateURLData> GetPrepopulatedEngineFromFullList(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
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

  // We look in the profile country's prepopulated set first. This is intended
  // to help using the right entry for the case where we have multiple ones in
  // the full list that share a same prepopulated id.
  const int country = search_engine_choice_service->GetCountryId();
  for (const EngineAndTier& engine_and_tier :
       GetPrepopulationSetFromCountryID(country)) {
    if (engine_and_tier.search_engine->id == prepopulated_id) {
      return TemplateURLDataFromPrepopulatedEngine(
          *engine_and_tier.search_engine);
    }
  }

  // Fallback: just grab the first matching entry from the complete list. In
  // case of IDs shared across multiple entries, we might be returning the
  // wrong one for the profile country. We can look into better heuristics in
  // future work.
  for (const PrepopulatedEngine* engine : kAllEngines) {
    if (engine->id == prepopulated_id) {
      return TemplateURLDataFromPrepopulatedEngine(*engine);
    }
  }

  return nullptr;
}

void ClearPrepopulatedEnginesInPrefs(PrefService* prefs) {
  if (!prefs)
    return;

  prefs->ClearPref(prefs::kSearchProviderOverrides);
  prefs->ClearPref(prefs::kSearchProviderOverridesVersion);
}

std::unique_ptr<TemplateURLData> GetPrepopulatedFallbackSearch(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service) {
  return FindPrepopulatedEngineInternal(prefs, search_engine_choice_service,
                                        google.id,
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
  for (int eea_country_id : search_engines::kEeaChoiceCountriesIds) {
    std::vector<EngineAndTier> country_engines =
        GetPrepopulationSetFromCountryID(eea_country_id);
    for (const EngineAndTier& engine : country_engines) {
      raw_ptr<const PrepopulatedEngine> search_engine = engine.search_engine;
      if (!base::Contains(used_engines, search_engine->id)) {
        result.push_back(TemplateURLDataFromPrepopulatedEngine(*search_engine));
        used_engines.emplace(search_engine->id);
      }
    }
  }

  return result;
}

std::vector<std::unique_ptr<TemplateURLData>> GetDefaultPrepopulatedEngines() {
  return base::ToVector(engines_default, [](const EngineAndTier& entry) {
    return TemplateURLDataFromPrepopulatedEngine(*entry.search_engine);
  });
}

// Test Utilities -------------------------------------------------------------

const std::vector<raw_ptr<const PrepopulatedEngine>>
GetPrepopulationSetFromCountryIDForTesting(int country_id) {
  return base::ToVector(GetPrepopulationSetFromCountryID(country_id),
                        &EngineAndTier::search_engine);
}
}  // namespace TemplateURLPrepopulateData
