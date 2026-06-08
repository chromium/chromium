// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace contextual_tasks {

BASE_DECLARE_FEATURE(kContextualTasks);
BASE_DECLARE_FEATURE(kContextualTasksExtraOauthScopes);
BASE_DECLARE_FEATURE(kEnableContextualTasksPinButtonInToolbar);
BASE_DECLARE_FEATURE(kContextualTasksContext);
BASE_DECLARE_FEATURE(
    kContextualTasksContextSmartTabSharingDefaultOnAvailability);
BASE_DECLARE_FEATURE(kContextualTasksContextLibrary);
BASE_DECLARE_FEATURE(kContextualTasksContextLogging);
BASE_DECLARE_FEATURE(kContextualTasksShowOnboardingTooltip);

// Enables prefetching of cookies for contextual tasks.
BASE_DECLARE_FEATURE(kContextualTasksCookiePrefetch);

// Overrides the value of EntryPointEligibilitymanager::IsEligible to true.
BASE_DECLARE_FEATURE(kContextualTasksForceEntryPointEligibility);

// Enables context menu settings for contextual tasks.
BASE_DECLARE_FEATURE(kContextualTasksContextMenu);

// Enables context menu settings for contextual tasks.
BASE_DECLARE_FEATURE(kContextualTasksSuggestionsEnabled);

// Force the application locale to US and the gl query parameter to us.
BASE_DECLARE_FEATURE(kContextualTasksForceCountryCodeUS);

// Remove tasks that have no tabs or threads associated with them on tab
// disassociation.
BASE_DECLARE_FEATURE(
    kContextualTasksRemoveTasksWithoutThreadsOrTabAssociations);

// Enables use of silk api to notify zero state rendered instead of the url
// param.
BASE_DECLARE_FEATURE(kEnableNotifyZeroStateRenderedCapability);

// If enabled, adds the Sec-CH-UA-Full-Version-List header to all network
// requests initiated from within an embedded Co-Browse <webview>.
BASE_DECLARE_FEATURE(kContextualTasksSendFullVersionListEnabled);

// If enabled, AIM will send the ContextualInputUploadType enum on
// ContextualInputs.
BASE_DECLARE_FEATURE(kContextualTasksSendContextualInputUploadType);

// When contextual tasks is disabled and this flag is enabled, intecept the
// contextual tasks URL and redirect to aim URL.
BASE_DECLARE_FEATURE(kContextualTasksUrlRedirectToAimUrl);

// Enables the use of Stratus dark mode colors.
BASE_DECLARE_FEATURE(kContextualTasksUseStratusDarkModeColors);

// If enabled, animates the caret.
BASE_DECLARE_FEATURE(kContextualTasksAnimatedCaret);

// Enables energy effect in Nextbox.
BASE_DECLARE_FEATURE(kEnergyEffectInNextbox);

// Fixes the composebox jump.
BASE_DECLARE_FEATURE(kContextualTasksComposeboxJumpFix);

// Enables the use of a rounded clip-path for the composebox.
BASE_DECLARE_FEATURE(kContextualTasksRoundedClipPath);

// Hides the the 3-dot (overflow) menu when viewing an ai page in the side
// panel. The menu is still shown for lens flows.
BASE_DECLARE_FEATURE(kContextualTasksHideMenuOnAiPage);

// Enables hiding the close button when in vertical tabs or immersive mode.
BASE_DECLARE_FEATURE(kContextualTasksHideCloseButtonInVerticalTabs);

// Enables intercepting YouTube links with timestamps to seek video instead of
// navigating.
BASE_DECLARE_FEATURE(kContextualTasksVideoCitations);

// Enables intercepting PDF links with page numbers to scroll to page instead of
// navigating.
BASE_DECLARE_FEATURE(kContextualTasksPdfCitations);

// When enabled, the back button can expand the side panel.
BASE_DECLARE_FEATURE(kContextualTasksBackButtonExpandsSidePanel);

// Enables lazy fetching of cluster info for multimodal queries.
BASE_DECLARE_FEATURE(kContextualTasksLazyFetchClusterInfo);

// Enables the use of APC comparison for webpages in the recontextualization
// flow.
BASE_DECLARE_FEATURE(kContextualTasksWebpageApcComparison);

// Enables the Java implementation of the Contextual Tasks Fusebox. Android
// only.
BASE_DECLARE_FEATURE(kContextualTasksJavaFusebox);

// Enables overriding side panel to show Bottom Sheet on demand.
BASE_DECLARE_FEATURE(kContextualTasksOverrideShowBottomSheetOnLargeScreen);

// When enabled, AIM must send the browser a message to initiate the cobrowse
// experience for link clicks.
BASE_DECLARE_FEATURE(kAimTriggeredThreadLinks);
bool GetIsContextualTasksPdfCitationsEnabled();

bool GetIsContextualTasksLazyFetchClusterInfoEnabled();

// Enum denoting which entry point can show when enabled.
enum class EntryPointOption {
  kNoEntryPoint,
  kToolbarEphemeralBranded,
};

// Enum of expand button UI option
enum class ExpandButtonOption {
  kSidePanelExpandButton,
  kToolbarCloseButton,
};

// Whether to only consider titles for similarity.
extern const base::FeatureParam<bool> kOnlyUseTitlesForSimilarity;
// Whether to deduplicate relevant tabs by URL.
extern const base::FeatureParam<bool> kDeduplicateRelevantTabsByUrl;
// Minimum score to consider a tab relevant.
extern const base::FeatureParam<double> kTabSelectionScoreThreshold;
// Minimum score required for a tab to be considered visible.
extern const base::FeatureParam<double> kContentVisibilityThreshold;

// Whether to use the immediately previous visited tab as the active tab signal
// fallback.
extern const base::FeatureParam<bool> kEnablePreviousTabFallback;
// Recency threshold for using the previous visited tab as active tab signal.
extern const base::FeatureParam<base::TimeDelta> kPreviousTabRecencyThreshold;

// Whether Smart Tab Sharing is enabled for the ContextualTasksContext feature.
extern const base::FeatureParam<bool> kContextualTasksContextSmartTabSharing;

// Option for smart tab sharing IPH first time prompt.
enum class SmartTabSharingIphFirstTimePromptOption {
  kIphFirstTimePromptV1,
  kIphFirstTimePromptV2,
};
extern const base::FeatureParam<SmartTabSharingIphFirstTimePromptOption>
    kSmartTabSharingIphFirstTimePromptOption;

// Option for smart tab sharing IPH default on variants.
enum class SmartTabSharingIphDefaultOnOption {
  kIphDefaultOnV1,
  kIphDefaultOnV2,
};
extern const base::FeatureParam<SmartTabSharingIphDefaultOnOption>
    kSmartTabSharingIphDefaultOnOption;

// Option for smart tab sharing IPH try it promo variants.
enum class SmartTabSharingIphTryItPromoOption {
  kIphTryItPromoV1,
  kIphTryItPromoV2,
};
extern const base::FeatureParam<SmartTabSharingIphTryItPromoOption>
    kSmartTabSharingIphTryItPromoOption;

// Option for smart tab sharing megaplus string.
enum class SmartTabSharingMegaplusStringOption {
  kMegaplusV1,
  kMegaplusV2,
  kMegaplusV3,
};
extern const base::FeatureParam<SmartTabSharingMegaplusStringOption>
    kSmartTabSharingMegaplusStringOption;

// Task string to use for formatting the query embedding.
extern const base::FeatureParam<std::string> kQueryEmbeddingTask;

// The sample rate for logging contextual tasks context quality.
extern const base::FeatureParam<double>
    kContextualTasksContextLoggingSampleRate;

// Controls whether we set the upload type in CreateSearchUrl.
extern const base::FeatureParam<bool> kSendContextualInputUploadTypeInSearchUrl;

// Controls whether we set the upload type in CreateClientToAimRequest.
extern const base::FeatureParam<bool>
    kSendContextualInputUploadTypeInAimRequest;

// Controls whether the contextual task page action should show
extern const base::FeatureParam<EntryPointOption, true> kShowEntryPoint;

// UI Options to expand the contextual tasks side panel to tab.
extern const base::FeatureParam<ExpandButtonOption, true> kExpandButtonOptions;

// Whether the context menu is enabled for Nextbox.
extern bool GetIsContextualTasksNextboxContextMenuEnabled();

// The file types that can be attached to a Nextbox as images.
extern const base::FeatureParam<std::string>
    kContextualTasksNextboxImageFileTypes;

// The file types that can be attached to a Nextbox as attachments.
extern const base::FeatureParam<std::string>
    kContextualTasksNextboxAttachmentFileTypes;

// The maximum size of a file that can be attached to a Nextbox.
extern const base::FeatureParam<int> kContextualTasksNextboxMaxFileSize;

// The user agent suffix to use for requests from the contextual tasks UI.
extern const base::FeatureParam<std::string> kContextualTasksUserAgentSuffix;

// Extra OAuth scopes separated by commas for contextual tasks.
extern const base::FeatureParam<std::string> kContextualTasksOAuthScopes;

// The URL for the help center article from the toolbar.
extern const base::FeatureParam<std::string> kContextualTasksHelpUrl;

// The URL for the help center article linked from the onboarding tooltip.
extern const base::FeatureParam<std::string>
    kContextualTasksOnboardingTooltipHelpUrl;

// Enables suggestions rendered on contextual tasks side, instead of from AIM
// webpage.
extern const base::FeatureParam<bool>
    kContextualTasksEnableNativeZeroStateSuggestions;

// The maximum number of times the onboarding tooltip can be shown to the user
// in a single session before it no longer shows up.
extern int GetContextualTasksShowOnboardingTooltipSessionImpressionCap();

// The maximum number of times the onboarding tooltip can be dismissed by the
// user before it no longer shows up.
extern int GetContextualTasksOnboardingTooltipDismissedCap();

// The delay in milliseconds before the onboarding tooltip is considered shown.
extern int GetContextualTasksOnboardingTooltipImpressionDelay();

// The number of seconds inactive side panel WebContents should keep in cache.
// Expired side panel WebContents will be destroyed.
extern int ContextualTasksInactiveSidePanelKeepInCacheMinutes();


// Returns if the protected page error is enabled.
extern bool GetIsProtectedPageErrorEnabled();

// Returns if the ghost loader is enabled.
extern bool GetIsGhostLoaderEnabled();

// Returns if basic mode should be forced when the thread history is opened
// before the handshake is complete.
extern bool ShouldForceBasicModeIfOpeningThreadHistory();

// Returns the base URL for the AI page.
extern std::string GetContextualTasksAiPageUrl();

// Returns the base URL for a Gemini thread.
extern std::string GetContextualTasksGeminiBaseUrl();

// Returns scheme component of the "display url" associated with the contextual
// tasks page.
extern std::string GetContextualTasksDisplayUrlScheme();

// Returns host component of the "display url" associated with the contextual
// tasks page.
extern std::string GetContextualTasksDisplayUrlHost();

// Returns path component of the "display url" associated with the contextual
// tasks page.
extern std::string GetContextualTasksDisplayUrlPath();

// Returns whether to show the expanded security chip in the location bar for
// the contextual tasks page.
extern bool ShouldShowExpandedSecurityChip();

// Returns the host that all URLs loaded in the embedded page in the Contextual
// Tasks WebUi should be routed to.
extern std::string GetForcedEmbeddedPageHost();

// Allows overriding the embedded page host at runtime for debugging.
extern void SetForcedEmbeddedPageHostOverride(const std::string& host);

// Returns the domains for the sign in page.
extern std::vector<std::string> GetContextualTasksSignInDomains();

// Whether the suggestions are enabled for Nextbox.
extern bool GetIsContextualTasksSuggestionsEnabled();

// Returns the timeout for smart tab sharing tab selection.
extern base::TimeDelta GetSmartTabSharingTabSelectionTimeout();

// Returns the score threshold required to display the smart tab sharing promo.
extern double GetSmartTabSharingPromoScoreThreshold();

// Enables tab auto-chip for contextual tasks. When disabled, no suggested
// chips will be shown in the composebox automatically.
extern bool GetIsTabAutoSuggestionChipEnabled();

// Returns whether Lens is enabled in contextual tasks. When this is enabled,
// Lens entry points will open results in the contextual tasks panels.
extern bool GetEnableLensInContextualTasks();

// Returns whether we should force the gsc=2 param to be added. This is used as
// a temporary workaround since the server is not yet ready to adapt the side
// panel UI unless the gsc=2 param is set.
extern bool ShouldForceGscInTabMode();

// Returns whether the country code should be forced to US.
extern bool ShouldForceCountryCodeUS();

// Returns the user agent suffix to use for requests.
extern std::string GetContextualTasksUserAgentSuffix();

// Returns the URL parameter name to check for NLM mode.
extern std::string GetContextualTasksNlmUrlParam();
extern bool IsCustomNlmUiEnabled();

// Whether the contextual tasks context quality should be logged.
extern bool ShouldLogContextualTasksContextQuality();

// Returns the help URL for the onboarding tooltip.
extern std::string GetContextualTasksOnboardingTooltipHelpUrl();

// Returns the help URL for the help center article from the toolbar.
extern std::string GetContextualTasksHelpUrl();

// Returns whether smart compose is enabled for Contextual Tasks.
extern bool GetEnableContextualTasksSmartCompose();

// Returns whether native (cobrowsing instead of AIM webpage)
// zero state suggestions are enabled for Contextual Tasks.
extern bool GetEnableNativeZeroStateSuggestions();


// Returns whether basic mode should be enabled.
extern bool GetIsBasicModeEnabled();

// Returns whether the z-order of the composebox should be changed in basic
// mode.
extern bool ShouldEnableBasicModeZOrder();

// Returns whether the cookie sync should be enabled.
extern bool ShouldEnableCookieSync();

// Returns whether the cookie prefetch should be enabled.
extern bool ShouldEnableCookiePrefetch();

// Returns whether the input plate can be locked and unlocked by a message
// from AIM.
extern bool ShouldEnableLockAndUnlockInputCapability();

// Returns whether the Stratus dark mode colors should be used.
extern bool ShouldUseStratusDarkModeColors();

// Returns whether the file hint is enabled in the composebox.
extern bool GetEnableFileHint();

// Returns whether the composebox jump fix is enabled.
extern bool GetEnableComposeboxJumpFix();

// Returns the UI option to expand contextual tasks side panel to tab.
extern ExpandButtonOption GetExpandButtonOption();

// Returns whether the rounded clip-path is enabled.
extern bool IsRoundedClipPathEnabled();

// Returns whether the pin button in toolbar is enabled.
extern bool IsContextualTasksPinButtonInToolbarEnabled();

// Returns whether the webpage APC comparison is enabled.
extern bool GetIsWebpageApcComparisonEnabled();

namespace flag_descriptions {

extern const char kContextualTasksName[];
extern const char kContextualTasksDescription[];
extern const char kContextualTasksContextLibraryName[];
extern const char kContextualTasksContextLibraryDescription[];
extern const char kContextualTasksContextName[];
extern const char kContextualTasksContextDescription[];
extern const char kContextualTasksSuggestionsEnabledName[];
extern const char kContextualTasksSuggestionsEnabledDescription[];
extern const char kContextualTasksJavaFuseboxName[];
extern const char kContextualTasksJavaFuseboxDescription[];
extern const char kContextualTasksBackButtonExpandsSidePanelName[];
extern const char kContextualTasksBackButtonExpandsSidePanelDescription[];
extern const char kContextualTasksOverrideShowBottomSheetOnLargeScreenName[];
extern const char
    kContextualTasksOverrideShowBottomSheetOnLargeScreenDescription[];
extern const char kContextualTasksCookiePrefetchName[];
extern const char kContextualTasksCookiePrefetchDescription[];

}  // namespace flag_descriptions

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_
