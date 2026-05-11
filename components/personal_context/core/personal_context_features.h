// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FEATURES_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace personal_context::features {

BASE_DECLARE_FEATURE(kPersonalContext);
BASE_DECLARE_FEATURE_PARAM(std::string, kPersonalContextEligibleTiers);

// TODO(crbug.com/403746095): Reuse this flag when the first_run logic is moved
// into c/personal_context.
BASE_DECLARE_FEATURE(kPersonalContextFirstRun);

}  // namespace personal_context::features

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FEATURES_H_
