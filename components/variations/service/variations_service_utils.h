// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_UTILS_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_UTILS_H_

#include <string>
#include <string_view>

#include "base/containers/flat_set.h"

namespace base {
class Time;
}  // namespace base

class PrefService;

namespace variations {
class VariationsService;

// Returns whether a seed fetched at |fetch_time| has expired.
bool HasSeedExpiredSinceTime(base::Time fetch_time);

bool HasSeedExpiredSinceTimeHelperForTesting(base::Time fetch_time,
                                             base::Time build_time);

// Gets the user's current country code. If access through variations fails, the
// country_codes component is used. `variations` can be null in test.
std::string GetCurrentCountryCode(const VariationsService* variations);

// Removes entries from the dictionary specified by `pref_name` in `local_state`
// for any keys that are not present in `existing_profiles`. This is used to
// clean up variations prefs for deleted profiles on platforms that support
// multiple profiles.
void RemovePrefsForDeletedProfiles(
    PrefService* local_state,
    std::string_view pref_name,
    const base::flat_set<std::string>& existing_profiles);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_UTILS_H_
