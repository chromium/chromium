// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service_utils.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/build_time.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/service/variations_service.h"

namespace variations {
namespace {

// Maximum age permitted for a variations seed, in days.
const int kMaxSeedAgeDays = 30;

// Helper function for "HasSeedExpiredSinceTime" that exposes |build_time| and
// makes it overridable by tests.
bool HasSeedExpiredSinceTimeHelper(base::Time fetch_time,
                                   base::Time build_time) {
  // TODO(crbug.com/40274989): Consider comparing the server-provided fetch time
  // with the network time.
  const base::TimeDelta seed_age = base::Time::Now() - fetch_time;

  // base::TimeDelta::InDays() rounds down to the nearest integer, so the seed
  // would not be considered expired if it is less than `kMaxSeedAgeDays + 1`.
  return seed_age.InDays() > kMaxSeedAgeDays && build_time > fetch_time;
}

}  // namespace

bool HasSeedExpiredSinceTime(base::Time fetch_time) {
  return HasSeedExpiredSinceTimeHelper(fetch_time, base::GetBuildTime());
}

bool HasSeedExpiredSinceTimeHelperForTesting(base::Time fetch_time,
                                             base::Time build_time) {
  return HasSeedExpiredSinceTimeHelper(fetch_time, build_time);
}

std::string GetCurrentCountryCode(const VariationsService* variations) {
  std::string country;

  if (variations) {
    country = variations->GetStoredPermanentCountry();
  }

  // Since variations doesn't provide a permanent country by default on things
  // like local builds, we try to fall back to the country_codes component which
  // should always have one.
  return country.empty()
             ? std::string(country_codes::GetCurrentCountryID().CountryCode())
             : country;
}

void RemovePrefsForDeletedProfiles(
    PrefService* local_state,
    std::string_view pref_name,
    const base::flat_set<std::string>& existing_profiles) {
  // Get the current value of the local state dict.
  const base::DictValue& cached_variations_profiles =
      local_state->GetDict(pref_name);
  std::vector<std::string> variations_profiles_to_delete;
  for (std::pair<const std::string&, const base::Value&> profile :
       cached_variations_profiles) {
    if (!existing_profiles.contains(profile.first)) {
      variations_profiles_to_delete.push_back(profile.first);
    }
  }

  ScopedDictPrefUpdate variations_prefs_update(local_state, pref_name);
  std::ranges::for_each(variations_profiles_to_delete,
                        [&variations_prefs_update](const std::string& profile) {
                          variations_prefs_update->Remove(profile);
                        });
}

}  // namespace variations
