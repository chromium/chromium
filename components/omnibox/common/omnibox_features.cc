// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/omnibox_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

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
BASE_FEATURE(kImageSearchSuggestionThumbnail, enable_if(IS_ANDROID));

// Feature used to allow users to remove suggestions from clipboard.
BASE_FEATURE(kOmniboxRemoveSuggestionsFromClipboard, enable_if(IS_ANDROID));

// When enabled, uses the grouping framework with prefixed suggestions (i.e.
// autocomplete_grouper_sections.h) to limit and group (but not sort) matches.
BASE_FEATURE(kGroupingFrameworkForNonZPS,
             "OmniboxGroupingFrameworkForNonZPS",
             enable_if(IS_ANDROID));

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

// Feature used to cap max suggestions to a dynamic limit based on how many URLs
// would be shown. E.g., show up to 10 suggestions if doing so would display no
// URLs; else show up to 8 suggestions if doing so would include 1 or more URLs.
BASE_FEATURE(kDynamicMaxAutocomplete,
             "OmniboxDynamicMaxAutocomplete",
             enable_if(!IS_IOS));

// Enables omnibox focus as a trigger for zero-prefix suggestions on web and
// SRP, subject to the same requirements and conditions as on-clobber
// suggestions.
BASE_FEATURE(kFocusTriggersWebAndSRPZeroSuggest,
             "OmniboxFocusTriggersWebAndSRPZeroSuggest",
             ENABLED);

// If enabled, suggestion group headers in the Omnibox popup will be hidden
// (e.g. in order to minimize visual clutter in the zero-prefix state).
BASE_FEATURE(kHideSuggestionGroupHeaders,
             "OmniboxHideSuggestionGroupHeaders",
             ENABLED);

// Enables local history zero-prefix suggestions in every context in which the
// remote zero-prefix suggestions are enabled.
BASE_FEATURE(kLocalHistoryZeroSuggestBeyondNTP, DISABLED);

// If enabled, zero prefix suggestions will be stored using an in-memory caching
// service, instead of using the existing prefs-based cache.
BASE_FEATURE(kZeroSuggestInMemoryCaching, DISABLED);

// Enables the use of a request debouncer to throttle the number of ZPS prefetch
// requests initiated over a given period of time (to help minimize the
// performance impact of ZPS prefetching on the remote Suggest service).
BASE_FEATURE(kZeroSuggestPrefetchDebouncing, DISABLED);

// Enables prefetching of the zero prefix suggestions for eligible users on NTP.
BASE_FEATURE(kZeroSuggestPrefetching, ENABLED);

// Enables prefetching of the zero prefix suggestions for eligible users on SRP.
BASE_FEATURE(kZeroSuggestPrefetchingOnSRP, enable_if(!IS_ANDROID));

// Enables prefetching of the zero prefix suggestions for eligible users on the
// Web (i.e. non-NTP and non-SRP URLs).
BASE_FEATURE(kZeroSuggestPrefetchingOnWeb, DISABLED);

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
BASE_FEATURE(kOnDeviceTailModel, "OmniboxOnDeviceTailModel", DISABLED);
BASE_FEATURE(kOnDeviceTailEnableEnglishModel,
             "OmniboxOnDeviceTailEnableEnglishModel",
             ENABLED);

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
             ENABLED);

// If enabled, the primary account must be subject to enterprise policies in
// order to receive Drive suggestions.
BASE_FEATURE(kDocumentProviderEnterpriseEligibility,
             "OmniboxDocumentProviderEnterpriseEligibility",
             ENABLED);

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
             ENABLED);

// If enabled, the omnibox popup is not presented until the mouse button is
// released.
BASE_FEATURE(kShowPopupOnMouseReleased,
             "OmniboxShowPopupOnMouseReleased",
             ENABLED);

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

// When enabled, the multimodal input button is shown in the Omnibox.
BASE_FEATURE(kOmniboxMultimodalInput, DISABLED);

// Whether the AI Mode entrypoint is shown in the Omnibox as a RHS button. Only
// used on desktop platforms.
// The first feature enables the entrypoint for all users. The second feature
// enables the entrypoint only for users who have their locale set to English
// and are located in the US, and has no effect if the first feature is
// enabled.
BASE_FEATURE(kAiModeOmniboxEntryPoint, DISABLED);
BASE_FEATURE(kAiModeOmniboxEntryPointEnUs, ENABLED);

// Hides the AIM entrypoint in the Omnibox when user input is in progress. Only
// used on desktop platforms.
BASE_FEATURE(kHideAimEntrypointOnUserInput,
             "OmniboxHideAimEntrypointOnUserInput",
             DISABLED);


// When enabled, removes the Search Ready Omnibox feature.
BASE_FEATURE(kRemoveSearchReadyOmnibox, DISABLED);

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
BASE_FEATURE(kOverrideAndroidOmniboxSpareRendererDelay, DISABLED);

// Parameter name used to look up the delay before falling back to the HTTP URL
// while trying an HTTPS URL. The parameter is treated as a TimeDelta, so the
// unit must be included in the value as well (e.g. 3s for 3 seconds).
// - If the HTTPS load finishes successfully during this time, the timer is
//   cleared and no more work is done.
// - Otherwise, a new navigation to the the fallback HTTP URL is started.
const char kDefaultTypedNavigationsToHttpsTimeoutParam[] = "timeout";

// If enabled, logs Omnibox URL scoring signals to OmniboxEventProto for
// training the ML scoring models.
BASE_FEATURE(kLogUrlScoringSignals, DISABLED);

// If true, enables history scoring signal annotator for populating history
// scoring signals associated with Search suggestions. These signals will be
// empty for Search suggestions otherwise.
BASE_FEATURE(kEnableHistoryScoringSignalsAnnotatorForSearches, DISABLED);

// If enabled, (floating-point) ML model scores are mapped to (integral)
// relevance scores by means of a piecewise function. This allows for the
// integration of URL model scores with search traditional scores.
BASE_FEATURE(kMlUrlPiecewiseMappedSearchBlending, DISABLED);

// If enabled, the ML scoring service will make use of an in-memory ML score
// cache in order to speed up the overall scoring process.
BASE_FEATURE(kMlUrlScoreCaching, enable_if(!IS_ANDROID));

// If enabled, runs the ML scoring model to assign new relevance scores to the
// URL suggestions and reranks them.
BASE_FEATURE(kMlUrlScoring, enable_if(!IS_ANDROID));

// If enabled, specifies how URL model scores integrate with search traditional
// scores.
BASE_FEATURE(kMlUrlSearchBlending, DISABLED);

// If enabled, creates Omnibox autocomplete URL scoring model. Prerequisite for
// `kMlUrlScoring` & `kMlUrlSearchBlending`.
BASE_FEATURE(kUrlScoringModel, enable_if(!IS_ANDROID));

BASE_FEATURE(kAnimateSuggestionsListAppearance, ENABLED);

// If enabled, sends a signal when a user touches down on a search suggestion to
// |SearchPrefetchService|. |SearchPrefetchService| will then prefetch
// suggestion iff the SearchNavigationPrefetch feature and "touch_down" param
// are enabled.
BASE_FEATURE(kOmniboxTouchDownTriggerForPrefetch, enable_if(IS_ANDROID));

// Enables keyword-based site search functionality on Android devices.
BASE_FEATURE(kOmniboxSiteSearch, DISABLED);

// Enables additional site search providers for the Site search Starter Pack.
BASE_FEATURE(kStarterPackExpansion, enable_if(!IS_ANDROID && !IS_IOS));

// Enables an informational IPH message at the bottom of the Omnibox directing
// users to certain starter pack engines.
BASE_FEATURE(kStarterPackIPH, DISABLED);

// Enables an '@aimode' starter pack keyword for eligible users only.
BASE_FEATURE(kAiModeStartPack, DISABLED);

// If enabled, |SearchProvider| will not function in Zero Suggest.
BASE_FEATURE(kAblateSearchProviderWarmup, DISABLED);

// If enabled, hl= is reported in search requests (applicable to iOS only).
BASE_FEATURE(kReportApplicationLanguageInSearchRequest, ENABLED);

// Enable asynchronous Omnibox/Suggest view inflation.
BASE_FEATURE(kOmniboxAsyncViewInflation, DISABLED);

// Use FusedLocationProvider on Android to fetch device location.
BASE_FEATURE(kUseFusedLocationProvider, ENABLED);

// Enables storing successful query/match in the shortcut database On Android.
BASE_FEATURE(kOmniboxShortcutsAndroid, ENABLED);

// Updates various NTP/Omnibox assets and descriptions for visual alignment on
// iOS.
BASE_FEATURE(kOmniboxMobileParityUpdate, ENABLED);

// Updates various NTP/Omnibox assets and descriptions for visual alignment on
// Android and iOS, V2.
BASE_FEATURE(kOmniboxMobileParityUpdateV2, ENABLED);

#if BUILDFLAG(IS_IOS)
// Updates the search engine logo on NTP. iOS only.
BASE_FEATURE(kOmniboxMobileParityUpdateV3, DISABLED);
#endif  // BUILDFLAG(IS_IOS)

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

// If enabled, search aggregators defined by the
// EnterpriseSearchAggregatorSettings policy are saved into prefs and available
// in the TemplateURLService, so that they can be accessed from the Omnibox and
// the Settings page.
BASE_FEATURE(kEnableSearchAggregatorPolicy, ENABLED);

BASE_FEATURE(kUseAgentspace25Logo, ENABLED);

// If enabled, site search engines, defined by the `SiteSearchSettings` policy,
// can be marked as user-overridable by administrators using an
// `allow_user_override` field. This setting is stored in preferences and
// determines if the engine can be overridden on the Settings page.
BASE_FEATURE(kEnableSiteSearchAllowUserOverridePolicy, ENABLED);

// Enables preconnecting to omnibox suggestions that are not only Search types.
BASE_FEATURE(kPreconnectNonSearchOmniboxSuggestions, DISABLED);

// Enabls adding an aim shortcut in the typed state.
BASE_FEATURE(kOmniboxAimShortcutTypedState, DISABLED);

// When enabled, unblocks omnibox height on small form factor devices, allowing
// users to type in multiline / longer text.
BASE_FEATURE(kMultilineEditField, "OmniboxMultilineEditField", DISABLED);

// Controls whether the composebox
BASE_FEATURE(kComposeboxUsesChromeComposeClient, ENABLED);

// Controls whether or not contextual composebox should display suggestions.
BASE_FEATURE(kComposeboxAttachmentsTypedState, DISABLED);

#if BUILDFLAG(IS_ANDROID)
// Accelerates time from cold start to focused Omnibox on low-end devices,
// prioritizing Omnibox focus and background initialization.
BASE_FEATURE(kJumpStartOmnibox, DISABLED);

// Prevents intermediate AutocompleteResult updates from being sent to Java on
// low-end devices. This aims at eliminating time spent on constructing,
// measuring, and laying out views that are about to be discarded, and reducing
// the volume of JNI jumps.
BASE_FEATURE(kSuppressIntermediateACUpdatesOnLowEndDevices, DISABLED);

// (Android only) Show tab groups via the search feature in the hub.
BASE_FEATURE(kAndroidHubSearchTabGroups, DISABLED);

// When enabled, delay focusTab to prioritize navigation
// (https://crbug.com/374852568).
BASE_FEATURE(kPostDelayedTaskFocusTab, ENABLED);

// Controls various Omnibox Diagnostics features.
BASE_FEATURE(kDiagnostics, "OmniboxDiagnostics", DISABLED);

// When enabled, offer a desktop-like omnibox UI enhancement on large form
// factors.
BASE_FEATURE(kOmniboxImprovementForLFF, DISABLED);

// If enabled, disables ligatures in the URL bar on Android.
BASE_FEATURE(kUrlBarWithoutLigatures, ENABLED);

namespace android {
static jlong JNI_OmniboxFeatureMap_GetNativeMap(JNIEnv* env) {
  static const base::Feature* const kFeaturesExposedToJava[] = {
      &kDiagnostics,
      &kAnimateSuggestionsListAppearance,
      &kOmniboxTouchDownTriggerForPrefetch,
      &kOmniboxAsyncViewInflation,
      &kRichAutocompletion,
      &kUrlBarWithoutLigatures,
      &kUseFusedLocationProvider,
      &kJumpStartOmnibox,
      &kAndroidHubSearchTabGroups,
      &kPostDelayedTaskFocusTab,
      &kOmniboxMobileParityUpdateV2,
      &kOmniboxSiteSearch,
      &kOmniboxAimShortcutTypedState,
      &kOmniboxMultimodalInput,
      &kMultilineEditField,
      &kOmniboxImprovementForLFF,
      &kRemoveSearchReadyOmnibox};
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return reinterpret_cast<jlong>(kFeatureMap.get());
}
}  // namespace android
#endif  // BUILDFLAG(IS_ANDROID)
// Note: no new flags beyond this point.
}  // namespace omnibox

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(OmniboxFeatureMap)
#endif
