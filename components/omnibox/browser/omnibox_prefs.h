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
inline constexpr char kGroupIdToggledOffHistogram[] =
    "Omnibox.GroupId.ToggledOff";
inline constexpr char kGroupIdToggledOnHistogram[] =
    "Omnibox.GroupId.ToggledOn";

// Alphabetical list of preference names specific to the omnibox component.
// Keep alphabetized, and document each.

// A client-side toggle for document (Drive) suggestions.
// Also gated by a feature and server-side Admin Panel controls.
inline constexpr char kDocumentSuggestEnabled[] = "documentsuggest.enabled";

// Enum specifying the active behavior for the intranet redirect detector.
// The browser pref kDNSInterceptionChecksEnabled also impacts the redirector.
// Values are defined in omnibox::IntranetRedirectorBehavior.
inline constexpr char kIntranetRedirectBehavior[] =
    "browser.intranet_redirect_behavior";

// Boolean that controls whether scoped search mode can be triggered by <space>.
inline constexpr char kKeywordSpaceTriggeringEnabled[] =
    "omnibox.keyword_space_triggering_enabled";

// A dictionary of visibility preferences for suggestion groups. The key is the
// suggestion group ID serialized as a string, and the value is
// SuggestionGroupVisibility serialized as an integer.
inline constexpr char kSuggestionGroupVisibility[] =
    "omnibox.suggestionGroupVisibility";

// Boolean that specifies whether to show the LensOverlay entry point.
inline constexpr char kShowGoogleLensShortcut[] =
    "omnibox.show_google_lens_shortcut";

// Boolean that specifies whether to always show full URLs in the omnibox.
inline constexpr char kPreventUrlElisionsInOmnibox[] =
    "omnibox.prevent_url_elisions";

// A cache of NTP zero suggest results using a JSON dictionary serialized into a
// string.
inline constexpr char kZeroSuggestCachedResults[] = "zerosuggest.cachedresults";

// A cache of SRP/Web zero suggest results using a JSON dictionary serialized
// into a string keyed off the page URL.
inline constexpr char kZeroSuggestCachedResultsWithURL[] =
    "zerosuggest.cachedresults_with_url";

// Booleans that specify whether various IPH suggestions have been dismissed.
inline constexpr char kDismissedGeminiIph[] = "omnibox.dismissed_gemini_iph";
inline constexpr char kDismissedFeaturedEnterpriseSiteSearchIphPrefName[] =
    "omnibox.dismissed_featured_enterprise_search_iph";
inline constexpr char kDismissedHistoryEmbeddingsSettingsPromo[] =
    "omnibox.dismissed_history_embeddings_settings_promo";
inline constexpr char kDismissedHistoryScopePromo[] =
    "omnibox.dismissed_history_scope_promo";
inline constexpr char kDismissedHistoryEmbeddingsScopePromo[] =
    "omnibox.dismissed_history_embeddings_scope_promo";

// How many times the various IPH suggestions were shown.
inline constexpr char kShownCountGeminiIph[] = "omnibox.shown_count_gemini_iph";
inline constexpr char kShownCountFeaturedEnterpriseSiteSearchIph[] =
    "omnibox.shown_count_featured_enterprise_search_iph";
inline constexpr char kShownCountHistoryEmbeddingsSettingsPromo[] =
    "omnibox.shown_count_history_embeddings_settings_promo";
inline constexpr char kShownCountHistoryScopePromo[] =
    "omnibox.shown_count_history_scope_promo";
inline constexpr char kShownCountHistoryEmbeddingsScopePromo[] =
    "omnibox.shown_count_history_embeddings_scope_promo";

// Many of the prefs defined above are registered locally where they're used.
// New prefs should be added here and ordered the same as they're defined above.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns the stored visibility preference for |suggestion_group_id|.
// If |suggestion_group_id| has never been manually hidden or shown by the user,
// this method returns SuggestionGroupVisibility::DEFAULT.
//
// Warning: UI code should use OmniboxController::IsSuggestionGroupHidden()
// instead, which passes the server-provided group ID to this method and takes
// the server-provided hint on default visibility of the group into account.
SuggestionGroupVisibility GetUserPreferenceForSuggestionGroupVisibility(
    const PrefService* prefs,
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
