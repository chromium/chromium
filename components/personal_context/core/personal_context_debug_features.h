// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_DEBUG_FEATURES_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_DEBUG_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace personal_context::features::debug {

BASE_DECLARE_FEATURE(kPersonalContextForceEnablementState);
BASE_DECLARE_FEATURE_PARAM(int, kPersonalContextForceEnablementStateParam);

}  // namespace personal_context::features::debug

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_DEBUG_FEATURES_H_
