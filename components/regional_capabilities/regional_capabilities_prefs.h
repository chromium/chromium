// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_PREFS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_PREFS_H_

#include <cstdint>

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Namespace for collecting utilities associated with preferences used by the
// regional_capabilities component.
namespace regional_capabilities::prefs {

// Preference key containing the country ID used for regional capability checks
// and to determine the list of search engine options to prepopulate.
//
// On most OSes (except iOS), this preference key is set once during the Profile
// creation. On iOS, this preference key is updated dynamically.
//
// This is a DEPRECATED preference key, which is used as a fallback if a new
// preference key `kCountryID` is unset.
//
// Until the `kDynamicProfileCountry` feature is not enabled by default,
// the code related to set/read this key preference should remain in place.
inline constexpr char kCountryIDAtInstall[] = "countryid_at_install";

// Preference key containing the country ID associated with the Chrome Profile,
// which updates dynamically. Used for regional capability checks such as
// to determine the list of search engine options to prepopulate.
inline constexpr char kCountryID[] = "countryid";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns a random number to use as a profile-constant seed for the random
// shuffling of the choice screen elements.
// The value is picked the first time this function is called for the profile,
// is is guaranteed to be non-0.
uint64_t GetShuffleSeed(PrefService& profile_prefs);

}  // namespace regional_capabilities::prefs

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_PREFS_H_
