// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UTILS_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class PrefService;
class SearchTermsData;
struct TemplateURLData;

namespace search_engines {

inline constexpr char
    kSearchEngineChoiceScreenProfileInitConditionsHistogram[] =
        "Search.ChoiceScreenProfileInitConditions";
inline constexpr char kSearchEngineChoiceScreenNavigationConditionsHistogram[] =
    "Search.ChoiceScreenNavigationConditions";
inline constexpr char kSearchEngineChoiceScreenEventsHistogram[] =
    "Search.ChoiceScreenEvents";
inline constexpr char
    kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram[] =
        "Search.ChoiceScreenDefaultSearchEngineType";
inline constexpr char kSearchEngineChoiceScreenSelectedEngineIndexHistogram[] =
    "Search.ChoiceScreenSelectedEngineIndex";
inline constexpr char
    kSearchEngineChoiceScreenShowedEngineAtHistogramPattern[] =
        "Search.ChoiceScreenShowedEngineAt.Index%d";
inline constexpr char
    kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram[] =
        "Search.ChoiceScreenShowedEngineAt.CountryMismatch";
inline constexpr char kSearchEngineChoiceWipeReasonHistogram[] =
    "Search.ChoiceWipeReason";
inline constexpr char kSearchEngineChoiceRepromptHistogram[] =
    "Search.ChoiceReprompt";
inline constexpr char kSearchEngineChoiceRepromptWildcardHistogram[] =
    "Search.ChoiceReprompt.Wildcard";
inline constexpr char kSearchEngineChoiceRepromptSpecificCountryHistogram[] =
    "Search.ChoiceReprompt.SpecificCountry";
inline constexpr char kSearchEngineChoiceUnexpectedIdHistogram[] =
    "Search.ChoiceDebug.UnexpectedSearchEngineId";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SearchEngineChoiceScreenConditions)
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
  // The user has an unknown (which we assume is because it has been removed)
  // prepopulated search engine set as default.
  kHasRemovedPrepopulatedSearchEngine = 13,
  // The user does not have Google as the default search engine.
  kHasNonGoogleSearchEngine = 14,
  // The user is eligible, the app could have presented a dialog but the
  // application was started via an external intent and the dialog skipped.
  kAppStartedByExternalIntent = 15,
  // The browser attempting to show the choice screen in a dialog is already
  // showing a choice screen.
  kAlreadyBeingShown = 16,
  // The user made the choice in the guest session and opted to save it across
  // guest sessions.
  kUsingPersistedGuestSessionChoice = 17,

  kMaxValue = kUsingPersistedGuestSessionChoice,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:SearchEngineChoiceScreenConditions)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SearchEngineChoiceScreenEvents)
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
  // The "More" button was clicked on the modal dialog.
  kMoreButtonClicked = 10,
  // The "More" button was clicked on the FRE-specific screen.
  kFreMoreButtonClicked = 11,
  // The "More" button was clicked on the profile creation specific screen.
  kProfileCreationMoreButtonClicked = 12,
  kMaxValue = kProfileCreationMoreButtonClicked,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:SearchEngineChoiceScreenEvents)

// The cause for wiping the search engine choice preferences. Only used for
// metrics.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WipeSearchEngineChoiceReason {
  kProfileWipe = 0,
  kMissingChoiceVersion = 1,
  kInvalidChoiceVersion = 2,
  kReprompt = 3,
  kCommandLineFlag = 4,

  kMaxValue = kCommandLineFlag,
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
  // Do not reprompt the users.
  kNoReprompt = 6,

  kMaxValue = kNoReprompt,
};

struct ChoiceScreenDisplayState {
 public:
  ChoiceScreenDisplayState(
      std::vector<SearchEngineType> search_engines,
      int country_id,
      std::optional<int> selected_engine_index = std::nullopt);
  ChoiceScreenDisplayState(const ChoiceScreenDisplayState& other);
  ~ChoiceScreenDisplayState();

  base::Value::Dict ToDict() const;
  static std::optional<ChoiceScreenDisplayState> FromDict(
      const base::Value::Dict& dict);

  // `SearchEngineType`s of the search engines displayed on the choice screen,
  // listed in an order matching their display order.
  // Note that the screen shows items from a list specific for each EEA
  // country, and these items are randomized before they are presented.
  const std::vector<SearchEngineType> search_engines;

  // Index of the selected search engine. Obviously can't be populated until
  // the choice has been made, so instances describing purely the state at the
  // time of the display won't have a value, while the ones describing it at
  // the moment of the choice will have it populated.
  std::optional<int> selected_engine_index;

  // The country used when generating the list. It should be the country
  // used to determine the set of search engines to show for the current
  // profile.
  const int country_id;
};

// Contains basic information about the search engine choice screen, notably
// the list of actual search engines to show, and other metadata associated
// with how it was determined.
class ChoiceScreenData {
 public:
  ChoiceScreenData(TemplateURL::OwnedTemplateURLVector owned_template_urls,
                   int country_id,
                   const SearchTermsData& search_terms_data);

  ChoiceScreenData(const ChoiceScreenData&) = delete;
  ChoiceScreenData& operator=(const ChoiceScreenData&) = delete;

  ~ChoiceScreenData();

  const TemplateURL::OwnedTemplateURLVector& search_engines() const {
    return search_engines_;
  }

  const ChoiceScreenDisplayState& display_state() const {
    return display_state_;
  }

 private:
  const TemplateURL::OwnedTemplateURLVector search_engines_;

  const ChoiceScreenDisplayState display_state_;
};

// The state of the search engine choice country command line override.
enum class SearchEngineCountryListOverride {
  // Display all the search engines used in the EEA region.
  kEeaAll,
  // Display the search engines that we default to when the country is unknown.
  kEeaDefault,
};

using SearchEngineCountryOverride =
    absl::variant<int, SearchEngineCountryListOverride>;

// Gets the search engine country command line override.
// Returns an int if the country id is passed to the command line or a
// `SearchEngineCountryListOverride` if the special values of
// `kDefaultListCountryOverride` or `kEeaListCountryOverride` are passed.
std::optional<SearchEngineCountryOverride> GetSearchEngineCountryOverride();

// Returns whether the search engine list is overridden in the command line to
// return the default list or the list of all eea engines.
bool HasSearchEngineCountryListOverride();

// Returns whether the provided `country_id` is eligible for the EEA default
// search engine choice prompt.
// See `//components/country_codes` for the Country ID format.
bool IsEeaChoiceCountry(int country_id);

// Records the specified choice screen condition at profile initialization.
void RecordChoiceScreenProfileInitCondition(
    SearchEngineChoiceScreenConditions event);

// Records the specified choice screen condition for relevant navigations.
void RecordChoiceScreenNavigationCondition(
    SearchEngineChoiceScreenConditions condition);

// Records the specified choice screen event.
void RecordChoiceScreenEvent(SearchEngineChoiceScreenEvents event);

// Records the type of the default search engine that was chosen by the user
// in the search engine choice screen or in the settings page.
void RecordChoiceScreenDefaultSearchProviderType(SearchEngineType engine_type);

// Records the index of the search engine that was chosen by the user as it was
// displayed on the choice screen.
void RecordChoiceScreenSelectedIndex(int selected_engine_index);

// Records the positions of search engines in a choice screen. Intended to be
// recorded when a choice happens, so that we emit it at most once per choice
// "scope". For more info, please see
// http://go/chrome-choice-screen-positions-histogram-design (Google-internal).
// Don't call this directly. Instead, go through
// `SearchEngineChoiceService::MaybeRecordChoiceScreenDisplayState()`.
void RecordChoiceScreenPositions(
    const std::vector<SearchEngineType>& displayed_search_engines);

// Records whether `RecordChoiceScreenPositions()` had to be skipped due to
// a mismatch between the Variations/UMA country and the profile/choice
// country. If `has_mismatch` is `true`, we record that it was "skipped", or
// that it was "not skipped" otherwise.
void RecordChoiceScreenPositionsCountryMismatch(bool has_mismatch);

// For debugging purposes, record the ID of the current default search engine
// that does not exist in the prepopulated search providers data.
void RecordUnexpectedSearchProvider(const TemplateURLData& data);

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

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UTILS_H_
