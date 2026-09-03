// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_debug_features.h"

#include "base/feature_list.h"
#include "components/personal_context/core/personal_context_features.h"

namespace personal_context::features::debug {

// When set, overrides the context memory service url.
BASE_FEATURE_PARAM(std::string,
                   kContextMemoryServiceBaseUrlParam,
                   &kPersonalContext,
                   "");

// When enabled, overrides the calculated enablement state of the
// Personal Context service. This allows developers to bypass complex
// eligibility requirements (Geo-IP, Account Type, Opt-ins) for local testing.
BASE_FEATURE(kPersonalContextForceEnablementState,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kPersonalContextForceEnablementStateParam,
                   &kPersonalContextForceEnablementState,
                   "state",
                   2);

BASE_FEATURE(kMockPersonalContextResult, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kMockPersonalContextResultTypeParam,
                   &kMockPersonalContextResult,
                   2);

BASE_FEATURE(kPersonalContextResetNoticePrefsOnStartup,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, all ambient autofill eligibility checks will be overridden and
// return true. Used for development purposes.
BASE_FEATURE(kAutofillAmbientAutofillSkipEligibilityChecks,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace personal_context::features::debug
