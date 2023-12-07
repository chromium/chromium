// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_UTILS_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "components/search_engines/search_engine_type.h"

namespace policy {
class PolicyService;
}

class PrefService;
class TemplateURLService;

namespace search_engines {

extern const char kSearchEngineChoiceScreenProfileInitConditionsHistogram[];
extern const char kSearchEngineChoiceScreenNavigationConditionsHistogram[];
extern const char kSearchEngineChoiceScreenEventsHistogram[];
extern const char kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram[];

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
  kExtensionControlled = 5,
  // The user is eligible to see the screen at the next opportunity.
  kEligible = 6,
  // The choice has already been completed.
  kAlreadyCompleted = 7,
  // The browser type is unsupported.
  kUnsupportedBrowserType = 8,
  // The feature can't run, it is disabled by local or remote configuration.
  kFeatureSuppressed = 9,
  // Some other dialog is showing and interfering with the choice one.
  kSuppressedByOtherDialog = 10,
  kMaxValue = kSuppressedByOtherDialog,
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
  // The "Learn more" screen was displayed on the non-FRE screen.
  kLearnMoreWasDisplayed = 5,
  // The "Learn more" screen was displayed on the FRE-specific screen.
  kFreLearnMoreWasDisplayed = 6,
  kMaxValue = kFreLearnMoreWasDisplayed,
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

//  The location from which the default search engine was set.
//  These values are persisted to logs. Entries should not be renumbered and
//  numeric values should never be reused.
//  Must be kept in sync with the ChoiceMadeLocation enum in
//  search_engines_browser_proxy.ts
enum class ChoiceMadeLocation {
  // `chrome://settings/search`
  kSearchSettings = 0,
  // `chrome://settings/searchEngines`
  // This value is also used for the settings pages on mobile.
  kSearchEngineSettings = 1,
  // The search engine choice dialog for existing users or the profile picker
  // for new users.
  kChoiceScreen = 2,
  kMaxValue = kChoiceScreen,
};

// Whether the choice screen flag is generally enabled for the specific flow.
bool IsChoiceScreenFlagEnabled(ChoicePromo promo);

// Returns which version of the settings screen for the default search engine
// setting should be shown.
// TODO(b/306367986): Restrict this function to iOS.
bool ShouldShowUpdatedSettings(PrefService& profile_prefs);

// Returns whether the search engine choice screen can be displayed or not based
// on device policies and profile properties.
// TODO(b/306367986): Restrict this function to iOS.
bool ShouldShowChoiceScreen(const policy::PolicyService& policy_service,
                            const ProfileProperties& profile_properties,
                            TemplateURLService* template_url_service);

// Returns the choice screen eligibility condition most relevant for the profile
// associated with `profile_prefs` and `template_url_service`.
// Only checks dynamic conditions, that can change from one call to the other
// during a profile's lifetime. Should be checked right before showing a choice
// screen.
SearchEngineChoiceScreenConditions GetDynamicChoiceScreenConditions(
    const PrefService& profile_prefs,
    const TemplateURLService& template_url_service);

// Returns the choice screen eligibility condition most relevant for the profile
// described by `profile_properties`.
// Only checks static conditions, such that if a non-eligible condition is
// returned, it would take at least a restart for the state to change. So this
// state can be checked and cached ahead of showing a choice screen.
SearchEngineChoiceScreenConditions GetStaticChoiceScreenConditions(
    const policy::PolicyService& policy_service,
    const ProfileProperties& profile_properties,
    const TemplateURLService& template_url_service);

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

// Records that the choice was made by settings the timestamp if applicable.
// Records the location from which the choice was made and the search engine
// that was chosen.
// The function should be called after the default search engine has been set.
// TODO(b/307713013): Remove the default value for `template_url_service` once
// the function is used on the iOS side.
void RecordChoiceMade(PrefService* profile_prefs,
                      ChoiceMadeLocation choice_location,
                      TemplateURLService* template_url_service = nullptr);

// Records the specified choice screen condition at profile initialization.
void RecordChoiceScreenProfileInitCondition(
    SearchEngineChoiceScreenConditions event);

// Records the specified choice screen event.
void RecordChoiceScreenEvent(SearchEngineChoiceScreenEvents event);

// Records the type of the default search engine that was chosen by the user
// in the search engine choice screen or in the settings page.
void RecordChoiceScreenDefaultSearchProviderType(SearchEngineType engine_type);

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_UTILS_H_
