// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREFS_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREFS_H_

#include <string>

class PrefRegistrySimple;
class PrefService;

namespace omnibox {

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

// Enum specifying the active behavior for the intranet redirect detector.
// The browser pref kDNSInterceptionChecksEnabled also impacts the redirector.
// Values are defined in omnibox::IntranetRedirectorBehavior.
inline constexpr char kIntranetRedirectBehavior[] =
    "browser.intranet_redirect_behavior";

// Boolean that controls whether scoped search mode can be triggered by <space>.
inline constexpr char kKeywordSpaceTriggeringEnabled[] =
    "omnibox.keyword_space_triggering_enabled";

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
inline constexpr char kFocusedSrpWebCount[] = "omnibox.focused_srp_web_count";
inline constexpr char kAIModeSearchSuggestSettings[] =
    "omnibox.ai_mode_search_suggest_settings";
inline constexpr char kAIModeSettings[] = "omnibox.ai_mode_settings";

// Many of the prefs defined above are registered locally where they're used.
// New prefs should be added here and ordered the same as they're defined above.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

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

// Returns true if the MIA is allowed per the policy.
bool IsMiaAllowedByPolicy(PrefService* prefs);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREFS_H_
