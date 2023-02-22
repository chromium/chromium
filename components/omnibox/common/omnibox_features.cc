// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/omnibox_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

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

constexpr auto enabled_by_default_ios_only =
#if BUILDFLAG(IS_IOS)
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

// Comment out this macro since it is currently not being used in this file.
// const auto enabled_by_default_android_ios =
// #if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
//     base::FEATURE_ENABLED_BY_DEFAULT;
// #else
//     base::FEATURE_DISABLED_BY_DEFAULT;
// #endif

// Feature used to enable various experiments on keyword mode, UI and
// suggestions.
BASE_FEATURE(kExperimentalKeywordMode,
             "OmniboxExperimentalKeywordMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to enable showing thumbnail in front of the Omnibox clipboard image
// search suggestion.
BASE_FEATURE(kImageSearchSuggestionThumbnail,
             "ImageSearchSuggestionThumbnail",
             enabled_by_default_android_only);

// Feature used to allow users to remove suggestions from clipboard.
BASE_FEATURE(kOmniboxRemoveSuggestionsFromClipboard,
             "OmniboxRemoveSuggestionsFromClipboard",
             enabled_by_default_android_only);

// When enabled, uses the grouping framework (i.e.
// autocomplete_grouper_sections.h) to limit and group (but not sort) matches.
BASE_FEATURE(kGroupingFramework,
             "OmniboxGroupingFramework",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Demotes the relevance scores when comparing suggestions based on the
// suggestion's |AutocompleteMatchType| and the user's |PageClassification|.
// This feature's main job is to contain the DemoteByType parameter.
BASE_FEATURE(kOmniboxDemoteByType,
             "OmniboxDemoteByType",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Remove Excessive Clear Calls on RecycledViewPool in Omnibox.
// The feature improves efficiency of the RecycledViewPool by removing excessive
// calls to RecycledViewPool#clear().
BASE_FEATURE(kOmniboxRemoveExcessiveRecycledViewClearCalls,
             "OmniboxRemoveExcessiveRecycledViewClearCalls",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature to tweak how the default suggestion is preserved. Feature params
// control which tweaks specifically are enabled. Enabling this feature without
// params is a no-op.
// TODO(manukh) Enabled by default 2/15/23 m112. Clean up feature code 4/4 when
//   m112 reaches stable.
BASE_FEATURE(kPreserveDefault,
             "OmniboxPreserveDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When disabled, when providers update their matches, the new set of matches
// are sorted and culled, then merged with the old matches, then sorted and
// culled again. When enabled, the first sort and cull is skipped.
BASE_FEATURE(kSingleSortAndCullPass,
             "OmniboxSingleSortAndCullPass",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to debounce `AutocompleteController::NotifyChanged()`.
BASE_FEATURE(kUpdateResultDebounce,
             "OmniboxUpdateResultDebounce",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             enabled_by_default_desktop_android);

// Feature used to cap max suggestions to a dynamic limit based on how many URLs
// would be shown. E.g., show up to 10 suggestions if doing so would display no
// URLs; else show up to 8 suggestions if doing so would include 1 or more URLs.
BASE_FEATURE(kDynamicMaxAutocomplete,
             "OmniboxDynamicMaxAutocomplete",
             enabled_by_default_desktop_android);

// If enabled, proactively sets the `stripped_destination_url` for the entity
// suggestions with identical search terms so they are not erroneously deduped.
BASE_FEATURE(kDisambiguateEntitySuggestions,
             "DisambiguateEntitySuggestions",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
             enabled_by_default_android_only);

// Enables on-focus zero-prefix suggestions on the SRP.
BASE_FEATURE(kFocusTriggersSRPZeroSuggest,
             "OmniboxFocusTriggersSRPZeroSuggest",
             enabled_by_default_android_only);

// If enabled, keeps all zero-prefix suggestions in the second column and does
// not count them toward the overall zero-suggest limit.
BASE_FEATURE(kKeepSecondaryZeroSuggest,
             "KeepSecondaryZeroSuggest",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kOnDeviceTailModel,
             "OmniboxOnDeviceTailModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, changes the way Google-provided search suggestions are scored by
// the backend. Note that this Feature is only used for triggering a server-
// side experiment config that will send experiment IDs to the backend. It is
// not referred to in any of the Chromium code.
BASE_FEATURE(kOmniboxExperimentalSuggestScoring,
             "OmniboxExperimentalSuggestScoring",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, suggestions from a cgi param name match are scored to 0.
BASE_FEATURE(kDisableCGIParamMatching,
             "OmniboxDisableCGIParamMatching",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Features used to enable matching short inputs to bookmarks for suggestions.
// By default, if both of the following are disabled, input words shorter than 3
//   characters won't prefix match bookmarks. E.g., the inputs 'abc x' or 'x'
//   won't match bookmark text 'abc xyz'.
// If |kShortBookmarkSuggestions()| is enabled, this limitation is lifted and
//   both inputs 'abc x' and 'x' can match bookmark text 'abc xyz'.
// If |kShortBookmarkSuggestionsByTotalInputLength()| is enabled, matching is
//   limited by input length rather than input word length. Input 'abc x' can
//   but input 'x' can't match bookmark text 'abc xyz'.
BASE_FEATURE(kShortBookmarkSuggestions,
             "OmniboxShortBookmarkSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);
// TODO(manukh): Clean up 4/4/23 when m112 reaches stable.
BASE_FEATURE(kShortBookmarkSuggestionsByTotalInputLength,
             "OmniboxShortBookmarkSuggestionsByTotalInputLength",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, inputs may match bookmark paths. These path matches won't
// contribute to scoring. E.g. 'planets jupiter' can suggest a bookmark titled
// 'Jupiter' with URL 'en.wikipedia.org/wiki/Jupiter' located in a path
// containing 'planet.'
// TODO(manukh): Clean up 4/4/23 when m112 reaches stable.
BASE_FEATURE(kBookmarkPaths,
             "OmniboxBookmarkPaths",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, when updating or creating a shortcut, the last word of the input
// is expanded, if possible, to a complete word in the suggestion description.
BASE_FEATURE(kShortcutExpanding,
             "OmniboxShortcutExpanding",
             enabled_by_default_desktop_only);

// If enabled, the shortcut provider is more aggressive in scoring. The exact
// details will change over time; but ATM, the shortcut provider will chose 1
// candidate to compete with HUP's URL-what-you-typed suggestion for the default
// slot.
BASE_FEATURE(kShortcutBoost,
             "OmniboxShortcutBoost",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the relevant AutocompleteProviders will store "title" data in
// AutocompleteMatch::contents and "URL" data in AutocompleteMatch::description
// for URL-based omnibox suggestions (see crbug.com/1202964 for more details).
BASE_FEATURE(kStoreTitleInContentsAndUrlInDescription,
             "OmniboxStoreTitleInContentsAndUrlInDescription",
             base::FEATURE_DISABLED_BY_DEFAULT);

// HQP scores suggestions higher when it finds fewer matches. When enabled,
// HQP will consider the count of unique hosts, rather than the total count of
// matches.
// TODO(manukh): Clean up 3/7/23 when m111 reaches stable.
BASE_FEATURE(kHistoryQuickProviderSpecificityScoreCountUniqueHosts,
             "OmniboxHistoryQuickProviderSpecificityScoreCountUniqueHosts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature used to fetch document suggestions.
BASE_FEATURE(kDocumentProvider,
             "OmniboxDocumentProvider",
             enabled_by_default_desktop_only);

// Feature to determine if the HQP should double as a domain provider by
// suggesting up to the provider limit for each of the user's highly visited
// domains.
BASE_FEATURE(kDomainSuggestions,
             "OmniboxDomainSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows Omnibox to dynamically adjust number of offered suggestions to fill in
// the space between Omnibox and the soft keyboard. The number of suggestions
// shown will be no less than minimum for the platform (eg. 5 for Android).
BASE_FEATURE(kAdaptiveSuggestionsCount,
             "OmniboxAdaptiveSuggestionsCount",
             enabled_by_default_android_only);

// If enabled, clipboard suggestion will not show the clipboard content until
// the user clicks the reveal button.
BASE_FEATURE(kClipboardSuggestionContentHidden,
             "ClipboardSuggestionContentHidden",
             enabled_by_default_android_only);

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
             enabled_by_default_android_only);

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

// Feature used to enable URL suggestions for inputs that may contain typos.
BASE_FEATURE(kOmniboxFuzzyUrlSuggestions,
             "OmniboxFuzzyUrlSuggestions",
             enabled_by_default_desktop_only);

// Feature used to enable the default browser pedal.
BASE_FEATURE(kOmniboxDefaultBrowserPedal,
             "OmniboxDefaultBrowserPedal",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature used to synchronize the toolbar's and status bar's color.
BASE_FEATURE(kOmniboxMatchToolbarAndStatusBarColor,
             "OmniboxMatchToolbarAndStatusBarColor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature used to add a recycled view pool on the most visited tile carousel.
BASE_FEATURE(kOmniboxMostVisitedTilesAddRecycledViewPool,
             "OmniboxMostVisitedTilesAddRecycledViewPool",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature used to add most visited tiles to the suggestions when the user is on
// a search result page that does not do search term replacement.
BASE_FEATURE(kOmniboxMostVisitedTilesOnSrp,
             "OmniboxMostVisitedTilesOnSrp",
             enabled_by_default_ios_only);

// If enabled, adds a grey square background to search icons, and makes answer
// icon square instead of round.
BASE_FEATURE(kSquareSuggestIcons,
             "OmniboxSquareIcons",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, forces omnibox suggestion rows to be uniformly sized.
BASE_FEATURE(kUniformRowHeight,
             "OmniboxUniformRowHeight",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, shows the omnibox suggestions popup in WebUI.
BASE_FEATURE(kWebUIOmniboxPopup,
             "WebUIOmniboxPopup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, use Assistant for omnibox voice query recognition instead of
// Android's built-in voice recognition service. Only works on Android.
BASE_FEATURE(kOmniboxAssistantVoiceSearch,
             "OmniboxAssistantVoiceSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Omnibox "steady state" height is increased from 28 dp to 34 dp to
// match GM3 guidelines.
BASE_FEATURE(kOmniboxSteadyStateHeight,
             "OmniboxSteadyStateHeight",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClosePopupWithEscape,
             "OmniboxClosePopupWithEscape",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, adds a "starter pack" of @history, @bookmarks, and @settings
// scopes to Site Search/Keyword Mode.
BASE_FEATURE(kSiteSearchStarterPack,
             "OmniboxSiteSearchStarterPack",
             enabled_by_default_desktop_only);

// Experiment to introduce new security indicators for HTTPS.
BASE_FEATURE(kUpdatedConnectionSecurityIndicators,
             "OmniboxUpdatedConnectionSecurityIndicators",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Omnibox reports the Searchbox Stats in the gs_lcrp= param in the
// Search Results Page URL.
BASE_FEATURE(kReportSearchboxStats,
             "OmniboxReportSearchboxStats",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, logs Omnibox URL scoring signals to OmniboxEventProto in UMA.
BASE_FEATURE(kLogUrlScoringSignals,
             "LogUrlScoringSignals",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, runs the ML scoring model to assign relevance scores to URL
// suggestions. This enables the autocomplete system related changes to support
// ML scoring and moves scoring out of the autocomplete providers into
// autocomplete controller.
BASE_FEATURE(kMlRelevanceScoring,
             "MlRelevanceScoring",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, creates Omnibox autocompete URL scoring model.
BASE_FEATURE(kUrlScoringModel,
             "UrlScoringModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace omnibox
