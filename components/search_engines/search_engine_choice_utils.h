// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_UTILS_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_UTILS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/search_engine_type.h"

class PrefService;
struct TemplateURLData;

namespace search_engines {

extern const char kSearchEngineChoiceScreenProfileInitConditionsHistogram[];
extern const char kSearchEngineChoiceScreenNavigationConditionsHistogram[];
extern const char kSearchEngineChoiceScreenEventsHistogram[];
extern const char kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram[];
extern const char kSearchEngineChoiceWipeReasonHistogram[];
extern const char kSearchEngineChoiceRepromptHistogram[];
extern const char kSearchEngineChoiceRepromptWildcardHistogram[];
extern const char kSearchEngineChoiceRepromptSpecificCountryHistogram[];
extern const char kSearchEngineChoiceUnexpectedIdHistogram[];
extern const char kSearchEngineChoiceIsDefaultProviderAddedToChoicesHistogram[];

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
  // The browser window can't fit the dialog's smallest variant.
  kBrowserWindowTooSmall = 11,
  // The user has a distribution custom search engine set as default.
  kHasDistributionCustomSearchEngine = 12,
  // The user has an unknown prepopulated search engine set as default.
  kHasRemovedPrepopulatedSearchEngine = 13,

  kMaxValue = kHasRemovedPrepopulatedSearchEngine,
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
  // The profile creation specific flavor of the screen was displayed.
  kProfileCreationChoiceScreenWasDisplayed = 7,
  // The user clicked `Set as default` on the profile creation specific screen.
  kProfileCreationDefaultWasSet = 8,
  // The "Learn more" screen was displayed on the profile creation specific
  // screen.
  kProfileCreationLearnMoreDisplayed = 9,
  kMaxValue = kProfileCreationLearnMoreDisplayed,
};

enum class ChoicePromo {
  // Any path of getting the choice screen.
  kAny = 0,
  // Showing the screen to existing users in a dialog.
  kDialog = 1,
  // Showing to new users in the First Run Experience.
  kFre = 2,
};

// The cause for wiping the search engine choice preferences. Only used for
// metrics.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WipeSearchEngineChoiceReason {
  kProfileWipe = 0,
  kMissingChoiceVersion = 1,
  kInvalidChoiceVersion = 2,
  kReprompt = 3,

  kMaxValue = kReprompt,
};

// Exposed for testing.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RepromptResult {
  // Reprompt.
  kReprompt = 0,

  // Cases below do not reprompt.
  //
  // Wrong JSON syntax.
  kInvalidDictionary = 1,
  // There was no applicable key (specific country or wildcard).
  kNoDictionaryKey = 2,
  // The reprompt version could not be parsed.
  kInvalidVersion = 3,
  // Chrome older than the requested version, reprompting would not make the
  // version recent enough.
  kChromeTooOld = 4,
  // The choice was made recently enough.
  kRecentChoice = 5,

  kMaxValue = kRecentChoice,
};

// Whether the choice screen flag is generally enabled for the specific flow.
// TODO(b/318824817): To be removed post-launch.
bool IsChoiceScreenFlagEnabled(ChoicePromo promo);

// Returns whether the provided `country_id` is eligible for the EEA default
// search engine choice prompt.
// See `//components/country_codes` for the Country ID format.
bool IsEeaChoiceCountry(int country_id);

// Records the specified choice screen condition at profile initialization.
void RecordChoiceScreenProfileInitCondition(
    SearchEngineChoiceScreenConditions event);

// Records the specified choice screen event.
void RecordChoiceScreenEvent(SearchEngineChoiceScreenEvents event);

// Records the type of the default search engine that was chosen by the user
// in the search engine choice screen or in the settings page.
void RecordChoiceScreenDefaultSearchProviderType(SearchEngineType engine_type);

// For debugging purposes, record the ID of the current default search engine
// that does not exist in the prepopulated search providers data.
void RecordUnexpectedSearchProvider(const TemplateURLData& data);

// For debugging purposes, record whether the current default search engine
// was inserted in the list of search engines to show in the choice screen.
void RecordIsDefaultProviderAddedToChoices(bool inserted_default);

// Clears the search engine choice prefs, such as the timestamp and the Chrome
// version, to ensure the choice screen is shown again.
void WipeSearchEngineChoicePrefs(PrefService& profile_prefs,
                                 WipeSearchEngineChoiceReason reason);

#if !BUILDFLAG(IS_ANDROID)
// Returns the engine marketing snippet string resource id or -1 if the snippet
// was not found.
// The function definition is generated in `generated_marketing_snippets.cc`.
// `engine_keyword` is the search engine keyword.
int GetMarketingSnippetResourceId(const std::u16string& engine_keyword);

// Returns the marketing snippet string or the fallback string if the search
// engine didn't provide its own.
std::u16string GetMarketingSnippetString(
    const TemplateURLData& template_url_data);

// Returns the resource ID for the icon associated with `engine_keyword`, or -1
// if not found. All search engines prepopulated in EEA countries are guaranteed
// to have an icon.
// The function definition is generated by `generate_search_engine_icons.py`in
// `generated_search_engine_resource_ids.cc`.
int GetIconResourceId(const std::u16string& engine_keyword);

#endif

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_UTILS_H_
