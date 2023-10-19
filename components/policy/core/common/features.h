// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
#define COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace features {

// Enable the policy test page at chrome://policy/test.
POLICY_EXPORT BASE_DECLARE_FEATURE(kEnablePolicyTestPage);

#if BUILDFLAG(IS_ANDROID)
// Enable comma-separated strings for list policies on Android.
// Enabled by default, to be used as a kill switch.
POLICY_EXPORT BASE_DECLARE_FEATURE(
    kListPoliciesAcceptCommaSeparatedStringsAndroid);

// Enable SafeSitesFilterBehavior policy on Android.
POLICY_EXPORT BASE_DECLARE_FEATURE(kSafeSitesFilterBehaviorPolicyAndroid);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
