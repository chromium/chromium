// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_features.h"

#include "base/feature_list.h"

namespace compose::features {

// Note: Compose is enabled by default because it is country--restricted at
// runtime.
BASE_FEATURE(kEnableCompose, "Compose", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kComposeInputParams,
             "ComposeInputParams",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeInnerText,
             "ComposeInnerText",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kComposeAXSnapshot,
             "ComposeAXSnapshot",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeAutoSubmit,
             "ComposeAutoSubmit",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeEligible,
             "ComposeEligible",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeSavedStateNudge,
             "ComposeNudge",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeProactiveNudge,
             "ComposeProactiveNudge",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeSavedStateNotification,
             "EnableComposeSavedStateNotification",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeNudgeAtCursor,
             "EnableComposeNudgeAtCursor",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeSelectionNudge,
             "EnableComposeSelectionNudge",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeLanguageBypassForContextMenu,
             "ComposeLanguageBypassForContextMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeWebUIAnimations,
             "ComposeWebUIAnimations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeOnDeviceDogfoodFooter,
             "EnableComposeOnDeviceDogfoodFooter",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeUiParams,
             "ComposeUiParams",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeTextOutputAnimation,
             "ComposeTextOutputAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeTextSelection,
             "ComposeTextSelection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kComposeRequestLatencyTimeout,
             "ComposeRequestLatencyTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableNudgeForUnspecifiedHint,
             "ComposeEnableNudgeForUnspecifiedHint",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableAdditionalTextMetrics,
             "EnableAdditionalTextMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHappinessTrackingSurveysForComposeAcceptance,
             "HappinessTrackingSurveysForComposeAcceptance",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHappinessTrackingSurveysForComposeClose,
             "HappinessTrackingSurveysForComposeClose",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHappinessTrackingSurveysForComposeNudgeClose,
             "HappinessTrackingSurveysForComposeNudgeClose",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeAllowOnDeviceExecution,
             "ComposeAllowOnDeviceExecution",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeUpfrontInputModes,
             "ComposeUpfrontInputModes",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace compose::features
