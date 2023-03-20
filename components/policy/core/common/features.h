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

// Enable reporting Login events to the reporting connector when the Password
// Manager detects that the user logged in to a web page.
POLICY_EXPORT BASE_DECLARE_FEATURE(kLoginEventReporting);

// Enable reporting password leaks to the reporting connector when the Password
// Manager's Leak Detector has found some compromised credentials.
POLICY_EXPORT BASE_DECLARE_FEATURE(kPasswordBreachEventReporting);

// Enable the UserCloudSigninRestrictionPolicyFetcher to get the
// ManagedAccountsSigninRestriction policy for a dasher account.
POLICY_EXPORT BASE_DECLARE_FEATURE(
    kEnableUserCloudSigninRestrictionPolicyFetcher);

// Causes the DMToken to be deleted (rather than invalidated) when a browser is
// deleted from CBCM.
POLICY_EXPORT BASE_DECLARE_FEATURE(kDmTokenDeletion);

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

// Prevent policies set by a single source from being treated as merged.
POLICY_EXPORT BASE_DECLARE_FEATURE(kPolicyMergeMultiSource);

#if BUILDFLAG(IS_IOS)
// Enable logging and chrome://policy/logs page on IOS.
POLICY_EXPORT BASE_DECLARE_FEATURE(kPolicyLogsPageIOS);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace features
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
