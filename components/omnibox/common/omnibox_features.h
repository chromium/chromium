// Copyright 2019 The Chromium Authors
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
BASE_DECLARE_FEATURE(kExperimentalKeywordMode);
BASE_DECLARE_FEATURE(kImageSearchSuggestionThumbnail);
BASE_DECLARE_FEATURE(kOmniboxRemoveSuggestionsFromClipboard);
BASE_DECLARE_FEATURE(kAndroidAuxiliarySearch);

// Flags that affect the "twiddle" step of AutocompleteResult, e.g.,
// `SortAndCull()`.
BASE_DECLARE_FEATURE(kAutocompleteStability);
BASE_DECLARE_FEATURE(kDocumentProviderDedupingOptimization);
BASE_DECLARE_FEATURE(kOmniboxDemoteByType);
BASE_DECLARE_FEATURE(kOmniboxRemoveExcessiveRecycledViewClearCalls);
BASE_DECLARE_FEATURE(kPreserveDefault);
BASE_DECLARE_FEATURE(kStrippedGurlOptimization);
BASE_DECLARE_FEATURE(kUpdateResultDebounce);
BASE_DECLARE_FEATURE(kSingleSortAndCullPass);

// Features below this line should be sorted alphabetically by their comments.

// Num suggestions - these affect how many suggestions are shown based on e.g.
// focus, page context, provider, or URL v non-URL.
BASE_DECLARE_FEATURE(kMaxZeroSuggestMatches);
BASE_DECLARE_FEATURE(kUIExperimentMaxAutocompleteMatches);
// The default value is established here as a bool so it can be referred to in
// OmniboxFieldTrial.
extern const bool kOmniboxMaxURLMatchesEnabledByDefault;
BASE_DECLARE_FEATURE(kOmniboxMaxURLMatches);
BASE_DECLARE_FEATURE(kDynamicMaxAutocomplete);

// Entity suggestion disambiguation.
BASE_DECLARE_FEATURE(kDisambiguateEntitySuggestions);
BASE_DECLARE_FEATURE(kDisambiguateTabMatchingForEntitySuggestions);

// Local history zero-prefix (aka zero-suggest) and prefix suggestions.
BASE_DECLARE_FEATURE(kAdjustLocalHistoryZeroSuggestRelevanceScore);
BASE_DECLARE_FEATURE(kClobberTriggersContextualWebZeroSuggest);
BASE_DECLARE_FEATURE(kClobberTriggersSRPZeroSuggest);
BASE_DECLARE_FEATURE(kFocusTriggersContextualWebZeroSuggest);
BASE_DECLARE_FEATURE(kFocusTriggersSRPZeroSuggest);
BASE_DECLARE_FEATURE(kKeepSecondaryZeroSuggest);
BASE_DECLARE_FEATURE(kLocalHistorySuggestRevamp);
BASE_DECLARE_FEATURE(kLocalHistoryZeroSuggestBeyondNTP);
BASE_DECLARE_FEATURE(kOmniboxLocalZeroSuggestAgeThreshold);
BASE_DECLARE_FEATURE(kOmniboxOnClobberFocusTypeOnContent);
BASE_DECLARE_FEATURE(kZeroSuggestInMemoryCaching);
BASE_DECLARE_FEATURE(kZeroSuggestOnNTPForSignedOutUsers);
BASE_DECLARE_FEATURE(kZeroSuggestPrefetching);
BASE_DECLARE_FEATURE(kZeroSuggestPrefetchingOnSRP);
BASE_DECLARE_FEATURE(kZeroSuggestPrefetchingOnWeb);
// Related, kMaxZeroSuggestMatches.

// On Device Suggest.
BASE_DECLARE_FEATURE(kOnDeviceHeadProviderIncognito);
BASE_DECLARE_FEATURE(kOnDeviceHeadProviderNonIncognito);
BASE_DECLARE_FEATURE(kOnDeviceTailModel);

// Provider-specific - These features change the behavior of specific providers.
BASE_DECLARE_FEATURE(kOmniboxExperimentalSuggestScoring);
BASE_DECLARE_FEATURE(kDisableCGIParamMatching);
BASE_DECLARE_FEATURE(kShortBookmarkSuggestions);
BASE_DECLARE_FEATURE(kShortBookmarkSuggestionsByTotalInputLength);
BASE_DECLARE_FEATURE(kBookmarkPaths);
BASE_DECLARE_FEATURE(kShortcutExpanding);
BASE_DECLARE_FEATURE(kShortcutBoost);
// TODO(crbug.com/1202964): Clean up feature flag used in staged roll-out of
// various CLs related to the contents/description clean-up work.
BASE_DECLARE_FEATURE(kStoreTitleInContentsAndUrlInDescription);
BASE_DECLARE_FEATURE(kHistoryQuickProviderSpecificityScoreCountUniqueHosts);

// Document provider and domain suggestions
BASE_DECLARE_FEATURE(kDocumentProvider);
BASE_DECLARE_FEATURE(kDocumentProviderAso);
BASE_DECLARE_FEATURE(kDomainSuggestions);

// Suggestions UI - these affect the UI or function of the suggestions popup.
BASE_DECLARE_FEATURE(kAdaptiveSuggestionsCount);
BASE_DECLARE_FEATURE(kClipboardSuggestionContentHidden);
BASE_DECLARE_FEATURE(kSuggestionAnswersColorReverse);
BASE_DECLARE_FEATURE(kMostVisitedTiles);
BASE_DECLARE_FEATURE(kMostVisitedTilesTitleWrapAround);
BASE_DECLARE_FEATURE(kRichAutocompletion);
BASE_DECLARE_FEATURE(kNtpRealboxPedals);
BASE_DECLARE_FEATURE(kOmniboxFuzzyUrlSuggestions);
BASE_DECLARE_FEATURE(kOmniboxDefaultBrowserPedal);
BASE_DECLARE_FEATURE(kOmniboxMatchToolbarAndStatusBarColor);
BASE_DECLARE_FEATURE(kOmniboxMostVisitedTilesAddRecycledViewPool);
BASE_DECLARE_FEATURE(kUniformRowHeight);
BASE_DECLARE_FEATURE(kWebUIOmniboxPopup);

// Omnibox UI - these affect the UI or function of the location bar (not the
// popup).
BASE_DECLARE_FEATURE(kOmniboxAssistantVoiceSearch);

// Omnibox & Suggestions UI - these affect both the omnibox and the suggestions
// popup.
BASE_DECLARE_FEATURE(kClosePopupWithEscape);

// Settings Page - these affect the appearance of the Search Engines settings
// page
BASE_DECLARE_FEATURE(kSiteSearchStarterPack);

// Experiment to introduce new security indicators for HTTPS.
BASE_DECLARE_FEATURE(kUpdatedConnectionSecurityIndicators);

// Navigation experiments.
BASE_DECLARE_FEATURE(kDefaultTypedNavigationsToHttps);
extern const char kDefaultTypedNavigationsToHttpsTimeoutParam[];

// Omnibox Logging.
BASE_DECLARE_FEATURE(kReportAssistedQueryStats);
BASE_DECLARE_FEATURE(kReportSearchboxStats);

// Omnibox ML scoring.
BASE_DECLARE_FEATURE(kLogUrlScoringSignals);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_
