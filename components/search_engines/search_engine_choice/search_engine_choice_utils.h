// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UTILS_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_metrics.h"  // IWYU pragma: export
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url.h"

class PrefService;
class SearchTermsData;
struct TemplateURLData;

namespace base {
class Time;
}  // namespace base

namespace regional_capabilities {
class RegionalCapabilitiesService;
}  // namespace regional_capabilities

namespace search_engines {

inline constexpr char
    kSearchEngineChoiceScreenProfileInitConditionsHistogram[] =
        "Search.ChoiceScreenProfileInitConditions";
inline constexpr char kPumaSearchChoiceScreenProfileInitConditionsHistogram[] =
    "PUMA.RegionalCapabilities.Search.ChoiceScreenProfileInitConditions";
inline constexpr char kSearchEngineChoiceScreenNavigationConditionsHistogram[] =
    "Search.ChoiceScreenNavigationConditions";
inline constexpr char kPumaSearchChoiceScreenNavigationConditionsHistogram[] =
    "PUMA.RegionalCapabilities.Search.ChoiceScreenNavigationConditions";
inline constexpr char kChoiceScreenProfileInitConditionsPostRestoreHistogram[] =
    "Search.ChoiceScreenProfileInitConditions.PostRestore";
inline constexpr char kChoiceScreenNavigationConditionsPostRestoreHistogram[] =
    "Search.ChoiceScreenNavigationConditions.PostRestore";
inline constexpr char kSearchEngineChoiceScreenEventsHistogram[] =
    "Search.ChoiceScreenEvents";
inline constexpr char kPumaSearchChoiceScreenEventsHistogram[] =
    "PUMA.RegionalCapabilities.Search.ChoiceScreenEvents";
inline constexpr char kChoiceScreenEventsPostRestoreHistogram[] =
    "Search.ChoiceScreenEvents.PostRestore";
inline constexpr char
    kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram[] =
        "Search.ChoiceScreenDefaultSearchEngineType";
inline constexpr char
    kPumaSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram[] =
        "PUMA.RegionalCapabilities.Search.ChoiceScreenDefaultSearchEngineType";
inline constexpr char
    kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram[] =
        "Search.ChoiceScreenDefaultSearchEngineType2";
inline constexpr char
    kPumaSearchEngineChoiceScreenDefaultSearchEngineType2Histogram[] =
        "PUMA.RegionalCapabilities.Search.ChoiceScreenDefaultSearchEngineType2";
inline constexpr char kSearchEngineChoiceScreenSelectedEngineIndexHistogram[] =
    "Search.ChoiceScreenSelectedEngineIndex";
inline constexpr char kPumaSearchChoiceScreenSelectedEngineIndexHistogram[] =
    "PUMA.RegionalCapabilities.Search.ChoiceScreenSelectedEngineIndex";
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
inline constexpr char kSearchEngineChoiceCompletedOnMonthHistogram[] =
    "Search.ChoiceCompletedOnMonth.OnProfileLoad2";

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
// LINT.ThenChange(/tools/metrics/histograms/metadata/search/enums.xml:SearchEngineChoiceScreenEvents)

// The cause for wiping the search engine choice preferences. Only used for
// metrics.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SearchEngineChoiceWipeReason {
  kProfileWipe = 0,
  kMissingMetadataVersion = 1,
  kInvalidMetadataVersion = 2,
  kFinchBasedReprompt = 3,
  kCommandLineFlag = 4,
  // kDeviceRestored = 5, // Deprecated
  kInvalidMetadata = 6,
  kMissingDefaultSearchEngine = 7,
  kChoiceRemadeAfterImport = 8,

  kMaxValue = kChoiceRemadeAfterImport,
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
      country_codes::CountryId country_id,
      bool is_current_default_search_presented,
      bool includes_non_regional_set_engine,
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
  const country_codes::CountryId country_id;

  // Whether the choice screen indicated which search provider was set as
  // default at the time it was shown.
  const bool is_current_default_search_presented;

  // Whether the choice screen included another engine that is not normally part
  // of the standard set for this region. This is expected to be used to include
  // the current default.
  const bool includes_non_regional_set_engine;
};

// Contains basic information about the search engine choice screen, notably
// the list of actual search engines to show, and other metadata associated
// with how it was determined.
class ChoiceScreenData {
 public:
  ChoiceScreenData(TemplateURL::OwnedTemplateURLVector owned_template_urls,
                   const TemplateURL* current_default_to_highlight,
                   country_codes::CountryId country_id,
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

  // When non-nullptr, designates the search engine to highlight on the choice
  // screen. Null values might indicate that the highlight feature is disabled
  // or that there is nothing to highlight because of an issue identifying the
  // right entry.
  const TemplateURL* current_default_to_highlight() const {
    return current_default_to_highlight_;
  }

 private:
  const TemplateURL::OwnedTemplateURLVector search_engines_;

  const ChoiceScreenDisplayState display_state_;

  const raw_ptr<const TemplateURL> current_default_to_highlight_;
};

// Records the type of the default search engine that was chosen by the user
// in the search engine choice screen or in the settings page.
void RecordChoiceScreenDefaultSearchProviderType(
    SearchEngineType engine_type,
    ChoiceMadeLocation choice_location);

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
                                 SearchEngineChoiceWipeReason reason);

struct ChoiceCompletionMetadata {
  enum class ParseError {
    kAbsent,
    kMissingVersion,
    kInvalidVersion,
    kMissingTimestamp,
    kNullTimestamp,
    kInvalidProgram,
  };

  base::Time timestamp;
  base::Version version;
  int serialized_program;
};

base::expected<ChoiceCompletionMetadata, ChoiceCompletionMetadata::ParseError>
GetChoiceCompletionMetadata(const PrefService& prefs);

// Creates a `ChoiceCompletionMetadata` with the specified program and current
// timestamp and version.
ChoiceCompletionMetadata CreateChoiceCompletionMetadataWithProgram(
    int serialized_program);

// Creates a `ChoiceCompletionMetadata` for the current state by getting the
// active program from `regional_capabilities_service`.
ChoiceCompletionMetadata CreateChoiceCompletionMetadataForCurrentState(
    regional_capabilities::RegionalCapabilitiesService&
        regional_capabilities_service);

// Persists the choice completion metadata to prefs.
void SetChoiceCompletionMetadata(PrefService& prefs,
                                 ChoiceCompletionMetadata metadata);

// Returns the timestamp of search engine choice screen. No value if no choice
// has been made.
std::optional<base::Time> GetChoiceScreenCompletionTimestamp(
    PrefService& prefs);

void ClearSearchEngineChoiceInvalidation(PrefService& prefs);

bool IsSearchEngineChoiceInvalid(const PrefService& prefs);

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UTILS_H_
