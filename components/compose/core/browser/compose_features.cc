// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_features.h"

namespace compose::features {

// Note: Compose is enabled by default because it requires settings UI enabling.
BASE_FEATURE(kEnableCompose, "Compose", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kComposeInputParams,
             "ComposeInputParams",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeInnerText,
             "ComposeInnerText",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kComposeAutoSubmit,
             "ComposeAutoSubmit",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeEligible,
             "ComposeEligible",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeNudge,
             "ComposeNudge",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeLanguageBypass,
             "ComposeLanguageBypass",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeWebUIAnimations,
             "ComposeWebUIAnimations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeOnDeviceDogfoodFooter,
             "EnableComposeOnDeviceDogfoodFooter",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeSavedStateNotification,
             "EnableComposeSavedStateNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kComposeUiParams,
             "ComposeUiParams",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace compose::features
