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

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns a random number to use as a profile-constant seed for the random
// shuffling of the choice screen elements.
// The value is picked the first time this function is called for the profile,
// is is guaranteed to be non-0.
uint64_t GetShuffleSeed(PrefService& profile_prefs);

}  // namespace regional_capabilities::prefs

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_PREFS_H_
