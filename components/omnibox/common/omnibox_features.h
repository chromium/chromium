// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_
#define COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_

#include "base/feature_list.h"

namespace omnibox {

// Please do not add more features to this "big blob" list.
// Instead, use the categorized and alphabetized lists below this "big blob".
// You can create a new category if none of the existing ones fit.
BASE_DECLARE_FEATURE(kExperimentalKeywordMode);
BASE_DECLARE_FEATURE(kImageSearchSuggestionThumbnail);
BASE_DECLARE_FEATURE(kOmniboxRemoveSuggestionsFromClipboard);

// Features that affect the "twiddle" step of AutocompleteController, e.g.,
// deduping or `SortAndCull()`.
BASE_DECLARE_FEATURE(kGroupingFrameworkForZPS);
BASE_DECLARE_FEATURE(kGroupingFrameworkForNonZPS);
BASE_DECLARE_FEATURE(kOmniboxDemoteByType);
BASE_DECLARE_FEATURE(kPreferNonShortcutMatchesWhenDeduping);
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
BASE_DECLARE_FEATURE(kDisambiguateTabMatchingForEntitySuggestions);

// Local history zero-prefix (aka zero-suggest) and prefix suggestions.
BASE_DECLARE_FEATURE(kAdjustLocalHistoryZeroSuggestRelevanceScore);
BASE_DECLARE_FEATURE(kClobberTriggersContextualWebZeroSuggest);
BASE_DECLARE_FEATURE(kClobberTriggersSRPZeroSuggest);
BASE_DECLARE_FEATURE(kFocusTriggersContextualWebZeroSuggest);
BASE_DECLARE_FEATURE(kFocusTriggersSRPZeroSuggest);
BASE_DECLARE_FEATURE(kLocalHistoryZeroSuggestBeyondNTP);
BASE_DECLARE_FEATURE(kNormalizeSearchSuggestions);
BASE_DECLARE_FEATURE(kOmniboxOnClobberFocusTypeOnContent);
BASE_DECLARE_FEATURE(kRealboxSecondaryZeroSuggest);
BASE_DECLARE_FEATURE(kZeroSuggestInMemoryCaching);
BASE_DECLARE_FEATURE(kZeroSuggestOnNTPForSignedOutUsers);
BASE_DECLARE_FEATURE(kZeroSuggestPrefetching);
BASE_DECLARE_FEATURE(kZeroSuggestPrefetchingOnSRP);
BASE_DECLARE_FEATURE(kZeroSuggestPrefetchingOnWeb);
// Related, kMaxZeroSuggestMatches.

// On Device Suggest.
BASE_DECLARE_FEATURE(kOnDeviceHeadProviderIncognito);
BASE_DECLARE_FEATURE(kOnDeviceHeadProviderNonIncognito);
BASE_DECLARE_FEATURE(kOnDeviceHeadProviderKorean);
BASE_DECLARE_FEATURE(kOnDeviceTailModel);

// Provider-specific - These features change the behavior of specific providers.
// TODO(crbug.com/1202964): Clean up feature flag used in staged roll-out of
// various CLs related to the contents/description clean-up work.
BASE_DECLARE_FEATURE(kStoreTitleInContentsAndUrlInDescription);

// Document provider and domain suggestions
BASE_DECLARE_FEATURE(kDocumentProvider);
BASE_DECLARE_FEATURE(kDocumentProviderNoSetting);
BASE_DECLARE_FEATURE(kDocumentProviderNoSyncRequirement);
BASE_DECLARE_FEATURE(kDomainSuggestions);

// Consent helper types
BASE_DECLARE_FEATURE(kPrefBasedDataCollectionConsentHelper);

// Suggestions UI - these affect the UI or function of the suggestions popup.
BASE_DECLARE_FEATURE(kAdaptiveSuggestionsCount);
BASE_DECLARE_FEATURE(kClipboardSuggestionContentHidden);
BASE_DECLARE_FEATURE(kSuppressClipboardSuggestionAfterFirstUsed);
BASE_DECLARE_FEATURE(kCompanyEntityIconAdjustment);
BASE_DECLARE_FEATURE(kCr2023ActionChips);
BASE_DECLARE_FEATURE(kCr2023ActionChipsIcons);
BASE_DECLARE_FEATURE(kSuggestionAnswersColorReverse);
BASE_DECLARE_FEATURE(kMostVisitedTiles);
BASE_DECLARE_FEATURE(kMostVisitedTilesHorizontalRenderGroup);
BASE_DECLARE_FEATURE(kRichAutocompletion);
BASE_DECLARE_FEATURE(kNtpRealboxPedals);
BASE_DECLARE_FEATURE(kOmniboxActionsUISimplification);
BASE_DECLARE_FEATURE(kOmniboxFuzzyUrlSuggestions);
BASE_DECLARE_FEATURE(kOmniboxMatchToolbarAndStatusBarColor);
BASE_DECLARE_FEATURE(kOmniboxMostVisitedTilesOnSrp);
BASE_DECLARE_FEATURE(kSearchReadyOmniboxAllowQueryEdit);
BASE_DECLARE_FEATURE(kSquareSuggestIcons);
BASE_DECLARE_FEATURE(kUniformRowHeight);
BASE_DECLARE_FEATURE(kWebUIOmniboxPopup);
BASE_DECLARE_FEATURE(kExpandedStateHeight);
BASE_DECLARE_FEATURE(kExpandedStateShape);
BASE_DECLARE_FEATURE(kExpandedStateColors);
BASE_DECLARE_FEATURE(kExpandedStateSuggestIcons);
BASE_DECLARE_FEATURE(kExpandedLayout);
BASE_DECLARE_FEATURE(kSuggestionHoverFillShape);

// Omnibox UI - these affect the UI or function of the location bar (not the
// popup).
BASE_DECLARE_FEATURE(kOmniboxAssistantVoiceSearch);

BASE_DECLARE_FEATURE(kOmniboxCR23SteadyStateIcons);
BASE_DECLARE_FEATURE(kOmniboxSteadyStateBackgroundColor);

BASE_DECLARE_FEATURE(kOmniboxSteadyStateHeight);
BASE_DECLARE_FEATURE(kOmniboxSteadyStateTextStyle);

BASE_DECLARE_FEATURE(kOmniboxSteadyStateTextColor);

// Omnibox & Suggestions UI - these affect both the omnibox and the suggestions
// popup.
BASE_DECLARE_FEATURE(kOmniboxModernizeVisualUpdate);

// Android only flag that controls whether the new security indicator should be
// used, on non-Android platforms this is controlled through the
// ChromeRefresh2023 flag.
BASE_DECLARE_FEATURE(kUpdatedConnectionSecurityIndicators);

// Navigation experiments.
BASE_DECLARE_FEATURE(kDefaultTypedNavigationsToHttps);
extern const char kDefaultTypedNavigationsToHttpsTimeoutParam[];

// Omnibox Logging.
BASE_DECLARE_FEATURE(kReportAssistedQueryStats);
BASE_DECLARE_FEATURE(kReportSearchboxStats);

// Omnibox ML scoring.
BASE_DECLARE_FEATURE(kLogUrlScoringSignals);
BASE_DECLARE_FEATURE(kMlUrlScoring);
BASE_DECLARE_FEATURE(kUrlScoringModel);

// Inspire Me - additional suggestions based on user's location and interests.
BASE_DECLARE_FEATURE(kInspireMe);

// Actions in Suggest - Action Chips for Entity Suggestions.
// Data driven feature; flag helps tune behavior.
BASE_DECLARE_FEATURE(kActionsInSuggest);

// Adds support for categorical suggestion type.
BASE_DECLARE_FEATURE(kCategoricalSuggestions);
BASE_DECLARE_FEATURE(kMergeSubtypes);

// Returns true if the Omnibox CR23 `feature`, which depends on customize
// chrome, should be enabled.
bool IsOmniboxCr23CustomizeGuardedFeatureEnabled(const base::Feature& feature);

// Allows for touch down events to send a signal to |SearchPrefetchService| to
// start prefetching the suggestion. The feature only applies to search
// suggestions and only controls whether the signal is sent.
BASE_DECLARE_FEATURE(kOmniboxTouchDownTriggerForPrefetch);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_
