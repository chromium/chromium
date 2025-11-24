// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREF_NAMES_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREF_NAMES_H_

namespace omnibox {

// Alphabetical list of preference names specific to the omnibox component.
// Keep alphabetized, and document each.

// An integer pref to store the last day the AIM hint was shown. The day is
// represented as the number of days since the Unix epoch.
inline constexpr char kAimHintLastImpressionDay[] =
    "omnibox.aim_hint_last_impression_day";

// An integer pref to store the number of times the AIM hint has been shown on
// the day in kAimHintLastImpressionDay.
inline constexpr char kAimHintDailyImpressionsCount[] =
    "omnibox.aim_hint_daily_impressions_count";

// An integer pref to store the total number of times the AIM hint has been
// shown.
inline constexpr char kAimHintTotalImpressions[] =
    "omnibox.aim_hint_total_impressions";

// An integer pref stores whether browser AIM features are enabled. E.g.
// omnibox and NTP AIM entrypoints. Controlled by an admin policy.
inline constexpr char kAIModeSettings[] = "omnibox.ai_mode_settings";

// LINT.IfChange(TipsPrefNames)
// Boolean for whether bottom omnibox was ever used.
inline constexpr char kBottomOmniboxEverUsed[] =
    "omnibox.bottom_omnibox_ever_used";
// LINT.ThenChange(//chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/ToolbarPositionController.java:TipsPrefNames)

// Booleans that specify whether various IPH suggestions have been dismissed.
inline constexpr char kDismissedEnterpriseSearchAggregatorIphPrefName[] =
    "omnibox.dismissed_enterprise_search_aggregator_iph";
inline constexpr char kDismissedFeaturedEnterpriseSiteSearchIphPrefName[] =
    "omnibox.dismissed_featured_enterprise_search_iph";
inline constexpr char kDismissedGeminiIph[] = "omnibox.dismissed_gemini_iph";
inline constexpr char kDismissedHistoryEmbeddingsScopePromo[] =
    "omnibox.dismissed_history_embeddings_scope_promo";
inline constexpr char kDismissedHistoryEmbeddingsSettingsPromo[] =
    "omnibox.dismissed_history_embeddings_settings_promo";
inline constexpr char kDismissedHistoryScopePromo[] =
    "omnibox.dismissed_history_scope_promo";

inline constexpr char kFocusedSrpWebCount[] = "omnibox.focused_srp_web_count";

// Boolean that specifies whether the omnibox should be positioned at the bottom
// of the screen.
inline constexpr char kIsOmniboxInBottomPosition[] =
    "omnibox.is_in_bottom_position";

// Enum specifying the active behavior for the intranet redirect detector.
// The browser pref kDNSInterceptionChecksEnabled also impacts the redirector.
// Values are defined in omnibox::IntranetRedirectorBehavior.
inline constexpr char kIntranetRedirectBehavior[] =
    "browser.intranet_redirect_behavior";

// Boolean that controls whether scoped search mode can be triggered by <space>.
inline constexpr char kKeywordSpaceTriggeringEnabled[] =
    "omnibox.keyword_space_triggering_enabled";

// Boolean that specifies whether to always show full URLs in the omnibox.
inline constexpr char kPreventUrlElisionsInOmnibox[] =
    "omnibox.prevent_url_elisions";

// Boolean that specifies whether to show the LensOverlay entry point.
inline constexpr char kShowGoogleLensShortcut[] =
    "omnibox.show_google_lens_shortcut";

// Boolean that specifies whether to show the AI Mode omnibox button.
inline constexpr char kShowAiModeOmniboxButton[] =
    "omnibox.show_ai_mode_omnibox_button";

// Boolean that specifies whether to show the search tools at the bottom of the
// omnibox.
inline constexpr char kShowSearchTools[] = "omnibox.show_search_tools";

// How many times the various IPH suggestions were shown.
inline constexpr char kShownCountEnterpriseSearchAggregatorIph[] =
    "omnibox.shown_count_enterprise_search_aggregator_iph";
inline constexpr char kShownCountFeaturedEnterpriseSiteSearchIph[] =
    "omnibox.shown_count_featured_enterprise_search_iph";
inline constexpr char kShownCountGeminiIph[] = "omnibox.shown_count_gemini_iph";
inline constexpr char kShownCountHistoryEmbeddingsScopePromo[] =
    "omnibox.shown_count_history_embeddings_scope_promo";
inline constexpr char kShownCountHistoryEmbeddingsSettingsPromo[] =
    "omnibox.shown_count_history_embeddings_settings_promo";
inline constexpr char kShownCountHistoryScopePromo[] =
    "omnibox.shown_count_history_scope_promo";

// A cache of NTP zero suggest results using a JSON dictionary serialized into a
// string.
inline constexpr char kZeroSuggestCachedResults[] = "zerosuggest.cachedresults";

// A cache of SRP/Web zero suggest results using a JSON dictionary serialized
// into a string keyed off the page URL.
inline constexpr char kZeroSuggestCachedResultsWithURL[] =
    "zerosuggest.cachedresults_with_url";

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PREF_NAMES_H_
