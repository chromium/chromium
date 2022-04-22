// Copyright 2019 The Chromium Authors. All rights reserved.
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

// Feature that enables the tab-switch suggestions corresponding to an open
// tab, for a button or dedicated suggestion. Enabled by default on Desktop, iOS
// and Android.
const base::Feature kOmniboxTabSwitchSuggestions{
    "OmniboxTabSwitchSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature used to enable various experiments on keyword mode, UI and
// suggestions.
const base::Feature kExperimentalKeywordMode{"OmniboxExperimentalKeywordMode",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to enable showing thumbnail in front of the Omnibox clipboard image
// search suggestion.
const base::Feature kImageSearchSuggestionThumbnail{
    "ImageSearchSuggestionThumbnail", enabled_by_default_android_only};

// Feature used to display the title of the current URL match.
const base::Feature kDisplayTitleForCurrentUrl{
    "OmniboxDisplayTitleForCurrentUrl", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature used to allow users to remove suggestions from clipboard.
const base::Feature kOmniboxRemoveSuggestionsFromClipboard{
    "OmniboxRemoveSuggestionsFromClipboard", enabled_by_default_android_only};

// Auxiliary search for Android. See http://crbug/1310100 for more details.
const base::Feature kAndroidAuxiliarySearch{
    "AndroidAuxiliarySearch", base::FEATURE_DISABLED_BY_DEFAULT};

// Demotes the relevance scores when comparing suggestions based on the
// suggestion's |AutocompleteMatchType| and the user's |PageClassification|.
// This feature's main job is to contain the DemoteByType parameter.
const base::Feature kOmniboxDemoteByType{"OmniboxDemoteByType",
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

// If enabled, when the user clears the whole omnibox text (i.e. via Backspace),
// Chrome will request remote ZeroSuggest suggestions for the OTHER page
// classification (contextual web), which does NOT include the SRP.
const base::Feature kClobberTriggersContextualWebZeroSuggest{
    "OmniboxClobberTriggersContextualWebZeroSuggest",
    enabled_by_default_desktop_only};

// If enabled, when the user clears the whole omnibox text (i.e. via Backspace),
// Chrome will request remote ZeroSuggest suggestions for the SRP (search
// results page).
const base::Feature kClobberTriggersSRPZeroSuggest{
    "OmniboxClobberTriggersSRPZeroSuggest", enabled_by_default_desktop_only};

// Used to adjust the age threshold since the last visit in order to consider a
// normalized keyword search term as a zero-prefix suggestion. If disabled, the
// default value of 7 days is used. If enabled, the age threshold is determined
// by this feature's companion parameter,
// OmniboxFieldTrial::kOmniboxLocalZeroSuggestAgeThresholdParam.
const base::Feature kOmniboxLocalZeroSuggestAgeThreshold{
    "OmniboxLocalZeroSuggestAgeThreshold", base::FEATURE_DISABLED_BY_DEFAULT};

// Used to enable/disable remote zero-prefix suggestions on the NTP
// (Omnibox and NTP realbox). Enabling this feature permits the code to issue
// suggestions request to the server on the new tab page for users who decided
// not to sign in.
const base::Feature kOmniboxTrendingZeroPrefixSuggestionsOnNTP{
    "OmniboxTrendingZeroPrefixSuggestionsOnNTP",
    enabled_by_default_desktop_android};

// Enables on-focus suggestions on the Open Web, that are contextual to the
// current URL. Will only work if user is signed-in and syncing, or is
// otherwise eligible to send the current page URL to the suggest server.
//
// There's multiple flags here for multiple backend configurations:
//  - Default (search queries)
//  - SRP specific toggle (enables SRP on top of Web Pages for features below)
//  - On-Content Suggestions
//
// TODO(tommycli): It's confusing whether Contextual Web includes SRP or not.
// `kOnFocusSuggestionsContextualWebAllowSRP` suggests it's included, but
// `kClobberTriggersContextualWebZeroSuggest` suggests it's not. Make this
// consistent, probably by renaming flags to distinguish between OTHER and SRP.
const base::Feature kOnFocusSuggestionsContextualWeb{
    "OmniboxOnFocusSuggestionsContextualWeb",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kOnFocusSuggestionsContextualWebAllowSRP{
    "OmniboxOnFocusSuggestionsContextualWebAllowSRP",
    enabled_by_default_android_only};
const base::Feature kOnFocusSuggestionsContextualWebOnContent{
    "OmniboxOnFocusSuggestionsContextualWebOnContent",
    enabled_by_default_android_only};

// Allows the LocalHistoryZeroSuggestProvider to use local search history.
const base::Feature kLocalHistoryZeroSuggest{
    "LocalHistoryZeroSuggest", enabled_by_default_desktop_android};

// Enables prefetching of the zero prefix suggestions for signed-in users.
const base::Feature kZeroSuggestPrefetching{"ZeroSuggestPrefetching",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

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

// If enabled, the HistoryQuickProvider's InMemoryURLIndex service never
// persists its index to a cache file on shutdown, and instead always rebuilds
// it from the HistoryService on startup. Persisting the index to disk causes
// over 10% of all shutdown hangs.
const base::Feature kHistoryQuickProviderAblateInMemoryURLIndexCacheFile{
    "OmniboxHistoryQuickProviderAblateInMemoryURLIndexCacheFile",
    enabled_by_default_desktop_only};

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

// If disabled, shortcuts to the same stripped destination URL are scored
// independently, and only the highest scored shortcut is kept. If enabled,
// duplicate shortcuts are given an aggregate score, as if they had been a
// single shortcut.
const base::Feature kAggregateShortcuts{"OmniboxAggregateShortcuts",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, inputs may match bookmark paths. These path matches won't
// contribute to scoring. E.g. 'planets jupiter' can suggest a bookmark titled
// 'Jupiter' with URL 'en.wikipedia.org/wiki/Jupiter' located in a path
// containing 'planet.'
const base::Feature kBookmarkPaths{"OmniboxBookmarkPaths",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to fetch document suggestions.
const base::Feature kDocumentProvider{"OmniboxDocumentProvider",
                                      enabled_by_default_desktop_only};
// Feature to debounce drive requests from the document provider.
const base::Feature kDebounceDocumentProvider{"OmniboxDebounceDocumentProvider",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
// Feature to determine a value in the drive request indicating whether the
// request should be served by the  ASO backend.
const base::Feature kDocumentProviderAso{"OmniboxDocumentProviderAso",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Allows Omnibox to dynamically adjust number of offered suggestions to fill in
// the space between Omnibox and the soft keyboard. The number of suggestions
// shown will be no less than minimum for the platform (eg. 5 for Android).
const base::Feature kAdaptiveSuggestionsCount{"OmniboxAdaptiveSuggestionsCount",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, clipboard suggestion will not show the clipboard content until
// the user clicks the reveal button.
const base::Feature kClipboardSuggestionContentHidden = {
    "ClipboardSuggestionContentHidden", enabled_by_default_android_only};

// If enabled, frequently visited sites are presented in form of a single row
// with a carousel of tiles, instead of one URL per row.
extern const base::Feature kMostVisitedTiles{"OmniboxMostVisitedTiles",
                                             enabled_by_default_android_only};

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
    "NtpRealboxSuggestionAnswers", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable Tail Suggest Formatting in the NTP Realbox.
const base::Feature kNtpRealboxTailSuggest{"NtpRealboxTailSuggest",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable URL suggestions for inputs that may contain typos.
const base::Feature kOmniboxFuzzyUrlSuggestions{
    "OmniboxFuzzyUrlSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable the first batch of Pedals on Android. The Pedals,
// which will be enabled on Android, should be already enabled on desktop.
const base::Feature kOmniboxPedalsAndroidBatch1{
    "OmniboxPedalsAndroidBatch1", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable the third batch of Pedals (Find your phone, etc.)
// for non-English locales (English locales are 'en' and 'en-GB').
const base::Feature kOmniboxPedalsBatch3NonEnglish{
    "OmniboxPedalsBatch3NonEnglish", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, use Assistant for omnibox voice query recognition instead of
// Android's built-in voice recognition service. Only works on Android.
const base::Feature kOmniboxAssistantVoiceSearch{
    "OmniboxAssistantVoiceSearch", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kClosePopupWithEscape{"OmniboxClosePopupWithEscape",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBlurWithEscape{"OmniboxBlurWithEscape",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, add an Active Search Engines category to
// chrome://settings/searchEngines. This section contains any search engines
// that have been used or manually added/modified by the user.
const base::Feature kActiveSearchEngines{"OmniboxActiveSearchEngines",
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
    "OmniboxDefaultTypedNavigationsToHttps", base::FEATURE_ENABLED_BY_DEFAULT};
// Parameter name used to look up the delay before falling back to the HTTP URL
// while trying an HTTPS URL. The parameter is treated as a TimeDelta, so the
// unit must be included in the value as well (e.g. 3s for 3 seconds).
// - If the HTTPS load finishes successfully during this time, the timer is
//   cleared and no more work is done.
// - Otherwise, a new navigation to the the fallback HTTP URL is started.
const char kDefaultTypedNavigationsToHttpsTimeoutParam[] = "timeout";

// Spare renderer warmup for faster website loading.
const base::Feature kOmniboxSpareRenderer{"OmniboxSpareRenderer",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

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
