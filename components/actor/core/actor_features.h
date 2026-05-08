// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_CORE_ACTOR_FEATURES_H_
#define COMPONENTS_ACTOR_CORE_ACTOR_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/page_content_annotations/core/page_content_annotations_enums.h"

namespace actor {

BASE_DECLARE_FEATURE(kGlicActionAllowlist);

BASE_DECLARE_FEATURE_PARAM(std::string, kAllowlist);
BASE_DECLARE_FEATURE_PARAM(std::string, kAllowlistExact);
BASE_DECLARE_FEATURE_PARAM(bool, kAllowlistOnly);

BASE_DECLARE_FEATURE(kGlicActionUseOptimizationGuide);
BASE_DECLARE_FEATURE(kActorBypassTOUValidationForGuestView);

BASE_DECLARE_FEATURE(kGlicExternalProtocolActionResultCode);
BASE_DECLARE_FEATURE(kGlicGranularBlockingActionResultCodes);

BASE_DECLARE_FEATURE(kGlicBlockNavigationToDangerousContentTypes);

BASE_DECLARE_FEATURE(kGlicBlockFileSystemAccessApiFilePicker);

BASE_DECLARE_FEATURE(kGlicDeferDownloadFilePickerToUserTakeover);

BASE_DECLARE_FEATURE(kGlicCrossOriginNavigationGating);
// Feature params to kGlicCrossOriginNavigationGating to enable individual
// checks for debugging.
// Toggles if we prompt users for navigation to sensitive sites (true) or we
// just fail the navigation (false).
BASE_DECLARE_FEATURE_PARAM(bool, kGlicPromptUserForSensitiveNavigations);
// Toggles confirming actor navigations to new origins.
BASE_DECLARE_FEATURE_PARAM(bool, kGlicConfirmNavigationToNewOrigins);
// Toggles displaying a user confirmation to confirm the navigation instead of
// relying on the web client making a server call.
// kGlicConfirmNavigationToNewOrigins must be enabled for this feature to work.
BASE_DECLARE_FEATURE_PARAM(bool, kGlicPromptUserForNavigationToNewOrigins);
// Toggles whether we are in "dark launch" mode where we ask the server for
// validation but only log the response.
// kGlicConfirmNavigationToNewOrigins must be enabled for this feature to work.
BASE_DECLARE_FEATURE_PARAM(bool, kGlicConfirmNavigationToNewOriginsDarkLaunch);
// Toggles whether novel origin gating is based on site (true) or origin
// (false). Note that gating sensitive sites will still be origin based.
BASE_DECLARE_FEATURE_PARAM(bool, kGlicNavigationGatingUseSiteNotOrigin);
// Controls whether the component updater provided blocklist should be enforced.
BASE_DECLARE_FEATURE_PARAM(bool, kGlicEnforceComponentUpdaterBlockListEntries);
// Controls whether tool requests can implicitly allow new origins.
BASE_DECLARE_FEATURE_PARAM(bool, kGlicAllowImplicitToolOriginGrants);

// Controls whether chrome records UMA metrics for navigations by sending the
// `NavigationConfirmationRequest` and recording the response.
BASE_DECLARE_FEATURE(kGlicRecordNavigationConfirmationRequestMetrics);

// When enabled, `beforeunload` dialog will not be displayed and the callback
// indicating the dialog outcome will be called with `true`.
// Warning: Enabling this feature can lead to data loss when navigating.
BASE_DECLARE_FEATURE(kGlicSkipBeforeUnloadDialogAndNavigate);

// Kill switch for skipping waiting for visual state update on new tabs.
BASE_DECLARE_FEATURE(kGlicSkipAwaitVisualStateForNewTabs);

// Enables the Paint Preview backend for taking screenshots.
BASE_DECLARE_FEATURE(kGlicTabScreenshotPaintPreviewBackend);

BASE_DECLARE_FEATURE(kGlicNavigateUsingLoadURL);

BASE_DECLARE_FEATURE(kGlicNavigateToolUseOpaqueInitiator);

BASE_DECLARE_FEATURE(kGlicNavigateWithoutUserGesture);

BASE_DECLARE_FEATURE(kGlicPerformActionsReturnsBeforeStateChange);

BASE_DECLARE_FEATURE(kGlicDeferActUntilUninterrupted);

// Kill switch for adding tabs to an ActorTask earlier during action handling.
BASE_DECLARE_FEATURE(kGlicEarlyAddTaskTabs);

// Enables a full page screenshot to be taken rather than only the viewport.
extern const base::FeatureParam<bool> kFullPageScreenshot;

// Controls the maximum memory/file bytes used for the capture of a single
// frame. 0 means no maximum.
extern const base::FeatureParam<size_t> kScreenshotMaxPerCaptureBytes;

// Controls whether iframe redaction is enabled, and which scope is used if so.
extern const base::FeatureParam<
    page_content_annotations::ScreenshotIframeRedactionScope>
    kScreenshotIframeRedaction;

// Kill switch for binding the created tab to the task that created it.
BASE_DECLARE_FEATURE(kActorBindCreatedTabToTask);

// When enabled, the actor will skip uploading screenshots when an actor turn
// is completed.
BASE_DECLARE_FEATURE(kGlicActorSkipScreenshot);

BASE_DECLARE_FEATURE(kActorRestartObservationDelayControllerOnNavigate);

// Kill switch to disable sending a browser signal (which is used for user
// interaction) before sending action to renderer.
BASE_DECLARE_FEATURE(kActorSendBrowserSignalForAction);

BASE_DECLARE_FEATURE(kGlicActorLoadAndExtractContentTool);
extern const base::FeatureParam<base::TimeDelta>
    kGlicActorLoadAndExtractContentToolTimeout;

BASE_DECLARE_FEATURE(kGlicActorTransientTasks);
extern const base::FeatureParam<bool> kGlicActorTransientTasksForceTransient;

BASE_DECLARE_FEATURE(kGlicActorEnableScriptTools);
extern const base::FeatureParam<base::TimeDelta>
    kActorScriptToolExecutionTimeout;
extern const base::FeatureParam<base::TimeDelta>
    kActorScriptToolCrossDocumentTimeout;

BASE_DECLARE_FEATURE(kActorScriptToolDelayObservation);
extern const base::FeatureParam<int> kActorScriptToolDelayObservationMillis;

// TODO(crbug.com/484367299): Implement a proper actor task state for
// interrupt-with-user-control.
BASE_DECLARE_FEATURE(kActorFormScriptToolInterrupt);

BASE_DECLARE_FEATURE(kGlicActorTabObservationController);

}  // namespace actor

#endif  // COMPONENTS_ACTOR_CORE_ACTOR_FEATURES_H_
