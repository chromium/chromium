// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_features.h"

#include "base/feature_list.h"

namespace compose::features {

// Note: Compose is enabled by default because it is country--restricted at
// runtime.
BASE_FEATURE(kEnableCompose, "Compose", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kComposeInputParams, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeInnerText, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kComposeAXSnapshot, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeAutoSubmit, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeEligible, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeSavedStateNudge,
             "ComposeNudge",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeProactiveNudge,
             "ComposeProactiveNudge",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeSavedStateNotification,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeNudgeAtCursor, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeSelectionNudge, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeLanguageBypassForContextMenu,
             "ComposeLanguageBypassForContextMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeWebUIAnimations,
             "ComposeWebUIAnimations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeOnDeviceDogfoodFooter,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeUiParams, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeTextOutputAnimation, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeRequestLatencyTimeout, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableNudgeForUnspecifiedHint,
             "ComposeEnableNudgeForUnspecifiedHint",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableAdditionalTextMetrics, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHappinessTrackingSurveysForComposeAcceptance,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHappinessTrackingSurveysForComposeClose,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHappinessTrackingSurveysForComposeNudgeClose,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeAllowOnDeviceExecution, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeUpfrontInputModes, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace compose::features
