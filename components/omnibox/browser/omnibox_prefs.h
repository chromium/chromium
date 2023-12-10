// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREFS_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREFS_H_

#include <string>

class PrefRegistrySimple;
class PrefService;

namespace omnibox {

// Reflects the omnibox::GroupId enum values for the Polaris zero-prefix
// suggestions in //third_party/omnibox_proto/groups.proto for reporting in UMA.
//
// This enum is tied directly to the `GroupId` UMA enum in
// //tools/metrics/histograms/enums.xml and should always reflect it (do not
// change one without changing the other).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UMAGroupId {
  kUnknown = 0,
  kInvalid,
  kPreviousSearchRelated,
  kPreviousSearchRelatedEntityChips,
  kTrends,
  kTrendsEntityChips,
  kRelatedQueries,
  kVisitedDocRelated,

  kMaxValue = kVisitedDocRelated
};

// These values are persisted to prefs. They cannot be freely changed.
enum SuggestionGroupVisibility {
  // When DEFAULT is returned, the group's visibility should be controlled by
  // the server-provided hint.
  DEFAULT = 0,

  // HIDDEN means the user has manually hidden the group before, and so this
  // group should be hidden regardless of the server-provided hint.
  HIDDEN = 1,

  // SHOWN means the user has manually shown the group before, and so this
  // group should be shown regardless of the server-provided hint.
  SHOWN = 2,
};

// Histograms being recorded when visibility of suggestion group IDs change.
extern const char kGroupIdToggledOffHistogram[];
extern const char kGroupIdToggledOnHistogram[];

// Alphabetical list of preference names specific to the omnibox component.
// Keep alphabetized, and document each in the .cc file.
extern const char kDocumentSuggestEnabled[];
extern const char kIntranetRedirectBehavior[];
extern const char kKeywordSpaceTriggeringEnabled[];
extern const char kSuggestionGroupVisibility[];
extern const char kPreventUrlElisionsInOmnibox[];
extern const char kZeroSuggestCachedResults[];
extern const char kZeroSuggestCachedResultsWithURL[];
extern const char kOmniboxInstantKeywordUsed[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns the stored visibility preference for |suggestion_group_id|.
// If |suggestion_group_id| has never been manually hidden or shown by the user,
// this method returns SuggestionGroupVisibility::DEFAULT.
//
// Warning: UI code should use OmniboxController::IsSuggestionGroupHidden()
// instead, which passes the server-provided group ID to this method and takes
// the server-provided hint on default visibility of the group into account.
SuggestionGroupVisibility GetUserPreferenceForSuggestionGroupVisibility(
    PrefService* prefs,
    int suggestion_group_id);

// Sets the stored visibility preference for |suggestion_group_id| to
// |visibility|.
//
// Warning: UI code should use OmniboxController::SetSuggestionGroupHidden()
// instead, which passes the server-provided group ID to this method.
void SetUserPreferenceForSuggestionGroupVisibility(
    PrefService* prefs,
    int suggestion_group_id,
    SuggestionGroupVisibility visibility);

// Updates the ZPS dictionary preference to cache the given |response| value
// using the |page_url| as the cache key.
void SetUserPreferenceForZeroSuggestCachedResponse(PrefService* prefs,
                                                   const std::string& page_url,
                                                   const std::string& response);

// Returns the cached response from the ZPS dictionary preference associated
// with the given |page_url|.
std::string GetUserPreferenceForZeroSuggestCachedResponse(
    PrefService* prefs,
    const std::string& page_url);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREFS_H_
