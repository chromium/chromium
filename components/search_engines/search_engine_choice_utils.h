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

extern const char kSearchEngineChoiceScreenEventsHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SearchEngineChoiceScreenEvents {
  kUnknown = 0,
  // The non-FRE version of the choice screen was displayed.
  kChoiceScreenWasDisplayed = 1,
  // The FRE-specific flavor of the screen was displayed.
  kFreChoiceScreenWasDisplayed = 2,
  // The user clicked/tapped `Set as default` on the non-FRE screen.
  kDefaultWasSet = 3,
  // The user clicked/tapped `Set as default` on the
  // FRE-specific screen.
  kFreDefaultWasSet = 4,
  kMaxValue = kFreDefaultWasSet,
};

// Profile properties that need to be passed to
// `ShouldShowChoiceScreen`. This is due to the fact that
// the 'Profile' class is different between platforms.
struct ProfileProperties {
  bool is_regular_profile = false;
  raw_ptr<PrefService> pref_service;
};

// Returns which version of the settings screen for the default search engine
// setting should be shown.
bool ShouldShowUpdatedSettings(PrefService& profile_prefs);

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

// Records the specified choice screen event.
void RecordChoiceScreenEvent(SearchEngineChoiceScreenEvents event);
}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_UTILS_H_
