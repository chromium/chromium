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
// The minimum number of words needed for a valid user input to Compose.
extern const base::FeatureParam<int> kEnableComposeInputMinWords;
// The maximum number of words allowed for a valid user input to Compose.
extern const base::FeatureParam<int> kEnableComposeInputMaxWords;
// The maximum number of characters allowed for a valid user input to Compose.
extern const base::FeatureParam<int> kEnableComposeInputMaxChars;

// Controls whether or not the Nudge UI entrypoint is enabled for Compose.
BASE_DECLARE_FEATURE(kEnableComposeNudge);

// chrome:flags switch for the multi line fill feature.
BASE_DECLARE_FEATURE(kFillMultiLine);

// Controls whether the language check is bypassed for Compose.
BASE_DECLARE_FEATURE(kEnableComposeLanguageBypass);
}  // namespace compose::features

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_FEATURES_H_
