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
class TemplateURLService;

namespace search_engines {

extern const char kSearchEngineChoiceScreenProfileInitConditionsHistogram[];
extern const char kSearchEngineChoiceScreenNavigationConditionsHistogram[];
extern const char kSearchEngineChoiceScreenEventsHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SearchEngineChoiceScreenConditions {
  // The user has a custom search engine set.
  kHasCustomSearchEngine = 0,
  // The user has a search provider list override.
  kSearchProviderOverride = 1,
  // The user is not in the regional scope.
  kNotInRegionalScope = 2,
  // A policy sets the default search engine or disables search altogether.
  kControlledByPolicy = 3,
  // The profile is out of scope.
  kProfileOutOfScope = 4,
  // An extension controls the default search engine.
  kExtensionContolled = 5,
  // The user is eligible to see the screen at the next opportunity.
  kEligible = 6,
  // The choice has already been completed.
  kAlreadyCompleted = 7,
  kMaxValue = kAlreadyCompleted,
};

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

enum class ChoicePromo {
  // Any path of getting the choice screen.
  kAny = 0,
  // Showing the screen to existing users in a dialog.
  kDialog = 1,
  // Showing to new users in the First Run Experience.
  kFre = 2,
};

// Whether the choice screen flag is generally enabled for the specific flow.
bool IsChoiceScreenFlagEnabled(ChoicePromo promo);

// Returns which version of the settings screen for the default search engine
// setting should be shown.
bool ShouldShowUpdatedSettings(PrefService& profile_prefs);

// Returns whether the search engine choice screen can be displayed or not based
// on device policies and profile properties.
// TODO(b/302687046): Change `template_url_service` to a reference and remove
// default value.
bool ShouldShowChoiceScreen(const policy::PolicyService& policy_service,
                            const ProfileProperties& profile_properties,
                            TemplateURLService* template_url_service = nullptr);

// Returns the country ID to use in the context of any search engine choice
// logic. If `profile_prefs` are null, returns
// `country_codes::GetCurrentCountryID()`. Can be overridden using
// `switches::kSearchEngineChoiceCountry`. See `//components/country_codes` for
// the Country ID format.
int GetSearchEngineChoiceCountryId(PrefService* profile_prefs);

// Returns whether the provided `country_id` is eligible for the EEA default
// search engine choice prompt.
// See `//components/country_codes` for the Country ID format.
bool IsEeaChoiceCountry(int country_id);

// Records the specified choice screen condition at profile initialization.
void RecordChoiceScreenProfileInitCondition(
    SearchEngineChoiceScreenConditions event);

// Records the specified choice screen event.
void RecordChoiceScreenEvent(SearchEngineChoiceScreenEvents event);
}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_UTILS_H_
