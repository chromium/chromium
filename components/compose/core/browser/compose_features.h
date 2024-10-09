// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_FEATURES_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace compose::features {

// Controls whether the Compose feature is available
BASE_DECLARE_FEATURE(kEnableCompose);

// Has flags for tweaking the valid sizes of input.
BASE_DECLARE_FEATURE(kComposeInputParams);

// Enables/disables inner text context gathering.
BASE_DECLARE_FEATURE(kComposeInnerText);

// Enables/disables inner text context gathering.
BASE_DECLARE_FEATURE(kComposeAXSnapshot);

// Enables Auto-submit of compose with a valid selection.
BASE_DECLARE_FEATURE(kComposeAutoSubmit);

// Force compose off even if enabled by other switches..
BASE_DECLARE_FEATURE(kComposeEligible);

// Controls whether or not the saved state nudge is enabled.
BASE_DECLARE_FEATURE(kEnableComposeSavedStateNudge);

// Controls whether or not the proactive nudge is enabled.
BASE_DECLARE_FEATURE(kEnableComposeProactiveNudge);

// Controls whether or not the saved state notification is shown.
BASE_DECLARE_FEATURE(kEnableComposeSavedStateNotification);

// Controls whether or not the nudge displays at the cursor.
BASE_DECLARE_FEATURE(kEnableComposeNudgeAtCursor);

// Controls whether or not the nudge should be shown on text selection.
BASE_DECLARE_FEATURE(kEnableComposeSelectionNudge);

// Controls whether the language check is bypassed for the context menu option.
BASE_DECLARE_FEATURE(kEnableComposeLanguageBypassForContextMenu);

// Controls whether or not the Compose WebUI dialog has animations.
BASE_DECLARE_FEATURE(kEnableComposeWebUIAnimations);

// Controls whether to enable the dogfood footer when on device evaluation is
// used.
BASE_DECLARE_FEATURE(kEnableComposeOnDeviceDogfoodFooter);

// Controls parameters around UI rendering.
BASE_DECLARE_FEATURE(kComposeUiParams);

// Enables animation of text output. Applies only to on-device evaluation.
BASE_DECLARE_FEATURE(kComposeTextOutputAnimation);

// Constrols parameters around text selection and insert/replacement heuristics.
BASE_DECLARE_FEATURE(kComposeTextSelection);

// Enables client-side timeout of a Compose request.
BASE_DECLARE_FEATURE(kComposeRequestLatencyTimeout);

// Default nudge allow/deny decision for unspecified hint.
BASE_DECLARE_FEATURE(kEnableNudgeForUnspecifiedHint);

// A kill switch for additional metrics added to ComposeTextUsageLogger.
BASE_DECLARE_FEATURE(kEnableAdditionalTextMetrics);

// Enables or disables the Happiness Tracking System for Compose acceptance.
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForComposeAcceptance);

// Enables or disables the Happiness Tracking System for Compose on close.
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForComposeClose);

// Enables or disables the Happiness Tracking System for nudge dismissal.
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForComposeNudgeClose);

// Enables on-device execution, if available.
BASE_DECLARE_FEATURE(kComposeAllowOnDeviceExecution);

// Enables or disables upfront input modes in the dialog.
BASE_DECLARE_FEATURE(kComposeUpfrontInputModes);

}  // namespace compose::features

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_FEATURES_H_
