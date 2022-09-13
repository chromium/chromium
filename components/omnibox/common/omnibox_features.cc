// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/omnibox_features.h"

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
const base::Feature kExperimentalKeywordMode{"OmniboxExperimentalKeywordMode",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to enable showing thumbnail in front of the Omnibox clipboard image
// search suggestion.
const base::Feature kImageSearchSuggestionThumbnail{
    "ImageSearchSuggestionThumbnail", enabled_by_default_android_only};

// Feature used to allow users to remove suggestions from clipboard.
const base::Feature kOmniboxRemoveSuggestionsFromClipboard{
    "OmniboxRemoveSuggestionsFromClipboard", enabled_by_default_android_only};

// Auxiliary search for Android. See http://crbug/1310100 for more details.
const base::Feature kAndroidAuxiliarySearch{"AndroidAuxiliarySearch",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables various tweaks to `AutocompleteController` autocompletion twiddling
// that may improve autocompletion stability. Feature params control which
// tweaks specifically are enabled. Enabling this feature without params is a
// no-op.
const base::Feature kAutocompleteStability{"OmniboxAutocompleteStability",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to enable memoizing and filtering non-doc hosts for
// `DocumentProvider::GetURLForDeduping()`.
const base::Feature kDocumentProviderDedupingOptimization{
    "OmniboxDocumentProviderDedupingOptimization",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to tweak how the default suggestion is preserved. Feature params
// control which tweaks specifically are enabled. Enabling this feature without
// params is a no-op.
const base::Feature kPreserveDefault{"OmniboxPreserveDefault",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Demotes the relevance scores when comparing suggestions based on the
// suggestion's |AutocompleteMatchType| and the user's |PageClassification|.
// This feature's main job is to contain the DemoteByType parameter.
const base::Feature kOmniboxDemoteByType{"OmniboxDemoteByType",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to enable memoizing URLs when replacing search terms in
// `AutocompleteMatch::GURLToStrippedGURL()`.
const base::Feature kStrippedGurlOptimization{
    "OmniboxStrippedGurlOptimization", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to debounce `AutocompleteController::UpdateResult()`.
const base::Feature kUpdateResultDebounce{"OmniboxUpdateResultDebounce",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to cap max zero suggestions shown according to the param
// OmniboxMaxZeroSuggestMatches. If omitted,
// OmniboxUIExperimentMaxAutocompleteMatches will be used instead. If present,
// OmniboxMaxZeroSuggestMatches will override
// OmniboxUIExperimentMaxAutocompleteMatches when |from_omnibox_focus| is true.
const base::Feature kMaxZeroSuggestMatches{"OmniboxMaxZeroSuggestMatches",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to cap max suggestions shown according to the params
// UIMaxAutocompleteMatches and UIMaxAutocompleteMatchesByProvider.
const base::Feature kUIExperimentMaxAutocompleteMatches{
    "OmniboxUIExperimentMaxAutocompleteMatches",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to cap the number of URL-type matches shown within the
// Omnibox. If enabled, the number of URL-type matches is limited (unless
// there are no more non-URL matches available.) If enabled, there is a
// companion parameter - OmniboxMaxURLMatches - which specifies the maximum
// desired number of URL-type matches.
const base::Feature kOmniboxMaxURLMatches{"OmniboxMaxURLMatches",
                                          enabled_by_default_desktop_android};

// Feature used to cap max suggestions to a dynamic limit based on how many URLs
// would be shown. E.g., show up to 10 suggestions if doing so would display no
// URLs; else show up to 8 suggestions if doing so would include 1 or more URLs.
const base::Feature kDynamicMaxAutocomplete{"OmniboxDynamicMaxAutocomplete",
                                            enabled_by_default_desktop_android};

// Used to adjust the relevance for the local history zero-prefix suggestions.
// If enabled, the relevance is determined by this feature's companion
// parameter, OmniboxFieldTrial::kLocalHistoryZeroSuggestRelevanceScore.
const base::Feature kAdjustLocalHistoryZeroSuggestRelevanceScore{
    "AdjustLocalHistoryZeroSuggestRelevanceScore",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables on-clobber (i.e., when the user clears the whole omnibox text)
// zero-prefix suggestions on the Open Web, that are contextual to the current
// URL. Will only work if user is signed-in and syncing, or is otherwise
// eligible to send the current page URL to the suggest server.
const base::Feature kClobberTriggersContextualWebZeroSuggest{
    "OmniboxClobberTriggersContextualWebZeroSuggest",
    enabled_by_default_desktop_only};

// Enables on-clobber (i.e., when the user clears the whole omnibox text)
// zero-prefix suggestions on the SRP.
const base::Feature kClobberTriggersSRPZeroSuggest{
    "OmniboxClobberTriggersSRPZeroSuggest", enabled_by_default_desktop_only};

// Enables on-focus zero-prefix suggestions on the Open Web, that are contextual
// to the current URL. Will only work if user is signed-in and syncing, or is
// otherwise eligible to send the current page URL to the suggest server.
const base::Feature kFocusTriggersContextualWebZeroSuggest{
    "OmniboxFocusTriggersContextualWebZeroSuggest",
    enabled_by_default_android_only};

// Enables on-focus zero-prefix suggestions on the SRP.
const base::Feature kFocusTriggersSRPZeroSuggest{
    "OmniboxFocusTriggersSRPZeroSuggest", enabled_by_default_android_only};

// Revamps how local search history is extracted and processed for generating
// zero-prefix and prefix suggestions.
extern const base::Feature kLocalHistorySuggestRevamp{
    "LocalHistorySuggestRevamp", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables local history zero-prefix suggestions in every context in which the
// remote zero-prefix suggestions are enabled.
const base::Feature kLocalHistoryZeroSuggestBeyondNTP{
    "LocalHistoryZeroSuggestBeyondNTP", base::FEATURE_DISABLED_BY_DEFAULT};

// Used to adjust the age threshold since the last visit in order to consider a
// normalized keyword search term as a zero-prefix suggestion. If disabled, the
// default value of 60 days for Desktop and 7 days for Android and iOS is used.
// If enabled, the age threshold is determined by this feature's companion
// parameter, OmniboxFieldTrial::kOmniboxLocalZeroSuggestAgeThresholdParam.
const base::Feature kOmniboxLocalZeroSuggestAgeThreshold{
    "OmniboxLocalZeroSuggestAgeThreshold", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables on-focus zero-prefix suggestions on the NTP for signed-out users.
const base::Feature kZeroSuggestOnNTPForSignedOutUsers{
    "OmniboxTrendingZeroPrefixSuggestionsOnNTP",
    enabled_by_default_desktop_android};

// Enables prefetching of the zero prefix suggestions for eligible users on NTP.
const base::Feature kZeroSuggestPrefetching{"ZeroSuggestPrefetching",
                                            enabled_by_default_desktop_android};

// Enables prefetching of the zero prefix suggestions for eligible users on SRP.
const base::Feature kZeroSuggestPrefetchingOnSRP{
    "ZeroSuggestPrefetchingOnSRP", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables prefetching of the zero prefix suggestions for eligible users on the
// Web (i.e. non-NTP and non-SRP URLs).
const base::Feature kZeroSuggestPrefetchingOnWeb{
    "ZeroSuggestPrefetchingOnWeb", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, zero prefix suggestions will be stored using an in-memory caching
// service, instead of using the existing prefs-based cache.
extern const base::Feature kZeroSuggestInMemoryCaching{
    "ZeroSuggestInMemoryCaching", base::FEATURE_DISABLED_BY_DEFAULT};

// Features to provide non personalized head search suggestion from a compact
// on device model. More specifically, feature name with suffix Incognito /
// NonIncognito will only controls behaviors under incognito / non-incognito
// mode respectively.
const base::Feature kOnDeviceHeadProviderIncognito{
    "OmniboxOnDeviceHeadProviderIncognito", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kOnDeviceHeadProviderNonIncognito{
    "OmniboxOnDeviceHeadProviderNonIncognito",
    base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, changes the way Google-provided search suggestions are scored by
// the backend. Note that this Feature is only used for triggering a server-
// side experiment config that will send experiment IDs to the backend. It is
// not referred to in any of the Chromium code.
const base::Feature kOmniboxExperimentalSuggestScoring{
    "OmniboxExperimentalSuggestScoring", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, suggestions from a cgi param name match are scored to 0.
const base::Feature kDisableCGIParamMatching{"OmniboxDisableCGIParamMatching",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Features used to enable matching short inputs to bookmarks for suggestions.
// By default, if both of the following are disabled, input words shorter than 3
//   characters won't prefix match bookmarks. E.g., the inputs 'abc x' or 'x'
//   won't match bookmark text 'abc xyz'.
// If |kShortBookmarkSuggestions()| is enabled, this limitation is lifted and
//   both inputs 'abc x' and 'x' can match bookmark text 'abc xyz'.
// If |kShortBookmarkSuggestionsByTotalInputLength()| is enabled, matching is
//   limited by input length rather than input word length. Input 'abc x' can
//   but input 'x' can't match bookmark text 'abc xyz'.
const base::Feature kShortBookmarkSuggestions{
    "OmniboxShortBookmarkSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kShortBookmarkSuggestionsByTotalInputLength{
    "OmniboxShortBookmarkSuggestionsByTotalInputLength",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, inputs may match bookmark paths. These path matches won't
// contribute to scoring. E.g. 'planets jupiter' can suggest a bookmark titled
// 'Jupiter' with URL 'en.wikipedia.org/wiki/Jupiter' located in a path
// containing 'planet.'
const base::Feature kBookmarkPaths{"OmniboxBookmarkPaths",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If disabled, shortcuts to the same stripped destination URL are scored
// independently, and only the highest scored shortcut is kept. If enabled,
// duplicate shortcuts are given an aggregate score, as if they had been a
// single shortcut.
const base::Feature kAggregateShortcuts{"OmniboxAggregateShortcuts",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, when updating or creating a shortcut, the last word of the input
// is expanded, if possible, to a complete word in the suggestion description.
const base::Feature kShortcutExpanding{"OmniboxShortcutExpanding",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the relevant AutocompleteProviders will store "title" data in
// AutocompleteMatch::contents and "URL" data in AutocompleteMatch::description
// for URL-based omnibox suggestions (see crbug.com/1202964 for more details).
const base::Feature kStoreTitleInContentsAndUrlInDescription{
    "OmniboxStoreTitleInContentsAndUrlInDescription",
    base::FEATURE_DISABLED_BY_DEFAULT};

// HQP scores suggestions higher when it finds fewer matches. When enabled,
// HQP will consider the count of unique hosts, rather than the total count of
// matches.
const base::Feature kHistoryQuickProviderSpecificityScoreCountUniqueHosts{
    "OmniboxHistoryQuickProviderSpecificityScoreCountUniqueHosts",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to fetch document suggestions.
const base::Feature kDocumentProvider{"OmniboxDocumentProvider",
                                      enabled_by_default_desktop_only};

// Feature to determine a value in the drive request indicating whether the
// request should be served by the  ASO backend.
const base::Feature kDocumentProviderAso{"OmniboxDocumentProviderAso",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Allows Omnibox to dynamically adjust number of offered suggestions to fill in
// the space between Omnibox and the soft keyboard. The number of suggestions
// shown will be no less than minimum for the platform (eg. 5 for Android).
const base::Feature kAdaptiveSuggestionsCount{"OmniboxAdaptiveSuggestionsCount",
                                              enabled_by_default_android_only};

// If enabled, clipboard suggestion will not show the clipboard content until
// the user clicks the reveal button.
const base::Feature kClipboardSuggestionContentHidden = {
    "ClipboardSuggestionContentHidden", enabled_by_default_android_only};

// If enabled, finance ticker answer from omnibox will reverse the color for
// stock ticker. only colors being swapped are those that represent "growth" and
// "loss" to represent colors red and green in a way that is appropriate for a
// given country/culture
const base::Feature kSuggestionAnswersColorReverse = {
    "SuggestionAnswersColorReverse", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, frequently visited sites are presented in form of a single row
// with a carousel of tiles, instead of one URL per row.
extern const base::Feature kMostVisitedTiles{"OmniboxMostVisitedTiles",
                                             enabled_by_default_android_only};

// If enabled, computes spacing between MV tiles so that about 4.5 tiles are
// shown on screen on narrow devices.
extern const base::Feature kMostVisitedTilesDynamicSpacing{
    "OmniboxMostVisitedTilesDynamicSpacing", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, permits the title on the MostVisitedTiles to wrap around to
// second line.
extern const base::Feature kMostVisitedTilesTitleWrapAround{
    "OmniboxMostVisitedTilesTitleWrapAround",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, expands autocompletion to possibly (depending on params) include
// suggestion titles and non-prefixes as opposed to be restricted to URL
// prefixes. Will also adjust the location bar UI and omnibox text selection to
// accommodate the autocompletions.
const base::Feature kRichAutocompletion{"OmniboxRichAutocompletion",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable Pedals in the NTP Realbox.
const base::Feature kNtpRealboxPedals{"NtpRealboxPedals",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable Suggestion Answers in the NTP Realbox.
const base::Feature kNtpRealboxSuggestionAnswers{
    "NtpRealboxSuggestionAnswers", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature used to enable Tail Suggest Formatting in the NTP Realbox.
const base::Feature kNtpRealboxTailSuggest{"NtpRealboxTailSuggest",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Feature used to enable URL suggestions for inputs that may contain typos.
const base::Feature kOmniboxFuzzyUrlSuggestions{
    "OmniboxFuzzyUrlSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to update the left and bottom padding of the omnibox suggestion
// header.
const base::Feature kOmniboxHeaderPaddingUpdate{
    "OmniboxHeaderPaddingUpdate", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to remove the capitalization of the suggestion header text.
const base::Feature kOmniboxRemoveSuggestionHeaderCapitalization{
    "OmniboxRemoveSuggestionHeaderCapitalization",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to remove the chevron on the right side of suggestion list
// header under omnibox.
const base::Feature kOmniboxRemoveSuggestionHeaderChevron{
    "OmniboxRemoveSuggestionHeaderChevron", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to add fading effect to most visited tiles on tablet.
const base::Feature kOmniboxMostVisitedTilesFadingOnTablet{
    "OmniboxMostVisitedTilesFadingOnTablet", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to add most visited tiles to the suggestions when the user is on
// a search result page that does not do search term replacement.
const base::Feature kOmniboxMostVisitedTilesOnSrp{
    "OmniboxMostVisitedTilesOnSrp", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, use Assistant for omnibox voice query recognition instead of
// Android's built-in voice recognition service. Only works on Android.
const base::Feature kOmniboxAssistantVoiceSearch{
    "OmniboxAssistantVoiceSearch", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kClosePopupWithEscape{"OmniboxClosePopupWithEscape",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBlurWithEscape{"OmniboxBlurWithEscape",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, adds a "starter pack" of @history, @bookmarks, and @settings
// scopes to Site Search/Keyword Mode.
const base::Feature kSiteSearchStarterPack{"OmniboxSiteSearchStarterPack",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Experiment to introduce new security indicators for HTTPS.
const base::Feature kUpdatedConnectionSecurityIndicators{
    "OmniboxUpdatedConnectionSecurityIndicators",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to default typed navigations to use HTTPS instead of HTTP.
// This only applies to navigations that don't have a scheme such as
// "example.com". Presently, typing "example.com" in a clean browsing profile
// loads http://example.com. When this feature is enabled, it should load
// https://example.com instead, with fallback to http://example.com if
// necessary.
const base::Feature kDefaultTypedNavigationsToHttps{
    "OmniboxDefaultTypedNavigationsToHttps",
    enabled_by_default_desktop_android};
// Parameter name used to look up the delay before falling back to the HTTP URL
// while trying an HTTPS URL. The parameter is treated as a TimeDelta, so the
// unit must be included in the value as well (e.g. 3s for 3 seconds).
// - If the HTTPS load finishes successfully during this time, the timer is
//   cleared and no more work is done.
// - Otherwise, a new navigation to the the fallback HTTP URL is started.
const char kDefaultTypedNavigationsToHttpsTimeoutParam[] = "timeout";

// If enabled, Omnibox reports the Assisted Query Stats in the aqs= param in the
// Search Results Page URL.
const base::Feature kReportAssistedQueryStats{"OmniboxReportAssistedQueryStats",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, Omnibox reports the Searchbox Stats in the gs_lcrp= param in the
// Search Results Page URL.
extern const base::Feature kReportSearchboxStats{
    "OmniboxReportSearchboxStats", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, retains all suggestions with headers to be presented entirely.
// Disabling the feature trims the suggestions list to the predefined limit.
extern const base::Feature kRetainSuggestionsWithHeaders{
    "OmniboxRetainSuggestionsWithHeaders", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace omnibox
