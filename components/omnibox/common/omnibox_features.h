// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_
#define COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace omnibox {

// Please do not add more features to this "big blob" list.
// Instead, use the categorized and alphabetized lists below this "big blob".
// You can create a new category if none of the existing ones fit.
extern const base::Feature kOmniboxTabSwitchSuggestions;
extern const base::Feature kExperimentalKeywordMode;
extern const base::Feature kImageSearchSuggestionThumbnail;
extern const base::Feature kDisplayTitleForCurrentUrl;
extern const base::Feature kOmniboxRemoveSuggestionsFromClipboard;

// Flags that affect the "twiddle" step of AutocompleteResult, i.e. SortAndCull.
// TODO(tommycli): There are more flags above that belong in this category.
extern const base::Feature kOmniboxDemoteByType;

// Features below this line should be sorted alphabetically by their comments.

// Num suggestions - these affect how many suggestions are shown based on e.g.
// focus, page context, provider, or URL v non-URL.
extern const base::Feature kMaxZeroSuggestMatches;
extern const base::Feature kUIExperimentMaxAutocompleteMatches;
// The default value is established here as a bool so it can be referred to in
// OmniboxFieldTrial.
extern const bool kOmniboxMaxURLMatchesEnabledByDefault;
extern const base::Feature kOmniboxMaxURLMatches;
extern const base::Feature kDynamicMaxAutocomplete;

// On-Focus Suggestions a.k.a. ZeroSuggest.
extern const base::Feature kClobberTriggersContextualWebZeroSuggest;
extern const base::Feature kClobberTriggersSRPZeroSuggest;
extern const base::Feature kOmniboxLocalZeroSuggestAgeThreshold;
extern const base::Feature kOmniboxTrendingZeroPrefixSuggestionsOnNTP;
extern const base::Feature kOnFocusSuggestionsContextualWeb;
extern const base::Feature kOnFocusSuggestionsContextualWebAllowSRP;
extern const base::Feature kOnFocusSuggestionsContextualWebOnContent;
extern const base::Feature kLocalHistoryZeroSuggest;
extern const base::Feature kZeroSuggestPrefetching;
// Related, kMaxZeroSuggestMatches.

// On Device Head Suggest.
extern const base::Feature kOnDeviceHeadProviderIncognito;
extern const base::Feature kOnDeviceHeadProviderNonIncognito;

// Provider-specific - These features change the behavior of specific providers.
extern const base::Feature kOmniboxExperimentalSuggestScoring;
extern const base::Feature kHistoryQuickProviderAblateInMemoryURLIndexCacheFile;
extern const base::Feature kDisableCGIParamMatching;
extern const base::Feature kShortBookmarkSuggestions;
extern const base::Feature kShortBookmarkSuggestionsByTotalInputLength;
extern const base::Feature kPreserveLongerShortcutsText;
extern const base::Feature kAggregateShortcuts;
extern const base::Feature kBookmarkPaths;

// Document provider
extern const base::Feature kDocumentProvider;
extern const base::Feature kDebounceDocumentProvider;
extern const base::Feature kDocumentProviderAso;

// Suggestions UI - these affect the UI or function of the suggestions popup.
extern const base::Feature kAdaptiveSuggestionsCount;
extern const base::Feature kClipboardSuggestionContentHidden;
extern const base::Feature kMostVisitedTiles;
extern const base::Feature kRichAutocompletion;
extern const base::Feature kNtpRealboxPedals;
extern const base::Feature kNtpRealboxSuggestionAnswers;
extern const base::Feature kNtpRealboxTailSuggest;
extern const base::Feature kOmniboxFuzzyUrlSuggestions;
extern const base::Feature kOmniboxPedalsAndroidBatch1;
extern const base::Feature kOmniboxPedalsBatch2NonEnglish;
extern const base::Feature kOmniboxPedalsBatch3NonEnglish;
extern const base::Feature kOmniboxPedalsTranslationConsole;
extern const base::Feature kWebUIOmniboxPopup;

// Omnibox UI - these affect the UI or function of the location bar (not the
// popup).
extern const base::Feature kOmniboxAssistantVoiceSearch;

// Omnibox & Suggestions UI - these affect both the omnibox and the suggestions
// popup.
extern const base::Feature kClosePopupWithEscape;
extern const base::Feature kBlurWithEscape;

// Settings Page - these affect the appearance of the Search Engines settings
// page
extern const base::Feature kActiveSearchEngines;
extern const base::Feature kSiteSearchStarterPack;

// Experiment to introduce new security indicators for HTTPS.
extern const base::Feature kUpdatedConnectionSecurityIndicators;

// Navigation experiments.
extern const base::Feature kDefaultTypedNavigationsToHttps;
extern const char kDefaultTypedNavigationsToHttpsTimeoutParam[];
extern const base::Feature kOmniboxSpareRenderer;

// Omnibox Logging.
extern const base::Feature kReportAssistedQueryStats;
extern const base::Feature kReportSearchboxStats;

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_
