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
// When enabled, it should instead request the kSearchResultsOAuth2Scope instead
// of the kChromeSyncOAuth2Scope
BASE_DECLARE_FEATURE(kContextualTasksScopeChange);
BASE_DECLARE_FEATURE(kContextualTasksContext);
BASE_DECLARE_FEATURE(kContextualTasksContextLibrary);
BASE_DECLARE_FEATURE(kContextualTasksContextLogging);
BASE_DECLARE_FEATURE(kContextualTasksShowOnboardingTooltip);

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

// Replace the overflow menu in the side panel with an explicit button to move
// the thread to a new tab.
BASE_DECLARE_FEATURE(kContextualTasksExpandButton);

// If enabled, adds the Sec-CH-UA-Full-Version-List header to all network
// requests initiated from within an embedded Co-Browse <webview>.
BASE_DECLARE_FEATURE(kContextualTasksSendFullVersionListEnabled);

// When contextual tasks is disabled and this flag is enabled, intecept the
// contextual tasks URL and redirect to aim URL.
BASE_DECLARE_FEATURE(kContextualTasksUrlRedirectToAimUrl);

// Enum denoting which entry point can show when enabled.
enum class EntryPointOption {
  kNoEntryPoint,
  kPageActionRevisit,
  kToolbarRevisit,
  kToolbarPermanent
};

// Enum of expand button UI option
enum class ExpandButtonOption {
  kSidePanelExpandButton,
  kToolbarCloseButton,
};

// Whether to only consider titles for similarity.
extern const base::FeatureParam<bool> kOnlyUseTitlesForSimilarity;
// Minimum score to consider a tab relevant.
extern const base::FeatureParam<double> kTabSelectionScoreThreshold;
// Minimum score required for a tab to be considered visible.
extern const base::FeatureParam<double> kContentVisibilityThreshold;

// The sample rate for logging contextual tasks context quality.
extern const base::FeatureParam<double>
    kContextualTasksContextLoggingSampleRate;

// Controls whether the contextual task page action should show
extern const base::FeatureParam<EntryPointOption, true> kShowEntryPoint;

// UI Options to expand the contextual tasks side panel to tab.
extern const base::FeatureParam<ExpandButtonOption, true> kExpandButtonOptions;

// If true, the side panel is task scoped. Meaning that for all tabs associated
// with the same task, they will share the same side panel. If the side panel
// changed to another task for one tab, all tabs associated with the former task
// will become associated with the new task. When set to false, task change in
// the side panel only affects the current tab.
extern const base::FeatureParam<bool> kTaskScopedSidePanel;

// Whether to open side panel when an external link is clicked on the contextual
// task page.
extern const base::FeatureParam<bool> kOpenSidePanelOnLinkClicked;

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

// The maximum number of files that can be attached to a Nextbox.
extern const base::FeatureParam<int> kContextualTasksNextboxMaxFileCount;

// The user agent suffix to use for requests from the contextual tasks UI.
extern const base::FeatureParam<std::string> kContextualTasksUserAgentSuffix;

// The URL for the help center article from the toolbar.
extern const base::FeatureParam<std::string> kContextualTasksHelpUrl;

// The URL for the help center article linked from the onboarding tooltip.
extern const base::FeatureParam<std::string>
    kContextualTasksOnboardingTooltipHelpUrl;

// Enables suggestions rendered on contextual tasks side, instead of from AIM
// webpage.
extern const base::FeatureParam<bool>
    kContextualTasksEnableNativeZeroStateSuggestions;

// The scheme component of the "display url" associated with the contextual
// tasks page.
extern const base::FeatureParam<std::string> kContextualTasksDisplayUrlScheme;

// The host component of the "display url" associated with the contextual tasks
// page.
extern const base::FeatureParam<std::string> kContextualTasksDisplayUrlHost;

// The path component of the "display url" associated with the contextual tasks
// page.
// NOTE: The value of this feature param must start with a forward slash to
// align with GURL path semantics (e.g. "/search" is OK, while "search" is not).
extern const base::FeatureParam<std::string> kContextualTasksDisplayUrlPath;

// Whether to show the expanded security chip in the location bar for the
// contextual tasks page.
extern const base::FeatureParam<bool> kContextualTasksShowExpandedSecurityChip;

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

// Returns if voice search is allowed in expanded composebox.
extern bool GetIsExpandedComposeboxVoiceSearchEnabled();

// Returns if voice search is allowed in base steady composebox.
extern bool GetIsSteadyComposeboxVoiceSearchEnabled();

// Returns if voice search queries should be auto submitted.
extern bool GetAutoSubmitVoiceSearchQuery();

// Returns if the protected page error is enabled.
extern bool GetIsProtectedPageErrorEnabled();

// Returns if the ghost loader is enabled.
extern bool GetIsGhostLoaderEnabled();

// Returns if basic mode should be forced when the thread history is opened
// before the handshake is complete.
extern bool ShouldForceBasicModeIfOpeningThreadHistory();

// Returns the base URL for the AI page.
extern std::string GetContextualTasksAiPageUrl();

// Returns the host that all URLs loaded in the embedded page in the Contextual
// Tasks WebUi should be routed to.
extern std::string GetForcedEmbeddedPageHost();

// Returns the domains for the sign in page.
extern std::vector<std::string> GetContextualTasksSignInDomains();

// Whether the suggestions are enabled for Nextbox.
extern bool GetIsContextualTasksSuggestionsEnabled();

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

// Returns whether the kSearchResultsOAuth2Scope should be used instead of the
// kChromeSyncOAuth2Scope.
extern bool ShouldUseSearchResultsScope();

// Returns whether basic mode should be enabled.
extern bool GetIsBasicModeEnabled();

// Returns whether the z-order of the composebox should be changed in basic mode.
extern bool ShouldEnableBasicModeZOrder();

// Returns whether the cookie sync should be enabled.
extern bool ShouldEnableCookieSync();

// Returns whether the input plate can be locked and unlocked by a message
// from AIM.
extern bool ShouldEnableLockAndUnlockInputCapability();

// Returns the UI option to expand contextual tasks side panel to tab.
extern ExpandButtonOption GetExpandButtonOption();

namespace flag_descriptions {

extern const char kContextualTasksName[];
extern const char kContextualTasksDescription[];
extern const char kContextualTasksContextLibraryName[];
extern const char kContextualTasksContextLibraryDescription[];
extern const char kContextualTasksContextName[];
extern const char kContextualTasksContextDescription[];
extern const char kContextualTasksExpandButtonName[];
extern const char kContextualTasksExpandButtonDescription[];
extern const char kContextualTasksSuggestionsEnabledName[];
extern const char kContextualTasksSuggestionsEnabledDescription[];

}  // namespace flag_descriptions

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_
