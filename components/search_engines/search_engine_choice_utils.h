// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_UTILS_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_UTILS_H_

#include "base/memory/raw_ptr.h"

namespace policy {
class PolicyService;
}

class PrefService;

namespace search_engines {

// Profile properties that need to be passed to
// `ShouldShowChoiceScreen`. This is due to the fact that
// the 'Profile' class is different between platforms.
struct ProfileProperties {
  bool is_regular_profile = false;
  raw_ptr<PrefService> pref_service;
};

// Returns whether the search engine choice screen can be displayed or not based
// on device policies and profile properties.
bool ShouldShowChoiceScreen(const policy::PolicyService& policy_service,
                            const ProfileProperties& profile_properties);

// Returns the country ID to use in the context of any search engine choice
// logic. Can be overridden using `switches::kSearchEngineChoiceCountry`.
// See `//components/country_codes` for the Country ID format.
int GetSearchEngineChoiceCountryId(PrefService& profile_prefs);

// Returns whether the provided `country_id` is eligible for the EEA default
// search engine choice prompt.
// See `//components/country_codes` for the Country ID format.
bool IsEeaChoiceCountry(int country_id);

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_UTILS_H_
