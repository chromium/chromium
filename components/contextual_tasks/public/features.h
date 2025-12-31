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
BASE_DECLARE_FEATURE(kContextualTasksContext);
BASE_DECLARE_FEATURE(kContextualTasksContextLibrary);
BASE_DECLARE_FEATURE(kContextualTasksContextLogging);
BASE_DECLARE_FEATURE(kContextualTasksShowOnboardingTooltip);

// Enables context menu settings for contextual tasks.
BASE_DECLARE_FEATURE(kContextualTasksContextMenu);

// Enables context menu settings for contextual tasks.
BASE_DECLARE_FEATURE(kContextualTasksSuggestionsEnabled);

// Force the application locale to US and the gl query parameter to us.
BASE_DECLARE_FEATURE(kContextualTasksForceCountryCodeUS);

// Force the context id migration to be enabled.
extern const base::FeatureParam<bool> kForceContextIdMigration;

// Enum denoting which entry point can show when enabled.
enum class EntryPointOption {
  kNoEntryPoint,
  kPageActionRevisit,
  kToolbarRevisit,
  kToolbarPermanent
};

// The minimum score required for two embeddings to be considered similar.
extern const base::FeatureParam<double> kMinEmbeddingSimilarityScore;
// Whether to only consider titles for similarity.
extern const base::FeatureParam<bool> kOnlyUseTitlesForSimilarity;
// Minimum score, computed using multiple signals, to consider a tab relevant.
extern const base::FeatureParam<double> kMinMultiSignalScore;
// Minimum score required for a tab to be considered visible.
extern const base::FeatureParam<double> kContentVisibilityThreshold;

// The sample rate for logging contextual tasks context quality.
extern const base::FeatureParam<double>
    kContextualTasksContextLoggingSampleRate;

// Controls whether the contextual task page action should show
extern const base::FeatureParam<EntryPointOption, true> kShowEntryPoint;

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
extern const base::FeatureParam<std::string> kContextualTasksNextboxImageFileTypes;

// The file types that can be attached to a Nextbox as attachments.
extern const base::FeatureParam<std::string> kContextualTasksNextboxAttachmentFileTypes;

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

// The maximum number of times the onboarding tooltip can be shown to the user
// in a single session before it no longer shows up.
extern int GetContextualTasksShowOnboardingTooltipSessionImpressionCap();

// The maximum number of times the onboarding tooltip can be dismissed by the
// user before it no longer shows up.
extern int GetContextualTasksOnboardingTooltipDismissedCap();

// Returns if voice search is allowed in expanded composebox.
extern bool GetIsExpandedComposeboxVoiceSearchEnabled();

// Returns if voice search is allowed in base steady composebox.
extern bool GetIsSteadyComposeboxVoiceSearchEnabled();

// Returns the base URL for the AI page.
extern std::string GetContextualTasksAiPageUrl();

// Returns the host that all URLs loaded in the embedded page in the Contextual
// Tasks WebUi should be routed to.
extern std::string GetForcedEmbeddedPageHost();

// Returns the domains for the sign in page.
extern std::vector<std::string> GetContextualTasksSignInDomains();

// Whether the suggestions are enabled for Nextbox.
extern bool GetIsContextualTasksSuggestionsEnabled();

// Returns whether Lens is enabled in contextual tasks. When this is enabled,
// Lens entry points will open results in the contextual tasks panels.
extern bool GetEnableLensInContextualTasks();

// Returns whether we should force the gsc=2 param to be added. This is used as
// a temporary workaround since the server is not yet ready to adapt the side
// panel UI unless the gsc=2 param is set.
extern bool ShouldForceGscInTabMode();

// Returns whether the country code should be forced to US.
extern bool ShouldForceCountryCodeUS();

// Returns whether the context id migration should be forced.
extern bool ShouldForceContextIdMigration();

// Returns the user agent suffix to use for requests.
extern std::string GetContextualTasksUserAgentSuffix();

// Whether the contextual tasks context quality should be logged.
extern bool ShouldLogContextualTasksContextQuality();

// Returns the help URL for the onboarding tooltip.
extern std::string GetContextualTasksOnboardingTooltipHelpUrl();

// Returns the help URL for the help center article from the toolbar.
extern std::string GetContextualTasksHelpUrl();

namespace flag_descriptions {

extern const char kContextualTasksName[];
extern const char kContextualTasksDescription[];
extern const char kContextualTasksContextLibraryName[];
extern const char kContextualTasksContextLibraryDescription[];
extern const char kContextualTasksContextName[];
extern const char kContextualTasksContextDescription[];
extern const char kContextualTasksSuggestionsEnabledName[];
extern const char kContextualTasksSuggestionsEnabledDescription[];

}  // namespace flag_descriptions

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_
