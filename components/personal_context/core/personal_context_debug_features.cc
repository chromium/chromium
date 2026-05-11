// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_debug_features.h"

#include "base/feature_list.h"

namespace personal_context::features::debug {

// When enabled, overrides the calculated enablement state of the
// Personal Context service. This allows developers to bypass complex
// eligibility requirements (Geo-IP, Account Type, Opt-ins) for local testing.
BASE_FEATURE(kPersonalContextForceEnablementState,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kPersonalContextForceEnablementStateParam,
                   &kPersonalContextForceEnablementState,
                   "state",
                   0);

}  // namespace personal_context::features::debug
