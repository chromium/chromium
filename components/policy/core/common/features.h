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

// Enable detection/upload Crowdstrike Agent signals with security
// events.
POLICY_EXPORT BASE_DECLARE_FEATURE(kCrowdstrikeSignalReporting);

// Enable the UserCloudSigninRestrictionPolicyFetcher to get the
// ManagedAccountsSigninRestriction policy for a dasher account.
POLICY_EXPORT
BASE_DECLARE_FEATURE(kEnableUserCloudSigninRestrictionPolicyFetcher);

// Enable the policy test page at chrome://policy/test.
POLICY_EXPORT BASE_DECLARE_FEATURE(kEnablePolicyTestPage);

#if BUILDFLAG(IS_ANDROID)
// Enable comma-separated strings for list policies on Android.
// Enabled by default, to be used as a kill switch.
POLICY_EXPORT BASE_DECLARE_FEATURE(
    kListPoliciesAcceptCommaSeparatedStringsAndroid);

// Enable logging and chrome://policy/logs page on Android.
POLICY_EXPORT BASE_DECLARE_FEATURE(kPolicyLogsPageAndroid);

// Enable SafeSitesFilterBehavior policy on Android.
POLICY_EXPORT BASE_DECLARE_FEATURE(kSafeSitesFilterBehaviorPolicyAndroid);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
// Enable logging and chrome://policy/logs page on IOS.
POLICY_EXPORT BASE_DECLARE_FEATURE(kPolicyLogsPageIOS);
#endif  // BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_IOS) || !BUILDFLAG(IS_ANDROID)
POLICY_EXPORT BASE_DECLARE_FEATURE(kPolicyLogsPageDesktop);
#endif  // !BUILDFLAG(IS_IOS) || !!BUILDFLAG(IS_ANDROID)

}  // namespace features
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
