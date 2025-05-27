// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/omnibox_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/omnibox/common/jni_headers/OmniboxFeatureMap_jni.h"
#endif

namespace omnibox {
namespace {
constexpr bool IS_ANDROID = !!BUILDFLAG(IS_ANDROID);
constexpr bool IS_IOS = !!BUILDFLAG(IS_IOS);

constexpr base::FeatureState DISABLED = base::FEATURE_DISABLED_BY_DEFAULT;
constexpr base::FeatureState ENABLED = base::FEATURE_ENABLED_BY_DEFAULT;

constexpr base::FeatureState enable_if(bool condition) {
  return condition ? ENABLED : DISABLED;
}
}  // namespace

// Feature to enable showing thumbnail in front of the Omnibox clipboard image
// search suggestion.
BASE_FEATURE(kImageSearchSuggestionThumbnail,
             "ImageSearchSuggestionThumbnail",
             enable_if(IS_ANDROID));

// Feature used to allow users to remove suggestions from clipboard.
BASE_FEATURE(kOmniboxRemoveSuggestionsFromClipboard,
             "OmniboxRemoveSuggestionsFromClipboard",
             enable_if(IS_ANDROID));

// When enabled, uses the grouping framework with prefixed suggestions (i.e.
// autocomplete_grouper_sections.h) to limit and group (but not sort) matches.
BASE_FEATURE(kGroupingFrameworkForNonZPS,
             "OmniboxGroupingFrameworkForNonZPS",
             enable_if(IS_ANDROID));

// Demotes the relevance scores when comparing suggestions based on the
// suggestion's |AutocompleteMatchType| and the user's |PageClassification|.
// This feature's main job is to contain the DemoteByType parameter.
BASE_FEATURE(kOmniboxDemoteByType, "OmniboxDemoteByType", DISABLED);

// Feature used to cap max zero suggestions shown according to the param
// OmniboxMaxZeroSuggestMatches. If omitted,
// OmniboxUIExperimentMaxAutocompleteMatches will be used instead. If present,
// OmniboxMaxZeroSuggestMatches will override
// OmniboxUIExperimentMaxAutocompleteMatches when |from_omnibox_focus| is true.
BASE_FEATURE(kMaxZeroSuggestMatches, "OmniboxMaxZeroSuggestMatches", DISABLED);

// Feature used to cap max suggestions shown according to the params
// UIMaxAutocompleteMatches and UIMaxAutocompleteMatchesByProvider.
BASE_FEATURE(kUIExperimentMaxAutocompleteMatches,
             "OmniboxUIExperimentMaxAutocompleteMatches",
             DISABLED);

// Feature used to cap the number of URL-type matches shown within the
// Omnibox. If enabled, the number of URL-type matches is limited (unless
// there are no more non-URL matches available.) If enabled, there is a
// companion parameter - OmniboxMaxURLMatches - which specifies the maximum
// desired number of URL-type matches.
BASE_FEATURE(kOmniboxMaxURLMatches, "OmniboxMaxURLMatches", ENABLED);

// Feature used to cap max suggestions to a dynamic limit based on how many URLs
// would be shown. E.g., show up to 10 suggestions if doing so would display no
// URLs; else show up to 8 suggestions if doing so would include 1 or more URLs.
BASE_FEATURE(kDynamicMaxAutocomplete,
             "OmniboxDynamicMaxAutocomplete",
             enable_if(!IS_IOS));

// If enabled, takes the search intent query params into account for triggering
// switch to tab actions on matches.
BASE_FEATURE(kDisambiguateTabMatchingForEntitySuggestions,
             "DisambiguateTabMatchingForEntitySuggestions",
             ENABLED);

// Enables omnibox focus as a trigger for zero-prefix suggestions on web and
// SRP, subject to the same requirements and conditions as on-clobber
// suggestions.
BASE_FEATURE(kFocusTriggersWebAndSRPZeroSuggest,
             "OmniboxFocusTriggersWebAndSRPZeroSuggest",
             DISABLED);

// If enabled, suggestion group headers in the Omnibox popup will be hidden
// (e.g. in order to minimize visual clutter in the zero-prefix state).
BASE_FEATURE(kHideSuggestionGroupHeaders,
             "OmniboxHideSuggestionGroupHeaders",
             DISABLED);

// Enables local history zero-prefix suggestions in every context in which the
// remote zero-prefix suggestions are enabled.
BASE_FEATURE(kLocalHistoryZeroSuggestBeyondNTP,
             "LocalHistoryZeroSuggestBeyondNTP",
             DISABLED);

// If enabled, zero prefix suggestions will be stored using an in-memory caching
// service, instead of using the existing prefs-based cache.
BASE_FEATURE(kZeroSuggestInMemoryCaching,
             "ZeroSuggestInMemoryCaching",
             DISABLED);

// Enables the use of a request debouncer to throttle the number of ZPS prefetch
// requests initiated over a given period of time (to help minimize the
// performance impact of ZPS prefetching on the remote Suggest service).
BASE_FEATURE(kZeroSuggestPrefetchDebouncing,
             "ZeroSuggestPrefetchDebouncing",
             DISABLED);

// Enables prefetching of the zero prefix suggestions for eligible users on NTP.
BASE_FEATURE(kZeroSuggestPrefetching, "ZeroSuggestPrefetching", ENABLED);

// Enables prefetching of the zero prefix suggestions for eligible users on SRP.
BASE_FEATURE(kZeroSuggestPrefetchingOnSRP,
             "ZeroSuggestPrefetchingOnSRP",
             enable_if(!IS_ANDROID));

// Enables prefetching of the zero prefix suggestions for eligible users on the
// Web (i.e. non-NTP and non-SRP URLs).
BASE_FEATURE(kZeroSuggestPrefetchingOnWeb,
             "ZeroSuggestPrefetchingOnWeb",
             DISABLED);

// Features to provide head and tail non personalized search suggestion from
// compact on device models. More specifically, feature name with suffix
// Incognito / NonIncognito  will only controls behaviors under incognito /
// non-incognito mode respectively.
BASE_FEATURE(kOnDeviceHeadProviderIncognito,
             "OmniboxOnDeviceHeadProviderIncognito",
             ENABLED);
BASE_FEATURE(kOnDeviceHeadProviderNonIncognito,
             "OmniboxOnDeviceHeadProviderNonIncognito",
             ENABLED);
BASE_FEATURE(kOnDeviceHeadProviderKorean,
             "OmniboxOnDeviceHeadProviderKorean",
             DISABLED);
BASE_FEATURE(kOnDeviceTailModel, "OmniboxOnDeviceTailModel", DISABLED);
BASE_FEATURE(kOnDeviceTailEnableEnglishModel,
             "OmniboxOnDeviceTailEnableEnglishModel",
             ENABLED);

// If enabled, the relevant AutocompleteProviders will store "title" data in
// AutocompleteMatch::contents and "URL" data in AutocompleteMatch::description
// for URL-based omnibox suggestions (see crbug.com/1202964 for more details).
BASE_FEATURE(kStoreTitleInContentsAndUrlInDescription,
             "OmniboxStoreTitleInContentsAndUrlInDescription",
             DISABLED);

// Feature used to fetch document suggestions.
BASE_FEATURE(kDocumentProvider,
             "OmniboxDocumentProvider",
             enable_if(!IS_ANDROID && !IS_IOS));

// If enabled, the authentication requirement for Drive suggestions is based on
// whether the primary account is available, i.e., the user is signed into
// Chrome, rarther than checking if any signed in account is available in the
// cookie jar.
BASE_FEATURE(kDocumentProviderPrimaryAccountRequirement,
             "OmniboxDocumentProviderPrimaryAccountRequirement",
             DISABLED);

// If enabled, the primary account must be subject to enterprise policies in
// order to receive Drive suggestions.
BASE_FEATURE(kDocumentProviderEnterpriseEligibility,
             "OmniboxDocumentProviderEnterpriseEligibility",
             DISABLED);

// If enabled, the enterprise eligibility requirement for Drive suggestions
// is considered met even when the account capability is unknown. Has no effect
// if kDocumentProviderEnterpriseEligibility is disabled.
BASE_FEATURE(kDocumentProviderEnterpriseEligibilityWhenUnknown,
             "OmniboxDocumentProviderEnterpriseEligibilityWhenUnknown",
             DISABLED);

// If enabled, the requirement to be in an active Sync state is removed and
// Drive suggestions are available to all clients who meet the other
// requirements.
BASE_FEATURE(kDocumentProviderNoSyncRequirement,
             "OmniboxDocumentProviderNoSyncRequirement",
             DISABLED);

// Feature to determine if the HQP should double as a domain provider by
// suggesting up to the provider limit for each of the user's highly visited
// domains.
BASE_FEATURE(kDomainSuggestions, "OmniboxDomainSuggestions", DISABLED);

// If enabled, the omnibox popup is not presented until the mouse button is
// released.
BASE_FEATURE(kShowPopupOnMouseReleased,
             "OmniboxShowPopupOnMouseReleased",
             DISABLED);

// If enabled, makes Most Visited Tiles a Horizontal render group.
// Horizontal render group decomposes aggregate suggestions (such as old Most
// Visited Tiles), expecting individual AutocompleteMatch entry for every
// element in the carousel.
BASE_FEATURE(kMostVisitedTilesHorizontalRenderGroup,
             "OmniboxMostVisitedTilesHorizontalRenderGroup",
             enable_if(IS_ANDROID));

// If enabled, expands autocompletion to possibly (depending on params) include
// suggestion titles and non-prefixes as opposed to be restricted to URL
// prefixes. Will also adjust the location bar UI and omnibox text selection to
// accommodate the autocompletions.
BASE_FEATURE(kRichAutocompletion, "OmniboxRichAutocompletion", ENABLED);

// If enabled, shows the omnibox suggestions popup in WebUI.
BASE_FEATURE(kWebUIOmniboxPopup, "WebUIOmniboxPopup", DISABLED);

// When enabled, use Assistant for omnibox voice query recognition instead of
// Android's built-in voice recognition service. Only works on Android.
BASE_FEATURE(kOmniboxAssistantVoiceSearch,
             "OmniboxAssistantVoiceSearch",
             DISABLED);

// Feature used to default typed navigations to use HTTPS instead of HTTP.
// This only applies to navigations that don't have a scheme such as
// "example.com". Presently, typing "example.com" in a clean browsing profile
// loads http://example.com. When this feature is enabled, it should load
// https://example.com instead, with fallback to http://example.com if
// necessary.
// TODO(crbug.com/375004882): On non-iOS platforms, this feature is now
// superseded by HTTPS-Upgrades and will be removed in the near future.
BASE_FEATURE(kDefaultTypedNavigationsToHttps,
             "OmniboxDefaultTypedNavigationsToHttps",
             enable_if(IS_IOS));

// Override the delay to create a spare renderer when the omnibox is focused
// on Android.
BASE_FEATURE(kOverrideAndroidOmniboxSpareRendererDelay,
             "OverrideAndroidOmniboxSpareRendererDelay",
             DISABLED);

// Parameter name used to look up the delay before falling back to the HTTP URL
// while trying an HTTPS URL. The parameter is treated as a TimeDelta, so the
// unit must be included in the value as well (e.g. 3s for 3 seconds).
// - If the HTTPS load finishes successfully during this time, the timer is
//   cleared and no more work is done.
// - Otherwise, a new navigation to the the fallback HTTP URL is started.
const char kDefaultTypedNavigationsToHttpsTimeoutParam[] = "timeout";

// If enabled, logs Omnibox URL scoring signals to OmniboxEventProto for
// training the ML scoring models.
BASE_FEATURE(kLogUrlScoringSignals, "LogUrlScoringSignals", DISABLED);

// If true, enables history scoring signal annotator for populating history
// scoring signals associated with Search suggestions. These signals will be
// empty for Search suggestions otherwise.
BASE_FEATURE(kEnableHistoryScoringSignalsAnnotatorForSearches,
             "EnableHistoryScoringSignalsAnnotatorForSearches",
             DISABLED);

// If enabled, (floating-point) ML model scores are mapped to (integral)
// relevance scores by means of a piecewise function. This allows for the
// integration of URL model scores with search traditional scores.
BASE_FEATURE(kMlUrlPiecewiseMappedSearchBlending,
             "MlUrlPiecewiseMappedSearchBlending",
             DISABLED);

// If enabled, the ML scoring service will make use of an in-memory ML score
// cache in order to speed up the overall scoring process.
BASE_FEATURE(kMlUrlScoreCaching,
             "MlUrlScoreCaching",
             enable_if(!IS_ANDROID && !IS_IOS));

// If enabled, runs the ML scoring model to assign new relevance scores to the
// URL suggestions and reranks them.
BASE_FEATURE(kMlUrlScoring, "MlUrlScoring", enable_if(!IS_ANDROID && !IS_IOS));

// If enabled, specifies how URL model scores integrate with search traditional
// scores.
BASE_FEATURE(kMlUrlSearchBlending, "MlUrlSearchBlending", DISABLED);

// If enabled, creates Omnibox autocomplete URL scoring model. Prerequisite for
// `kMlUrlScoring` & `kMlUrlSearchBlending`.
BASE_FEATURE(kUrlScoringModel,
             "UrlScoringModel",
             enable_if(!IS_ANDROID && !IS_IOS));

BASE_FEATURE(kAnimateSuggestionsListAppearance,
             "AnimateSuggestionsListAppearance",
             DISABLED);

BASE_FEATURE(kOmniboxAnswerActions, "OmniboxAnswerActions", DISABLED);

// If enabled, treats categorical suggestions just like the entity suggestions
// by reusing the `ACMatchType::SEARCH_SUGGEST_ENTITY` and reports the original
// `omnibox::TYPE_CATEGORICAL_QUERY` to the server.
BASE_FEATURE(kCategoricalSuggestions, "CategoricalSuggestions", ENABLED);

// If enabled, merges the suggestion subtypes for the remote suggestions and the
// local verbatim and history suggestion duplicates at the provider level. This
// is needed for omnibox::kCategoricalSuggestions to function correctly but is
// being controlled by a separate feature in case there are unintended side
// effects beyond the categorical suggestions.
BASE_FEATURE(kMergeSubtypes, "MergeSubtypes", ENABLED);

// If enabled, sends a signal when a user touches down on a search suggestion to
// |SearchPrefetchService|. |SearchPrefetchService| will then prefetch
// suggestion iff the SearchNavigationPrefetch feature and "touch_down" param
// are enabled.
BASE_FEATURE(kOmniboxTouchDownTriggerForPrefetch,
             "OmniboxTouchDownTriggerForPrefetch",
             enable_if(IS_ANDROID));

// Enables additional site search providers for the Site search Starter Pack.
BASE_FEATURE(kStarterPackExpansion,
             "StarterPackExpansion",
             enable_if(!IS_ANDROID && !IS_IOS));

// Enables an informational IPH message at the bottom of the Omnibox directing
// users to certain starter pack engines.
BASE_FEATURE(kStarterPackIPH, "StarterPackIPH", DISABLED);

// If enabled, |SearchProvider| will not function in Zero Suggest.
BASE_FEATURE(kAblateSearchProviderWarmup,
             "AblateSearchProviderWarmup",
             DISABLED);

// If enabled, hl= is reported in search requests (applicable to iOS only).
BASE_FEATURE(kReportApplicationLanguageInSearchRequest,
             "ReportApplicationLanguageInSearchRequest",
             ENABLED);

// Enable asynchronous Omnibox/Suggest view inflation.
BASE_FEATURE(kOmniboxAsyncViewInflation, "OmniboxAsyncViewInflation", DISABLED);

// Use FusedLocationProvider on Android to fetch device location.
BASE_FEATURE(kUseFusedLocationProvider, "UseFusedLocationProvider", ENABLED);

// Enables storing successful query/match in the shortcut database On Android.
BASE_FEATURE(kOmniboxShortcutsAndroid, "OmniboxShortcutsAndroid", ENABLED);

// When enabled, it increases ipad's zps matches limit on web,srp and ntp.
BASE_FEATURE(kIpadZeroSuggestMatches,
             "IpadZeroSuggestMatches",
             enable_if(IS_IOS));

// Updates various NTP/Omnibox assets and descriptions for visual alignment on
// Android and iOS.
BASE_FEATURE(kOmniboxMobileParityUpdate, "OmniboxMobileParityUpdate", DISABLED);

// The features below allow tuning number of suggestions offered to users in
// specific contexts. These features are default enabled and are used to control
// related fieldtrial parameters.
BASE_FEATURE(kNumNtpZpsRecentSearches,
             "OmniboxNumNtpZpsRecentSearches",
             ENABLED);
BASE_FEATURE(kNumNtpZpsTrendingSearches,
             "OmniboxNumNtpZpsTrendingSearches",
             ENABLED);
BASE_FEATURE(kNumWebZpsRecentSearches,
             "OmniboxNumWebZpsRecentSearches",
             ENABLED);
BASE_FEATURE(kNumWebZpsRelatedSearches,
             "OmniboxNumWebZpsRelatedSearches",
             ENABLED);
BASE_FEATURE(kNumWebZpsMostVisitedUrls,
             "OmniboxNumWebZpsMostVisitedUrls",
             ENABLED);
BASE_FEATURE(kNumSrpZpsRecentSearches,
             "OmniboxNumSrpZpsRecentSearches",
             ENABLED);
BASE_FEATURE(kNumSrpZpsRelatedSearches,
             "OmniboxNumSrpZpsRelatedSearches",
             ENABLED);

#if BUILDFLAG(IS_ANDROID)
// Enable the Elegant Text Height attribute on the UrlBar.
// This attribute increases line height by up to 60% to accommodate certain
// scripts (e.g. Burmese).
BASE_FEATURE(kOmniboxElegantTextHeight, "OmniboxElegantTextHeight", ENABLED);

// Whether the contents of the omnibox should be retained on focus as opposed to
// being cleared. When this feature flag is enabled and the omnibox contents are
// retained, focus events will also result in the omnibox contents being fully
// selected so as to allow for easy replacement by the user. Note that even with
// this feature flag enabled, only large screen devices with an attached
// keyboard and precision pointer will exhibit a change in behavior.
BASE_FEATURE(kRetainOmniboxOnFocus, "RetainOmniboxOnFocus", ENABLED);

// Accelerates time from cold start to focused Omnibox on low-end devices,
// prioritizing Omnibox focus and background initialization.
BASE_FEATURE(kJumpStartOmnibox, "JumpStartOmnibox", DISABLED);

// Prevents intermediate AutocompleteResult updates from being sent to Java on
// low-end devices. This aims at eliminating time spent on constructing,
// measuring, and laying out views that are about to be discarded, and reducing
// the volume of JNI jumps.
BASE_FEATURE(kSuppressIntermediateACUpdatesOnLowEndDevices,
             "SuppressIntermediateACUpdatesOnLowEndDevices",
             DISABLED);

// (Android only) Show the search feature in the hub.
BASE_FEATURE(kAndroidHubSearch, "AndroidHubSearch", ENABLED);

// (Android only) Show tab groups via the search feature in the hub.
BASE_FEATURE(kAndroidHubSearchTabGroups, "AndroidHubSearchTabGroups", DISABLED);

// When enabled, delay focusTab to prioritize navigation
// (https://crbug.com/374852568).
BASE_FEATURE(kPostDelayedTaskFocusTab, "PostDelayedTaskFocusTab", ENABLED);

// Controls various Omnibox Diagnostics features.
BASE_FEATURE(kDiagnostics, "OmniboxDiagnostics", DISABLED);

namespace android {
static jlong JNI_OmniboxFeatureMap_GetNativeMap(JNIEnv* env) {
  static const base::Feature* const kFeaturesExposedToJava[] = {
      &kDiagnostics,
      &kOmniboxAnswerActions,
      &kAnimateSuggestionsListAppearance,
      &kOmniboxTouchDownTriggerForPrefetch,
      &kOmniboxAsyncViewInflation,
      &kRichAutocompletion,
      &kUseFusedLocationProvider,
      &kOmniboxElegantTextHeight,
      &kRetainOmniboxOnFocus,
      &kJumpStartOmnibox,
      &kAndroidHubSearch,
      &kAndroidHubSearchTabGroups,
      &kPostDelayedTaskFocusTab,
      &kOmniboxMobileParityUpdate};
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return reinterpret_cast<jlong>(kFeatureMap.get());
}
}  // namespace android
#endif  // BUILDFLAG(IS_ANDROID)

// If enabled, search aggregators defined by the
// EnterpriseSearchAggregatorSettings policy are saved into prefs and available
// in the TemplateURLService, so that they can be accessed from the Omnibox and
// the Settings page.
BASE_FEATURE(kEnableSearchAggregatorPolicy,
             "EnableSearchAggregatorPolicy",
             ENABLED);

// If enabled, site search engines, defined by the `SiteSearchSettings` policy,
// can be marked as user-overridable by administrators using an
// `allow_user_override` field. This setting is stored in preferences and
// determines if the engine can be overridden on the Settings page.
BASE_FEATURE(kEnableSiteSearchAllowUserOverridePolicy,
             "EnableSiteSearchAllowUserOverridePolicy",
             DISABLED);

}  // namespace omnibox
