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
BASE_DECLARE_FEATURE(kImageSearchSuggestionThumbnail);
BASE_DECLARE_FEATURE(kOmniboxRemoveSuggestionsFromClipboard);

// Features that affect the "twiddle" step of AutocompleteController, e.g.,
// deduping or `SortAndCull()`.
BASE_DECLARE_FEATURE(kGroupingFrameworkForNonZPS);

// Features below this line should be sorted alphabetically by their comments.

// Num suggestions - these affect how many suggestions are shown based on e.g.
// focus, page context, provider, or URL v non-URL.
BASE_DECLARE_FEATURE(kMaxZeroSuggestMatches);
BASE_DECLARE_FEATURE(kUIExperimentMaxAutocompleteMatches);
BASE_DECLARE_FEATURE(kDynamicMaxAutocomplete);

// Local history zero-prefix (aka zero-suggest) and prefix suggestions.
BASE_DECLARE_FEATURE(kFocusTriggersWebAndSRPZeroSuggest);
BASE_DECLARE_FEATURE(kHideSuggestionGroupHeaders);
BASE_DECLARE_FEATURE(kLocalHistoryZeroSuggestBeyondNTP);
BASE_DECLARE_FEATURE(kZeroSuggestInMemoryCaching);
BASE_DECLARE_FEATURE(kZeroSuggestPrefetchDebouncing);
BASE_DECLARE_FEATURE(kZeroSuggestPrefetching);
BASE_DECLARE_FEATURE(kZeroSuggestPrefetchingOnSRP);
BASE_DECLARE_FEATURE(kZeroSuggestPrefetchingOnWeb);
// Related, kMaxZeroSuggestMatches.

// On Device Suggest.
BASE_DECLARE_FEATURE(kOnDeviceHeadProviderIncognito);
BASE_DECLARE_FEATURE(kOnDeviceHeadProviderNonIncognito);
BASE_DECLARE_FEATURE(kOnDeviceTailModel);
BASE_DECLARE_FEATURE(kOnDeviceTailEnableEnglishModel);

// Document provider and domain suggestions
BASE_DECLARE_FEATURE(kDocumentProvider);
BASE_DECLARE_FEATURE(kDocumentProviderPrimaryAccountRequirement);
BASE_DECLARE_FEATURE(kDocumentProviderEnterpriseEligibility);
BASE_DECLARE_FEATURE(kDocumentProviderEnterpriseEligibilityWhenUnknown);
BASE_DECLARE_FEATURE(kDocumentProviderNoSyncRequirement);

// Suggestions UI - these affect the UI or function of the suggestions popup.
BASE_DECLARE_FEATURE(kShowPopupOnMouseReleased);
BASE_DECLARE_FEATURE(kMostVisitedTilesHorizontalRenderGroup);
BASE_DECLARE_FEATURE(kRichAutocompletion);

// Omnibox UI - these affect the UI or function of the location bar (not the
// popup).
BASE_DECLARE_FEATURE(kAiModeOmniboxEntryPoint);
BASE_DECLARE_FEATURE(kAiModeOmniboxEntryPointEnUs);
BASE_DECLARE_FEATURE(kHideAimEntrypointOnUserInput);
BASE_DECLARE_FEATURE(kOmniboxMultimodalInput);
BASE_DECLARE_FEATURE(kRemoveSearchReadyOmnibox);

// Navigation experiments.
BASE_DECLARE_FEATURE(kDefaultTypedNavigationsToHttps);
extern const char kDefaultTypedNavigationsToHttpsTimeoutParam[];

BASE_DECLARE_FEATURE(kOverrideAndroidOmniboxSpareRendererDelay);
// The delay value in milliseconds.
inline constexpr base::FeatureParam<int> kOmniboxSpareRendererDelayMs{
    &kOverrideAndroidOmniboxSpareRendererDelay,
    "omnibox_spare_renderer_delay_ms", 1000};

// Omnibox ML scoring.
BASE_DECLARE_FEATURE(kLogUrlScoringSignals);
BASE_DECLARE_FEATURE(kEnableHistoryScoringSignalsAnnotatorForSearches);
BASE_DECLARE_FEATURE(kMlUrlPiecewiseMappedSearchBlending);
BASE_DECLARE_FEATURE(kMlUrlScoreCaching);
BASE_DECLARE_FEATURE(kMlUrlScoring);
BASE_DECLARE_FEATURE(kMlUrlSearchBlending);
BASE_DECLARE_FEATURE(kUrlScoringModel);

// Animate appearance of suggestions list.
BASE_DECLARE_FEATURE(kAnimateSuggestionsListAppearance);

// Allows for touch down events to send a signal to |SearchPrefetchService| to
// start prefetching the suggestion. The feature only applies to search
// suggestions and only controls whether the signal is sent.
BASE_DECLARE_FEATURE(kOmniboxTouchDownTriggerForPrefetch);

// Site search/Keyword mode related features.
BASE_DECLARE_FEATURE(kOmniboxSiteSearch);
BASE_DECLARE_FEATURE(kStarterPackExpansion);
BASE_DECLARE_FEATURE(kStarterPackIPH);
BASE_DECLARE_FEATURE(kAiModeStartPack);

// Search and Suggest requests and params.
BASE_DECLARE_FEATURE(kAblateSearchProviderWarmup);
BASE_DECLARE_FEATURE(kReportApplicationLanguageInSearchRequest);

BASE_DECLARE_FEATURE(kOmniboxAsyncViewInflation);
BASE_DECLARE_FEATURE(kUseFusedLocationProvider);

BASE_DECLARE_FEATURE(kOmniboxMobileParityUpdate);
BASE_DECLARE_FEATURE(kOmniboxMobileParityUpdateV2);
#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kOmniboxMobileParityUpdateV3);
#endif  // BUILDFLAG(IS_IOS)

// Omnibox suggestions tuning
BASE_DECLARE_FEATURE(kNumNtpZpsRecentSearches);
BASE_DECLARE_FEATURE(kNumNtpZpsTrendingSearches);
BASE_DECLARE_FEATURE(kNumWebZpsRecentSearches);
BASE_DECLARE_FEATURE(kNumWebZpsRelatedSearches);
BASE_DECLARE_FEATURE(kNumWebZpsMostVisitedUrls);
BASE_DECLARE_FEATURE(kNumSrpZpsRecentSearches);
BASE_DECLARE_FEATURE(kNumSrpZpsRelatedSearches);


// `ShortcutsProvider` features.
BASE_DECLARE_FEATURE(kOmniboxShortcutsAndroid);

// Enterprise search aggregators features.
BASE_DECLARE_FEATURE(kEnableSearchAggregatorPolicy);
BASE_DECLARE_FEATURE(kUseAgentspace25Logo);

// Site search allow user override feature.
BASE_DECLARE_FEATURE(kEnableSiteSearchAllowUserOverridePolicy);

// Preconnect/prerender behavior for suggestions
BASE_DECLARE_FEATURE(kPreconnectNonSearchOmniboxSuggestions);

// When enabled, unblocks omnibox height on small form factor devices, allowing
// users to type in multiline / longer text.
BASE_DECLARE_FEATURE(kMultilineEditField);

// Whether the composebox should use the new `chrome-compose` client.
BASE_DECLARE_FEATURE(kComposeboxUsesChromeComposeClient);

// Controls whether or not contextual composebox should display suggestions.
BASE_DECLARE_FEATURE(kComposeboxAttachmentsTypedState);

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kDiagnostics);
BASE_DECLARE_FEATURE(kJumpStartOmnibox);
BASE_DECLARE_FEATURE(kSuppressIntermediateACUpdatesOnLowEndDevices);
// Delay focusTab to prioritize navigation (https://crbug.com/374852568).
BASE_DECLARE_FEATURE(kPostDelayedTaskFocusTab);
BASE_DECLARE_FEATURE(kAndroidHubSearchTabGroups);
BASE_DECLARE_FEATURE(kOmniboxImprovementForLFF);
#endif  // BUILDFLAG(IS_ANDROID)
// Note: no new flags beyond this point.
}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURES_H_
