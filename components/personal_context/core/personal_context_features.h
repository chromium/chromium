// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FEATURES_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace personal_context::features {

// The main feature flag for the Personal Context service. When disabled,
// all Personal Context features and services are turned off. Enabled by default
// and kept around as a kill-switch.
BASE_DECLARE_FEATURE(kPersonalContext);

// Controls whether the opt-in flow for the first run experience is enabled.
BASE_DECLARE_FEATURE(kPersonalContextFirstRunOptIn);

// Returns true if the opt-in flow for the first run experience is enabled.
bool IsPersonalContextFirstRunOptInEnabled();

// Controls whether the Personal Context non-eligibility UMA metric is logged.
BASE_DECLARE_FEATURE(kPersonalContextLogNonEligibilityUma);

}  // namespace personal_context::features

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FEATURES_H_
