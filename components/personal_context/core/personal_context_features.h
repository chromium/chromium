// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FEATURES_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace personal_context::features {

// The main feature flag for the Personal Context service. When disabled,
// all Personal Context features and services are turned off.
BASE_DECLARE_FEATURE(kPersonalContext);

// Determines whether the `FetchContext` API is allowed to execute network
// requests.
BASE_DECLARE_FEATURE_PARAM(bool, kPersonalContextEnableFetchContext);

// Comma-separated list of AI subscription tiers that are eligible to use
// Personal Context features (e.g., "1,2").
BASE_DECLARE_FEATURE_PARAM(std::string, kPersonalContextEligibleTiers);

// The base URL of the Context Memory Service (CMS) endpoint.
BASE_DECLARE_FEATURE_PARAM(std::string, kContextMemoryServiceBaseUrl);

// Controls whether the First Run and onboarding experience for Personal Context
// is enabled. When disabled, the service remains in a "Not Eligible" state
// because the required user acknowledgment or setup flow cannot be triggered.
BASE_DECLARE_FEATURE(kPersonalContextFirstRun);

// Controls whether the further evolution of the notice UI for the first run
// experience is enabled.
BASE_DECLARE_FEATURE(kPersonalContextFirstRunNoticePhase2);

// Controls whether the opt-in flow for the first run experience is enabled.
BASE_DECLARE_FEATURE(kPersonalContextFirstRunOptIn);

// Returns true if the first run experience or any of its phases/extensions are
// enabled.
bool IsPersonalContextFirstRunEnabled();

// Returns true if the notice phase 2 of the first run experience is enabled.
bool IsPersonalContextFirstRunNoticePhase2Enabled();

// Returns true if the opt-in flow for the first run experience is enabled.
bool IsPersonalContextFirstRunOptInEnabled();

}  // namespace personal_context::features

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FEATURES_H_
