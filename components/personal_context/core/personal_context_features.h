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

// The URL of the Context Memory Service (CMS) endpoint used for fetching
// personal context data.
BASE_DECLARE_FEATURE_PARAM(std::string, kContextMemoryFetchContextEndpointUrl);

// TODO(crbug.com/403746095): Reuse this flag when the first_run logic is moved
// into c/personal_context. Controls whether the First Run and onboarding
// experience for Personal Context is enabled. When disabled, the service
// remains in a "Not Eligible" state because the required user acknowledgment
// or setup flow cannot be triggered.
BASE_DECLARE_FEATURE(kPersonalContextFirstRun);

}  // namespace personal_context::features

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FEATURES_H_
