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

// Enables Auto-submit of compose with a valid selection.
BASE_DECLARE_FEATURE(kComposeAutoSubmit);

// Force compose off even if enabled by other switches..
BASE_DECLARE_FEATURE(kComposeEligible);

// Controls whether or not the Nudge UI entrypoint is enabled for Compose.
BASE_DECLARE_FEATURE(kEnableComposeNudge);

// Controls whether the language check is bypassed for Compose.
BASE_DECLARE_FEATURE(kEnableComposeLanguageBypass);

// Controls whether or not the Compose WebUI dialog has animations.
BASE_DECLARE_FEATURE(kEnableComposeWebUIAnimations);

// Controls whether to enable the dogfood footer when on device evaluation is
// used.
BASE_DECLARE_FEATURE(kEnableComposeOnDeviceDogfoodFooter);

// Controls whether or not the saved state notification is shown.
BASE_DECLARE_FEATURE(kEnableComposeSavedStateNotification);

// Controls parameters around UI rendering.
BASE_DECLARE_FEATURE(kComposeUiParams);

// Enables animation of text output. Applies only to on-device evaluation.
BASE_DECLARE_FEATURE(kComposeTextOutputAnimation);

// Constrols parameters around text selection and insert/replacement heuristics.
BASE_DECLARE_FEATURE(kComposeTextSelection);

}  // namespace compose::features

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_FEATURES_H_
