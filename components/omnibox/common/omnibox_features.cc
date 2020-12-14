// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/omnibox_features.h"

#include "build/build_config.h"

namespace omnibox {

const auto enabled_by_default_desktop_only =
#if defined(OS_ANDROID) || defined(OS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

const auto enabled_by_default_android_only =
#if defined(OS_ANDROID)
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

const auto enabled_by_default_desktop_android =
#if defined(OS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

const auto enabled_by_default_desktop_ios =
#if defined(OS_ANDROID)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

// Comment out this macro since it is currently not being used in this file.
// const auto enabled_by_default_android_ios =
// #if defined(OS_ANDROID) || defined(OS_IOS)
//     base::FEATURE_ENABLED_BY_DEFAULT;
// #else
//     base::FEATURE_DISABLED_BY_DEFAULT;
// #endif

// Allows Omnibox to dynamically adjust number of offered suggestions to fill in
// the space between Omnibox an the soft keyboard. The number of suggestions
// shown will be no less than minimum for the platform (eg. 5 for Android).
const base::Feature kAdaptiveSuggestionsCount{
    "OmniboxAdaptiveSuggestionsCount", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to hide the scheme from steady state URLs displayed in the
// toolbar. It is restored during editing.
const base::Feature kHideFileUrlScheme{
    "OmniboxUIExperimentHideFileUrlScheme",
    // Android and iOS don't have the File security chip, and therefore still
    // need to show the file scheme.
    enabled_by_default_desktop_only};

// Feature used to enable matching short words to bookmarks for suggestions.
const base::Feature kOmniboxShortBookmarkSuggestions{
    "OmniboxShortBookmarkSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to force on the experiment of transmission of tail suggestions
// from GWS to this client, currently testing for desktop.
const base::Feature kOmniboxTailSuggestions{"OmniboxTailSuggestions",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Feature that enables the tab-switch suggestions corresponding to an open
// tab, for a button or dedicated suggestion. Enabled by default on Desktop
// and iOS.
const base::Feature kOmniboxTabSwitchSuggestions{
    "OmniboxTabSwitchSuggestions", enabled_by_default_desktop_ios};

// Feature used to enable various experiments on keyword mode, UI and
// suggestions.
const base::Feature kExperimentalKeywordMode{"OmniboxExperimentalKeywordMode",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to enable showing thumbnail in front of the Omnibox clipboard image
// search suggestion.
const base::Feature kImageSearchSuggestionThumbnail{
    "ImageSearchSuggestionThumbnail", enabled_by_default_android_only};

// Feature to enable the search provider to send a request to the suggest
// server on focus.  This allows the suggest server to warm up, by, for
// example, loading per-user models into memory.  Having a per-user model
// in memory allows the suggest server to respond more quickly with
// personalized suggestions as the user types.
const base::Feature kSearchProviderWarmUpOnFocus{
    "OmniboxWarmUpSearchProviderOnFocus", enabled_by_default_desktop_android};

// Feature used to display the title of the current URL match.
const base::Feature kDisplayTitleForCurrentUrl{
    "OmniboxDisplayTitleForCurrentUrl", enabled_by_default_desktop_android};

// Feature used to always swap the title and URL.
const base::Feature kUIExperimentSwapTitleAndUrl{
    "OmniboxUIExperimentSwapTitleAndUrl", enabled_by_default_desktop_only};

// Feature used to enable speculatively starting a service worker associated
// with the destination of the default match when the user's input looks like a
// query.
const base::Feature kSpeculativeServiceWorkerStartOnQueryInput{
    "OmniboxSpeculativeServiceWorkerStartOnQueryInput",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Feature used to fetch document suggestions.
const base::Feature kDocumentProvider{"OmniboxDocumentProvider",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Show the search engine logo in the omnibox on Android (desktop already does
// this).
const base::Feature kOmniboxSearchEngineLogo{"OmniboxSearchEngineLogo",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Feature used to allow users to remove suggestions from clipboard.
const base::Feature kOmniboxRemoveSuggestionsFromClipboard{
    "OmniboxRemoveSuggestionsFromClipboard", enabled_by_default_android_only};

// Feature to debounce drive requests from the document provider.
const base::Feature kDebounceDocumentProvider{
    "OmniboxDebounceDocumentProvider", base::FEATURE_DISABLED_BY_DEFAULT};

// Demotes the relevance scores when comparing suggestions based on the
// suggestion's |AutocompleteMatchType| and the user's |PageClassification|.
// This feature's main job is to contain the DemoteByType parameter.
const base::Feature kOmniboxDemoteByType{"OmniboxDemoteByType",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to reduce entity latency by sharing a decoder. Param values will
// configure other optimizations as well.
const base::Feature kEntitySuggestionsReduceLatency{
    "OmniboxEntitySuggestionsReduceLatency", base::FEATURE_DISABLED_BY_DEFAULT};

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
const bool kOmniboxMaxURLMatchesEnabledByDefault =
#if defined(OS_IOS) || defined(OS_ANDROID)
    false;
#else
    true;
#endif
const base::Feature kOmniboxMaxURLMatches{
    "OmniboxMaxURLMatches", kOmniboxMaxURLMatchesEnabledByDefault
                                ? base::FEATURE_ENABLED_BY_DEFAULT
                                : base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to cap max suggestions to a dynamic limit based on how many URLs
// would be shown. E.g., show up to 10 suggestions if doing so would display no
// URLs; else show up to 8 suggestions if doing so would include 1 or more URLs.
const base::Feature kDynamicMaxAutocomplete{"OmniboxDynamicMaxAutocomplete",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable bubbling URL suggestions above search suggestions
// after grouping if 2 conditions are met:
// 1) There must be a sufficient score gap between the adjacent searches.
// 2) There must be a sufficient buffer between the URL and search scores.
const base::Feature kBubbleUrlSuggestions{"OmniboxBubbleUrlSuggestions",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, when the user clears the whole omnibox text (i.e. via Backspace),
// Chrome will request remote ZeroSuggest suggestions for the OTHER page
// classification (contextual web).
const base::Feature kClobberTriggersContextualWebZeroSuggest{
    "OmniboxClobberTriggersContextualWebZeroSuggest",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Used to adjust the age threshold since the last visit in order to consider a
// normalized keyword search term as a zero-prefix suggestion. If disabled, the
// default value of history::kLowQualityMatchAgeLimitInDays is used. If enabled,
// the age threshold is determined by this feature's companion parameter,
// OmniboxFieldTrial::kOmniboxLocalZeroSuggestAgeThresholdParam.
const base::Feature kOmniboxLocalZeroSuggestAgeThreshold{
    "OmniboxLocalZeroSuggestAgeThreshold", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, enables local zero-prefix suggestions for signed in users.
// Local zero-prefix suggestions are enabled for signed in users by default. We
// will be experimenting with DISABLING this behavior.
const base::Feature kOmniboxLocalZeroSuggestForAuthenticatedUsers{
    "OmniboxLocalZeroSuggestForAuthenticatedUsers",
    base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, ranks the local zero-prefix suggestions based on frecency
// (combined frequency and recency).
const base::Feature kOmniboxLocalZeroSuggestFrecencyRanking{
    "OmniboxLocalZeroSuggestFrecencyRanking",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Used to force enable/disable trending zero-prefix suggestions on the NTP
// (Omnibox and NTP realbox). This feature triggers a server-side behavior only
// and has no direct impact on the client behavior.
const base::Feature kOmniboxTrendingZeroPrefixSuggestionsOnNTP{
    "OmniboxTrendingZeroPrefixSuggestionsOnNTP",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Used to enable/disable caching for remote zero-prefix suggestions. Caching is
// enabled by default. We will be experimenting with DISABLING this behavior.
const base::Feature kOmniboxZeroSuggestCaching{
    "OmniboxZeroSuggestCaching", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables on-focus suggestions on the Open Web, that are contextual to the
// current URL. Will only work if user is signed-in and syncing, or is
// otherwise eligible to send the current page URL to the suggest server.
//
// There's multiple flags here for multiple backend configurations:
//  - Default (search queries)
//  - On-Content Suggestions
const base::Feature kOnFocusSuggestionsContextualWeb{
    "OmniboxOnFocusSuggestionsContextualWeb",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kOnFocusSuggestionsContextualWebOnContent{
    "OmniboxOnFocusSuggestionsContextualWebOnContent",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Reactive Zero-Prefix Suggestions (rZPS) on the NTP, for the Omnibox
// and Realbox respectively. Note: enabling this feature merely makes
// ZeroSuggestProvider send the request. There are additional requirements,
// like the user being signed-in, and the suggest server having rZPS enabled.
const base::Feature kReactiveZeroSuggestionsOnNTPOmnibox{
    "OmniboxReactiveZeroSuggestionsOnNTPOmnibox",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kReactiveZeroSuggestionsOnNTPRealbox{
    "OmniboxReactiveZeroSuggestionsOnNTPRealbox",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Allows the LocalHistoryZeroSuggestProvider to use local search history.
const base::Feature kLocalHistoryZeroSuggest{
    "LocalHistoryZeroSuggest", enabled_by_default_desktop_android};

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
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, suggestions from a cgi param name match are scored to 0.
const base::Feature kDisableCGIParamMatching{"OmniboxDisableCGIParamMatching",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, uses the native VoiceSuggestProvider. Android-specific.
const base::Feature kNativeVoiceSuggestProvider{
    "OmniboxNativeVoiceSuggestProvider", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, inputs may match bookmark paths. These path matches won't
// contribute to scoring. E.g. 'planets jupiter' can suggest a bookmark titled
// 'Jupiter' with URL 'en.wikipedia.org/wiki/Jupiter' located in a path
// containing 'planet.'
const base::Feature kBookmarkPaths{"OmniboxBookmarkPaths",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, shows slightly more compact suggestions, allowing the
// kAdaptiveSuggestionsCount feature to fit more suggestions on screen.
const base::Feature kCompactSuggestions{"OmniboxCompactSuggestions",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// If enbaled, frequently visited sites are presented in form of a single row
// with a carousel of tiles, instead of one URL per row.
extern const base::Feature kMostVisitedTiles{"OmniboxMostVisitedTiles",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, expands autocompletion to possibly (depending on params) include
// suggestion titles and non-prefixes as opposed to be restricted to URL
// prefixes. Will also adjust the location bar UI and omnibox text selection to
// accommodate the autocompletions.
const base::Feature kRichAutocompletion{"OmniboxRichAutocompletion",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Feature that enables Search Ready Omnibox in incognito.
const base::Feature kOmniboxSearchReadyIncognito{
    "OmniboxSearchReadyIncognito", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature that puts a single row of buttons on suggestions with actionable
// elements like keywords, tab-switch buttons, and Pedals.
const base::Feature kOmniboxSuggestionButtonRow{
    "OmniboxSuggestionButtonRow", enabled_by_default_desktop_only};

// Feature used to enable Pedal suggestions.
const base::Feature kOmniboxPedalSuggestions{"OmniboxPedalSuggestions",
                                             enabled_by_default_desktop_only};

// Feature used to enable the keyword search button.
const base::Feature kOmniboxKeywordSearchButton{
    "OmniboxKeywordSearchButton", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables new UI changes indicating focus and hover states.
const base::Feature kOmniboxRefinedFocusState{
    "OmniboxRefinedFocusState", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, uses WebUI to render the omnibox suggestions popup, similar to
// how the NTP "realbox" is implemented.
const base::Feature kWebUIOmniboxPopup{"WebUIOmniboxPopup",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables rollout of changing the default behavior for DNS interception checks
// and did-you-mean infobar.
// Users who are in the enabled group for this feature will have interception
// checks and did-you-mean turned off. Enterprise Policy takes precedence over
// this setting, and policy is checked before the feature group is checked and
// marked.
const base::Feature kIntranetRedirectBehaviorPolicyRollout{
    "OmniboxDNSInterceptionChecksPolicyRollout",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, use Assistant for omnibox voice query recognition instead of
// Android's built-in voice recognition service. Only works on Android.
const base::Feature kOmniboxAssistantVoiceSearch{
    "OmniboxAssistantVoiceSearch", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to reveal the path, query and ref from steady state URLs
// on hover.
const base::Feature kRevealSteadyStateUrlPathQueryAndRefOnHover{
    "OmniboxUIExperimentRevealSteadyStateUrlPathQueryAndRefOnHover",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to hide the path, query and ref from steady state URLs
// on interaction with the page.
const base::Feature kHideSteadyStateUrlPathQueryAndRefOnInteraction{
    "OmniboxUIExperimentHideSteadyStateUrlPathQueryAndRefOnInteraction",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to possibly elide not just the path, query, and ref from steady
// state URLs, but also subdomains beyond the registrable domain, depending on
// whether the hostname fails lookalike checks. Has no effect unless
// kRevealSteadyStateUrlPathQueryAndRefOnHover and/or
// kHideSteadyStateUrlPathQueryAndRefOnInteraction are enabled.
const base::Feature kMaybeElideToRegistrableDomain{
    "OmniboxUIExperimentElideToRegistrableDomain",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to default typed navigations to use HTTPS instead of HTTP.
// This only applies to navigations that don't have a scheme such as
// "example.com". Presently, typing "example.com" in a clean browsing profile
// loads http://example.com. When this feature is enabled, it should load
// https://example.com instead, with fallback to http://example.com if
// necessary.
const base::Feature kDefaultTypedNavigationsToHttps{
    "OmniboxDefaultTypedNavigationsToHttps", base::FEATURE_DISABLED_BY_DEFAULT};

// NOTE: while this is enabled by default, CCT visits are only tagged with the
// necessary transition type if the intent launching CCT supplies the
// appropriate parameter.
const base::Feature kHideVisitsFromCct{"OmniboxHideVisitsFromCct",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace omnibox
