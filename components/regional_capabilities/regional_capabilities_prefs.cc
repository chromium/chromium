// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_prefs.h"

#include <cstdint>

#include "base/feature_list.h"
#include "base/rand_util.h"
#include "components/country_codes/country_codes.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/search_engines_pref_names.h"

namespace regional_capabilities::prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterInt64Pref(
      ::prefs::kDefaultSearchProviderChoiceScreenRandomShuffleSeed, 0);
  registry->RegisterIntegerPref(kCountryIDAtInstall,
                                country_codes::CountryId().Serialize());
  if (base::FeatureList::IsEnabled(switches::kDynamicProfileCountry)) {
    registry->RegisterIntegerPref(kCountryID,
                                  country_codes::CountryId().Serialize());
  }
}

uint64_t GetShuffleSeed(PrefService& profile_prefs) {
  uint64_t shuffle_seed = profile_prefs.GetInt64(
      ::prefs::kDefaultSearchProviderChoiceScreenRandomShuffleSeed);

  // Ensure that the generated seed is not 0 to avoid accidental re-seeding
  // and re-shuffle next time we call this.
  while (shuffle_seed == 0) {
    shuffle_seed = base::RandUint64();
    profile_prefs.SetInt64(
        ::prefs::kDefaultSearchProviderChoiceScreenRandomShuffleSeed,
        shuffle_seed);
  }

  return shuffle_seed;
}

}  // namespace regional_capabilities::prefs
