// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/omnibox_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"

namespace omnibox {

constexpr auto enabled_by_default_desktop_only =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

constexpr auto enabled_by_default_android_only =
#if BUILDFLAG(IS_ANDROID)
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

constexpr auto enabled_by_default_desktop_android =
#if BUILDFLAG(IS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

const auto enabled_by_default_android_ios =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

// Feature to enable showing thumbnail in front of the Omnibox clipboard image
// search suggestion.
BASE_FEATURE(kImageSearchSuggestionThumbnail,
             "ImageSearchSuggestionThumbnail",
             enabled_by_default_android_only);

// Feature used to allow users to remove suggestions from clipboard.
BASE_FEATURE(kOmniboxRemoveSuggestionsFromClipboard,
             "OmniboxRemoveSuggestionsFromClipboard",
             enabled_by_default_android_only);

// When enabled, uses the grouping framework with zero prefix suggestions (i.e.
// autocomplete_grouper_sections.h) to limit and group (but not sort) matches.
BASE_FEATURE(kGroupingFrameworkForZPS,
             "OmniboxGroupingFrameworkForZPS",
             enabled_by_default_desktop_android);

// When enabled, uses the grouping framework with prefixed suggestions (i.e.
// autocomplete_grouper_sections.h) to limit and group (but not sort) matches.
BASE_FEATURE(kGroupingFrameworkForNonZPS,
             "OmniboxGroupingFrameworkForNonZPS",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Demotes the relevance scores when comparing suggestions based on the
// suggestion's |AutocompleteMatchType| and the user's |PageClassification|.
// This feature's main job is to contain the DemoteByType parameter.
BASE_FEATURE(kOmniboxDemoteByType,
             "OmniboxDemoteByType",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, deduping prefers non-shortcut provider matches, while still
// treating fuzzy provider matches as the least preferred.
BASE_FEATURE(kPreferNonShortcutMatchesWhenDeduping,
             "OmniboxPreferNonShortcutMatchesWhenDeduping",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Determines which are culled when both tail and history cluster suggestions
// are available. See `MaybeCullTailSuggestions()`.
// TODO(manukh): Enabled by default 6/1/23 m116. Clean up feature code 8/15 when
//   m116 reaches stable.
BASE_FEATURE(kPreferTailOverHistoryClusterSuggestions,
             "OmniboxPreferTailOverHistoryClusterSuggestions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When disabled, when providers update their matches, the new set of matches
// are sorted and culled, then merged with the old matches, then sorted and
// culled again. When enabled, the first sort and cull is skipped.
BASE_FEATURE(kSingleSortAndCullPass,
             "OmniboxSingleSortAndCullPass",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to debounce `AutocompleteController::NotifyChanged()`.
// TODO(manukh): Enabled by default 7/11/23 m117. Clean up feature code 9/12
//   when m117 reaches stable.
BASE_FEATURE(kUpdateResultDebounce,
             "OmniboxUpdateResultDebounce",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature used to cap max zero suggestions shown according to the param
// OmniboxMaxZeroSuggestMatches. If omitted,
// OmniboxUIExperimentMaxAutocompleteMatches will be used instead. If present,
// OmniboxMaxZeroSuggestMatches will override
// OmniboxUIExperimentMaxAutocompleteMatches when |from_omnibox_focus| is true.
BASE_FEATURE(kMaxZeroSuggestMatches,
             "OmniboxMaxZeroSuggestMatches",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature used to cap max suggestions shown according to the params
// UIMaxAutocompleteMatches and UIMaxAutocompleteMatchesByProvider.
BASE_FEATURE(kUIExperimentMaxAutocompleteMatches,
             "OmniboxUIExperimentMaxAutocompleteMatches",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature used to cap the number of URL-type matches shown within the
// Omnibox. If enabled, the number of URL-type matches is limited (unless
// there are no more non-URL matches available.) If enabled, there is a
// companion parameter - OmniboxMaxURLMatches - which specifies the maximum
// desired number of URL-type matches.
BASE_FEATURE(kOmniboxMaxURLMatches,
             "OmniboxMaxURLMatches",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature used to cap max suggestions to a dynamic limit based on how many URLs
// would be shown. E.g., show up to 10 suggestions if doing so would display no
// URLs; else show up to 8 suggestions if doing so would include 1 or more URLs.
BASE_FEATURE(kDynamicMaxAutocomplete,
             "OmniboxDynamicMaxAutocomplete",
             enabled_by_default_desktop_android);

// If enabled, takes the search intent query params into account for triggering
// switch to tab actions on matches.
BASE_FEATURE(kDisambiguateTabMatchingForEntitySuggestions,
             "DisambiguateTabMatchingForEntitySuggestions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Used to adjust the relevance for the local history zero-prefix suggestions.
// If enabled, the relevance is determined by this feature's companion
// parameter, OmniboxFieldTrial::kLocalHistoryZeroSuggestRelevanceScore.
BASE_FEATURE(kAdjustLocalHistoryZeroSuggestRelevanceScore,
             "AdjustLocalHistoryZeroSuggestRelevanceScore",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables on-clobber (i.e., when the user clears the whole omnibox text)
// zero-prefix suggestions on the Open Web, that are contextual to the current
// URL. Will only work if user is signed-in and syncing, or is otherwise
// eligible to send the current page URL to the suggest server.
BASE_FEATURE(kClobberTriggersContextualWebZeroSuggest,
             "OmniboxClobberTriggersContextualWebZeroSuggest",
             enabled_by_default_desktop_android);

// Enables on-clobber (i.e., when the user clears the whole omnibox text)
// zero-prefix suggestions on the SRP.
BASE_FEATURE(kClobberTriggersSRPZeroSuggest,
             "OmniboxClobberTriggersSRPZeroSuggest",
             enabled_by_default_desktop_android);

// Enables on-focus zero-prefix suggestions on the Open Web, that are contextual
// to the current URL. Will only work if user is signed-in and syncing, or is
// otherwise eligible to send the current page URL to the suggest server.
BASE_FEATURE(kFocusTriggersContextualWebZeroSuggest,
             "OmniboxFocusTriggersContextualWebZeroSuggest",
             enabled_by_default_android_ios);

// Enables on-focus zero-prefix suggestions on the SRP.
BASE_FEATURE(kFocusTriggersSRPZeroSuggest,
             "OmniboxFocusTriggersSRPZeroSuggest",
             enabled_by_default_android_ios);

// Enables local history zero-prefix suggestions in every context in which the
// remote zero-prefix suggestions are enabled.
BASE_FEATURE(kLocalHistoryZeroSuggestBeyondNTP,
             "LocalHistoryZeroSuggestBeyondNTP",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, SearchProvider uses `normalized_term` instead of `term` from the
// `keyword_search_terms` table. `normalized_term` is the original search term
// in lower case with extra whitespace characters collapsed. To ensure
// suggestions from SearchProvider continue to get deduped with those from
// ShortcutsProvider, AutocompleteMatch::GURLToStrippedGURL uses the normalized
// term to build the destination URLs so they are identical despite case
// mismatches in the terms.
BASE_FEATURE(kNormalizeSearchSuggestions,
             "NormalizeSearchSuggestions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Mainly used to enable sending INTERACTION_CLOBBER focus type for zero-prefix
// requests with an empty input on Web/SRP on Mobile. Enabled by default on
// Desktop because it is also used by Desktop in the cross-platform code in the
// OmniboxEditModel for triggering zero-suggest prefetching on Web/SRP.
BASE_FEATURE(kOmniboxOnClobberFocusTypeOnContent,
             "OmniboxOnClobberFocusTypeOnContent",
             enabled_by_default_desktop_android);

// If enabled, allows up to
// `OmniboxFieldTrial::kRealboxMaxPreviousSearchRelatedSuggestions` zero-prefix
// suggestions in the 2nd column of realbox.
BASE_FEATURE(kRealboxSecondaryZeroSuggest,
             "RealboxSecondaryZeroSuggest",
             enabled_by_default_desktop_only);

// If enabled, zero prefix suggestions will be stored using an in-memory caching
// service, instead of using the existing prefs-based cache.
BASE_FEATURE(kZeroSuggestInMemoryCaching,
             "ZeroSuggestInMemoryCaching",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables on-focus zero-prefix suggestions on the NTP for signed-out users.
BASE_FEATURE(kZeroSuggestOnNTPForSignedOutUsers,
             "OmniboxTrendingZeroPrefixSuggestionsOnNTP",
             enabled_by_default_desktop_android);

// Enables prefetching of the zero prefix suggestions for eligible users on NTP.
BASE_FEATURE(kZeroSuggestPrefetching,
             "ZeroSuggestPrefetching",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables prefetching of the zero prefix suggestions for eligible users on SRP.
BASE_FEATURE(kZeroSuggestPrefetchingOnSRP,
             "ZeroSuggestPrefetchingOnSRP",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables prefetching of the zero prefix suggestions for eligible users on the
// Web (i.e. non-NTP and non-SRP URLs).
BASE_FEATURE(kZeroSuggestPrefetchingOnWeb,
             "ZeroSuggestPrefetchingOnWeb",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Features to provide head and tail non personalized search suggestion from
// compact on device models. More specifically, feature name with suffix
// Incognito / NonIncognito  will only controls behaviors under incognito /
// non-incognito mode respectively.
BASE_FEATURE(kOnDeviceHeadProviderIncognito,
             "OmniboxOnDeviceHeadProviderIncognito",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kOnDeviceHeadProviderNonIncognito,
             "OmniboxOnDeviceHeadProviderNonIncognito",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kOnDeviceHeadProviderKorean,
             "OmniboxOnDeviceHeadProviderKorean",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kOnDeviceTailModel,
             "OmniboxOnDeviceTailModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the relevant AutocompleteProviders will store "title" data in
// AutocompleteMatch::contents and "URL" data in AutocompleteMatch::description
// for URL-based omnibox suggestions (see crbug.com/1202964 for more details).
BASE_FEATURE(kStoreTitleInContentsAndUrlInDescription,
             "OmniboxStoreTitleInContentsAndUrlInDescription",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature used to fetch document suggestions.
BASE_FEATURE(kDocumentProvider,
             "OmniboxDocumentProvider",
             enabled_by_default_desktop_only);

// If enabled, the 'Show Google Drive Suggestions' setting is removed and Drive
// suggestions are available to all clients who meet the other requirements.
BASE_FEATURE(kDocumentProviderNoSetting,
             "OmniboxDocumentProviderNoSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the requirement to be in an active Sync state is removed and
// Drive suggestions are available to all clients who meet the other
// requirements.
BASE_FEATURE(kDocumentProviderNoSyncRequirement,
             "OmniboxDocumentProviderNoSyncRequirement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to determine if the HQP should double as a domain provider by
// suggesting up to the provider limit for each of the user's highly visited
// domains.
BASE_FEATURE(kDomainSuggestions,
             "OmniboxDomainSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to determine if omnibox should use a pref based data collection
// consent helper instead of a history sync based one.
BASE_FEATURE(kPrefBasedDataCollectionConsentHelper,
             "PrefBasedDataCollectionConsentHelper",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows Omnibox to dynamically adjust number of offered suggestions to fill in
// the space between Omnibox and the soft keyboard. The number of suggestions
// shown will be no less than minimum for the platform (eg. 5 for Android).
BASE_FEATURE(kAdaptiveSuggestionsCount,
             "OmniboxAdaptiveSuggestionsCount",
             enabled_by_default_android_ios);

// If enabled, clipboard suggestion will not show the clipboard content until
// the user clicks the reveal button.
BASE_FEATURE(kClipboardSuggestionContentHidden,
             "ClipboardSuggestionContentHidden",
             enabled_by_default_android_only);

// If enabled, clipboard suggestion for distinct clip data will not show after
// first use.
BASE_FEATURE(kSuppressClipboardSuggestionAfterFirstUsed,
             "SuppressClipboardSuggestionAfterFirstUsed",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, uses the Chrome Refresh 2023 design's shape for action chips in
// the omnibox suggestion popup.
BASE_FEATURE(kCr2023ActionChips,
             "Cr2023ActionChips",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, uses the Chrome Refresh 2023 design's icons for action chips in
// the omnibox suggestion popup.
BASE_FEATURE(kCr2023ActionChipsIcons,
             "Cr2023ActionChipsIcons",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, finance ticker answer from omnibox will reverse the color for
// stock ticker. only colors being swapped are those that represent "growth" and
// "loss" to represent colors red and green in a way that is appropriate for a
// given country/culture
BASE_FEATURE(kSuggestionAnswersColorReverse,
             "SuggestionAnswersColorReverse",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, frequently visited sites are presented in form of a single row
// with a carousel of tiles, instead of one URL per row.
BASE_FEATURE(kMostVisitedTiles,
             "OmniboxMostVisitedTiles",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, makes Most Visited Tiles a Horizontal render group.
// Horizontal render group decomposes aggregate suggestions (such as old Most
// Visited Tiles), expecting individual AutocompleteMatch entry for every
// element in the carousel.
BASE_FEATURE(kMostVisitedTilesHorizontalRenderGroup,
             "OmniboxMostVisitedTilesHorizontalRenderGroup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, expands autocompletion to possibly (depending on params) include
// suggestion titles and non-prefixes as opposed to be restricted to URL
// prefixes. Will also adjust the location bar UI and omnibox text selection to
// accommodate the autocompletions.
BASE_FEATURE(kRichAutocompletion,
             "OmniboxRichAutocompletion",
             enabled_by_default_desktop_only);

// Feature used to enable Pedals in the NTP Realbox.
BASE_FEATURE(kNtpRealboxPedals,
             "NtpRealboxPedals",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature used to enable the simplified actions UI design.
BASE_FEATURE(kOmniboxActionsUISimplification,
             "OmniboxActionsUISimplification",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature used to enable URL suggestions for inputs that may contain typos.
BASE_FEATURE(kOmniboxFuzzyUrlSuggestions,
             "OmniboxFuzzyUrlSuggestions",
             enabled_by_default_desktop_only);

// Feature used to synchronize the toolbar's and status bar's color.
BASE_FEATURE(kOmniboxMatchToolbarAndStatusBarColor,
             "OmniboxMatchToolbarAndStatusBarColor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature used to add most visited tiles to the suggestions when the user is on
// a search result page that does not do search term replacement.
BASE_FEATURE(kOmniboxMostVisitedTilesOnSrp,
             "OmniboxMostVisitedTilesOnSrp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, allows Search Ready Omnibox to populate original search query
// when the user presses the <edit> button on EditUrl suggestion.
BASE_FEATURE(kSearchReadyOmniboxAllowQueryEdit,
             "SearchReadyOmniboxAllowQueryEdit",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, adds a grey square background to search icons, and makes answer
// icon square instead of round.
// TODO(manukh): Clean up feature code 9/12 when m117 reaches stable; we're
//   launching the rest of CR23 in m117.
BASE_FEATURE(kSquareSuggestIcons,
             "OmniboxSquareIcons",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, forces omnibox suggestion rows to be uniformly sized.
// TODO(manukh): Clean up feature code 9/12 when m117 reaches stable; we're
//   launching the rest of CR23 in m117.
BASE_FEATURE(kUniformRowHeight,
             "OmniboxUniformRowHeight",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, shows the omnibox suggestions popup in WebUI.
BASE_FEATURE(kWebUIOmniboxPopup,
             "WebUIOmniboxPopup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Omnibox "expanded state" height is increased from 42 px to 44 px.
BASE_FEATURE(kExpandedStateHeight,
             "OmniboxExpandedStateHeight",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Omnibox "expanded state" corner radius is increased from 8px to
// 16px.
BASE_FEATURE(kExpandedStateShape,
             "OmniboxExpandedStateShape",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Omnibox "expanded state" colors are updated to match CR23
// guidelines.
BASE_FEATURE(kExpandedStateColors,
             "OmniboxExpandedStateColors",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Omnibox "expanded state" icons are updated to match CR23
// guidelines.
BASE_FEATURE(kExpandedStateSuggestIcons,
             "OmniboxExpandedStateSuggestIcons",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Omnibox "expanded state" layout is updated to match CR23
// guidelines.
BASE_FEATURE(kExpandedLayout,
             "OmniboxExpandedLayout",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the shape of the "hover fill" that's rendered for Omnibox
// suggestions is updated to match CR23 guidelines.
BASE_FEATURE(kSuggestionHoverFillShape,
             "OmniboxSuggestionHoverFillShape",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, use Assistant for omnibox voice query recognition instead of
// Android's built-in voice recognition service. Only works on Android.
BASE_FEATURE(kOmniboxAssistantVoiceSearch,
             "OmniboxAssistantVoiceSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Omnibox LHS and RHS icons are updated to match CR23
// guidelines.
BASE_FEATURE(kOmniboxCR23SteadyStateIcons,
             "kOmniboxCR23SteadyStateIcons",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Omnibox "steady state" background color is updated to match CR23
// guidelines.
BASE_FEATURE(kOmniboxSteadyStateBackgroundColor,
             "OmniboxSteadyStateBackgroundColor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Omnibox "steady state" height is increased from 28 dp to 34 dp to
// match CR23 guidelines.
// TODO(manukh): Clean up feature code 9/12 when m117 reaches stable; we're
//   launching the rest of CR23 in m117.
BASE_FEATURE(kOmniboxSteadyStateHeight,
             "OmniboxSteadyStateHeight",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Omnibox "steady state" text style is updated to match CR23
// guidelines.
BASE_FEATURE(kOmniboxSteadyStateTextStyle,
             "OmniboxSteadyStateTextStyle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Omnibox "steady state" text color is updated to match CR23
// guidelines.
BASE_FEATURE(kOmniboxSteadyStateTextColor,
             "OmniboxSteadyStateTextColor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, switching tabs will not restore the omnibox state.
// TODO(manukh): Should also blur the omnibox on tab switch.
BASE_FEATURE(kDiscardTemporaryInputOnTabSwitch,
             "OmniboxDiscardTemporaryInputOnTabSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable new Omnibox & Suggestions visual style.
BASE_FEATURE(kOmniboxModernizeVisualUpdate,
             "OmniboxModernizeVisualUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Android only flag that controls whether the new security indicator should be
// used, on non-Android platforms this is controlled through the
// ChromeRefresh2023 flag.
BASE_FEATURE(kUpdatedConnectionSecurityIndicators,
             "OmniboxUpdatedConnectionSecurityIndicators",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature used to default typed navigations to use HTTPS instead of HTTP.
// This only applies to navigations that don't have a scheme such as
// "example.com". Presently, typing "example.com" in a clean browsing profile
// loads http://example.com. When this feature is enabled, it should load
// https://example.com instead, with fallback to http://example.com if
// necessary.
BASE_FEATURE(kDefaultTypedNavigationsToHttps,
             "OmniboxDefaultTypedNavigationsToHttps",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Parameter name used to look up the delay before falling back to the HTTP URL
// while trying an HTTPS URL. The parameter is treated as a TimeDelta, so the
// unit must be included in the value as well (e.g. 3s for 3 seconds).
// - If the HTTPS load finishes successfully during this time, the timer is
//   cleared and no more work is done.
// - Otherwise, a new navigation to the the fallback HTTP URL is started.
const char kDefaultTypedNavigationsToHttpsTimeoutParam[] = "timeout";

// If enabled, Omnibox reports the Assisted Query Stats in the aqs= param in the
// Search Results Page URL.
BASE_FEATURE(kReportAssistedQueryStats,
             "OmniboxReportAssistedQueryStats",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, `OmniboxEditModel` uses a new version of `current_match_` that
// should be valid, and therefore usable, more often. The previous
// `current_match_` is almost always invalid and therefore the model often
// resorts to recalculating it each time its needed.
BASE_FEATURE(kRedoCurrentMatch,
             "OmniboxRedoCurrentMatch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, when reverting `OmniboxView`, it will first revert the
// `OmniboxEditModel` before closing the popup. This should be more performant;
// see comments in `OmniboxView::RevertAll()`.
BASE_FEATURE(kRevertModelBeforeClosingPopup,
             "OmniboxRevertModelBeforeClosingPopup",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, an existing `AutocompleteClient` will be used instead of
// generating a new one in `OmniboxEditModel`.
BASE_FEATURE(kUseExistingAutocompleteClient,
             "OmniboxUseExistingAutocompleteClient",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Omnibox reports the Searchbox Stats in the gs_lcrp= param in the
// Search Results Page URL.
BASE_FEATURE(kReportSearchboxStats,
             "OmniboxReportSearchboxStats",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, logs Omnibox URL scoring signals to OmniboxEventProto for
// training the ML scoring models.
BASE_FEATURE(kLogUrlScoringSignals,
             "LogUrlScoringSignals",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, runs the ML scoring model to assign new relevance scores to the
// URL suggestions and reranks them.
BASE_FEATURE(kMlUrlScoring, "MlUrlScoring", base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, creates Omnibox autocompete URL scoring model.
BASE_FEATURE(kUrlScoringModel,
             "UrlScoringModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, appends additional Trending and Recent Search Related Queries to
// the suggestion list on the NTP and SRP.
BASE_FEATURE(kInspireMe, "OmniboxInspireMe", enabled_by_default_android_only);

// Actions in Suggest is a data-driven feature; it's considered enabled when the
// data is available.
// The feature flag below helps us tune feature behaviors.
BASE_FEATURE(kActionsInSuggest,
             "OmniboxActionsInSuggest",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, treats categorical suggestions just like the entity suggestions
// by reusing the `ACMatchType::SEARCH_SUGGEST_ENTITY` and reports the original
// `omnibox::TYPE_CATEGORICAL_QUERY` to the server.
BASE_FEATURE(kCategoricalSuggestions,
             "CategoricalSuggestions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, merges the suggestion subtypes for the remote suggestions and the
// local verbatim and history suggestion duplicates at the provider level. This
// is needed for omnibox::kCategoricalSuggestions to function correctly but is
// being controlled by a separate feature in case there are unintended side
// effects beyond the categorical suggestions.
BASE_FEATURE(kMergeSubtypes, "MergeSubtypes", base::FEATURE_ENABLED_BY_DEFAULT);

bool IsOmniboxCr23CustomizeGuardedFeatureEnabled(const base::Feature& feature) {
  if (!features::CustomizeChromeSupportsChromeRefresh2023()) {
    // Bail before checking any other feature flags so that associated studies
    // don't get activated.
    return false;
  }

  return features::GetChromeRefresh2023Level() ==
             features::ChromeRefresh2023Level::kLevel2 ||
         base::FeatureList::IsEnabled(feature);
}

// If enabled, sends a signal when a user touches down on a search suggestion to
// |SearchPrefetchService|. |SearchPrefetchService| will then prefetch
// suggestion iff the SearchNavigationPrefetch feature and "touch_down" param
// are enabled.
BASE_FEATURE(kOmniboxTouchDownTriggerForPrefetch,
             "OmniboxTouchDownTriggerForPrefetch",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace omnibox
