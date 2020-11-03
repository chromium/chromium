// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_
#define COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_

#include "base/feature_list.h"

namespace omnibox {

// Please do not add more features to this "big blob" list.
// Instead, use the categorized and alphabetized lists below this "big blob".
// You can create a new category if none of the existing ones fit.
extern const base::Feature kHideFileUrlScheme;
extern const base::Feature kOmniboxShortBookmarkSuggestions;
extern const base::Feature kOmniboxTailSuggestions;
extern const base::Feature kOmniboxTabSwitchSuggestions;
extern const base::Feature kExperimentalKeywordMode;
extern const base::Feature kEnableClipboardProviderImageSuggestions;
extern const base::Feature kSearchProviderWarmUpOnFocus;
extern const base::Feature kDisplayTitleForCurrentUrl;
extern const base::Feature kUIExperimentSwapTitleAndUrl;
extern const base::Feature kSpeculativeServiceWorkerStartOnQueryInput;
extern const base::Feature kDocumentProvider;
extern const base::Feature kOmniboxDisableInstantExtendedLimit;
extern const base::Feature kOmniboxSearchEngineLogo;
extern const base::Feature kOmniboxRemoveSuggestionsFromClipboard;
extern const base::Feature kDebounceDocumentProvider;

// Flags that affect the "twiddle" step of AutocompleteResult, i.e. SortAndCull.
// TODO(tommycli): There are more flags above that belong in this category.
extern const base::Feature kOmniboxDemoteByType;

// A special flag, enabled by default, that can be used to disable all new
// search features (e.g. zero suggest).
extern const base::Feature kNewSearchFeatures;

// Features below this line should be sorted alphabetically by their comments.

// Entity suggestion features.
extern const base::Feature kEntitySuggestionsReduceLatency;

// Num suggestions - these affect how many suggestions are shown based on e.g.
// focus, page context, provider, or URL v non-URL.
// Note that all of these are overridden and default values used instead if
// kNewSearchFeatures is disabled.
extern const base::Feature kMaxZeroSuggestMatches;
extern const base::Feature kUIExperimentMaxAutocompleteMatches;
// The default value is established here as a bool so it can be referred to in
// OmniboxFieldTrial.
extern const bool kOmniboxMaxURLMatchesEnabledByDefault;
extern const base::Feature kOmniboxMaxURLMatches;
extern const base::Feature kDynamicMaxAutocomplete;

// Ranking
extern const base::Feature kBubbleUrlSuggestions;

// On-Focus Suggestions a.k.a. ZeroSuggest.
extern const base::Feature kClobberTriggersContextualWebZeroSuggest;
extern const base::Feature kOmniboxLocalZeroSuggestAgeThreshold;
extern const base::Feature kOmniboxLocalZeroSuggestForAuthenticatedUsers;
extern const base::Feature kOmniboxLocalZeroSuggestFrecencyRanking;
extern const base::Feature kOmniboxTrendingZeroPrefixSuggestionsOnNTP;
extern const base::Feature kOnFocusSuggestions;
extern const base::Feature kOmniboxZeroSuggestCaching;
extern const base::Feature kOnFocusSuggestionsContextualWeb;
extern const base::Feature kOnFocusSuggestionsContextualWebOnContent;
extern const base::Feature kReactiveZeroSuggestionsOnNTPOmnibox;
extern const base::Feature kReactiveZeroSuggestionsOnNTPRealbox;
extern const base::Feature kZeroSuggestionsOnNTP;
extern const base::Feature kZeroSuggestionsOnNTPRealbox;
extern const base::Feature kZeroSuggestionsOnSERP;
// Related, kMaxZeroSuggestMatches.

// On Device Head Suggest.
extern const base::Feature kOnDeviceHeadProviderIncognito;
extern const base::Feature kOnDeviceHeadProviderNonIncognito;

// Scoring - these affect how relevance scores are calculated for suggestions.
extern const base::Feature kOmniboxExperimentalSuggestScoring;
extern const base::Feature kHistoryQuickProviderAllowButDoNotScoreMidwordTerms;
extern const base::Feature kHistoryQuickProviderAllowMidwordContinuations;

// Suggestions UI - these affect the UI or function of the suggestions popup.
extern const base::Feature kAdaptiveSuggestionsCount;
extern const base::Feature kBookmarkPaths;
extern const base::Feature kCompactSuggestions;
extern const base::Feature kDeferredKeyboardPopup;
extern const base::Feature kMostVisitedTiles;
extern const base::Feature kRichAutocompletion;
extern const base::Feature kOmniboxSearchReadyIncognito;
extern const base::Feature kOmniboxSuggestionButtonRow;
extern const base::Feature kOmniboxPedalSuggestions;
extern const base::Feature kOmniboxKeywordSearchButton;
extern const base::Feature kOmniboxRefinedFocusState;
extern const base::Feature kOmniboxSuggestionsRecyclerView;
extern const base::Feature kOmniboxSuggestionsWrapAround;
extern const base::Feature kWebUIOmniboxPopup;

// Omnibox UI - these affect the UI or function of the location bar (not the
// popup).
extern const base::Feature kOmniboxAssistantVoiceSearch;

// Path-hiding experiments - these hide the path and other URL components in
// some circumstances in the steady-state omnibox.
extern const base::Feature kRevealSteadyStateUrlPathQueryAndRefOnHover;
extern const base::Feature kHideSteadyStateUrlPathQueryAndRefOnInteraction;
extern const base::Feature kMaybeElideToRegistrableDomain;

// Experiment to control whether visits from CCT are hidden.
// TODO(https://crbug.com/1141501): this is for an experiment, and will be
// removed once data is collected from experiment.
extern const base::Feature kHideVisitsFromCct;

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_
