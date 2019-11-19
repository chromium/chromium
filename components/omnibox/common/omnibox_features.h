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
extern const base::Feature kHideSteadyStateUrlScheme;
extern const base::Feature kHideSteadyStateUrlTrivialSubdomains;
extern const base::Feature kHideSteadyStateUrlPathQueryAndRef;
extern const base::Feature kOneClickUnelide;
extern const base::Feature kSimplifyHttpsIndicator;
extern const base::Feature kOmniboxGroupSuggestionsBySearchVsUrl;
extern const base::Feature kOmniboxLocalEntitySuggestions;
extern const base::Feature kOmniboxMaxURLMatches;
extern const base::Feature kOmniboxRichEntitySuggestions;
extern const base::Feature kOmniboxReverseAnswers;
extern const base::Feature kOmniboxShortBookmarkSuggestions;
extern const base::Feature kOmniboxTailSuggestions;
extern const base::Feature kOmniboxTabSwitchSuggestions;
extern const base::Feature kOmniboxTabSwitchSuggestionsDedicatedRow;
extern const base::Feature kExperimentalKeywordMode;
extern const base::Feature kOmniboxPedalSuggestions;
extern const base::Feature kOmniboxSuggestionTransparencyOptions;
extern const base::Feature kEnableClipboardProviderTextSuggestions;
extern const base::Feature kEnableClipboardProviderImageSuggestions;
extern const base::Feature kSearchProviderWarmUpOnFocus;
extern const base::Feature kDisplayTitleForCurrentUrl;
extern const base::Feature kUIExperimentMaxAutocompleteMatches;
extern const base::Feature kQueryInOmnibox;
extern const base::Feature kUIExperimentShowSuggestionFavicons;
extern const base::Feature kUIExperimentSwapTitleAndUrl;
extern const base::Feature kSpeculativeServiceWorkerStartOnQueryInput;
extern const base::Feature kDocumentProvider;
extern const base::Feature kAutocompleteTitles;
extern const base::Feature kOmniboxPopupShortcutIconsInZeroState;
extern const base::Feature kOmniboxMaterialDesignWeatherIcons;
extern const base::Feature kOmniboxDisableInstantExtendedLimit;
extern const base::Feature kOmniboxSearchEngineLogo;
extern const base::Feature kOmniboxRemoveSuggestionsFromClipboard;
extern const base::Feature kOnDeviceHeadProvider;
extern const base::Feature kDebounceDocumentProvider;

// Flags that affect the "twiddle" step of AutocompleteResult, i.e. SortAndCull.
// TODO(tommycli): There are more flags above that belong in this category.
extern const base::Feature kOmniboxPreserveDefaultMatchScore;
extern const base::Feature kOmniboxPreserveDefaultMatchAgainstAsyncUpdate;
extern const base::Feature kOmniboxDemoteByType;

// On-Focus Suggestions a.k.a. ZeroSuggest.
extern const base::Feature kOnFocusSuggestions;
extern const base::Feature kZeroSuggestionsOnNTP;
extern const base::Feature kZeroSuggestionsOnNTPRealbox;
extern const base::Feature kZeroSuggestionsOnSERP;

// Scoring - these affect how relevance scores are calculated for suggestions.
extern const base::Feature kOmniboxExperimentalSuggestScoring;

// Suggestions UI - these affect the UI or function of the suggestions popup.
extern const base::Feature kConfirmOmniboxSuggestionRemovals;

// Flags related to new rows and managing rows in the Omnibox.
// TODO(krb): Move more flags here.
extern const base::Feature kOmniboxLooseMaxLimitOnDedicatedRows;

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_
