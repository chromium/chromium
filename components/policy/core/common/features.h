// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
#define COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/policy/policy_export.h"

namespace policy::features {

// Enable the PolicyBlocklistThrottle optimization to hide the DEFER latency
// on WillStartRequest and WillRedirectRequest. See https://crbug.com/349964973.
// This is launched, but the feature flag will be kept in 2025 for monitoring.
POLICY_EXPORT BASE_DECLARE_FEATURE(kPolicyBlocklistProceedUntilResponse);

// Enables the fact that the ProfileSeparationDomainExceptionList retroactively
// signs out accounts that require a new profile. This is used as a kill switch.
POLICY_EXPORT BASE_DECLARE_FEATURE(
    kProfileSeparationDomainExceptionListRetroactive);

// Enables the addition of new security fields for SecOps.
POLICY_EXPORT BASE_DECLARE_FEATURE(kEnhancedSecurityEventFields);

// Controls if we can use the cec flag in PolicyData.
POLICY_EXPORT BASE_DECLARE_FEATURE(kUseCECFlagInPolicyData);

#if BUILDFLAG(IS_ANDROID)
// Enables policy initialization for signed-in users in new entry points.
POLICY_EXPORT BASE_DECLARE_FEATURE(
    kInitializePoliciesForSignedInUserInNewEntryPoints);
#endif

// Enables a configurable delay for policy registration.
POLICY_EXPORT BASE_DECLARE_FEATURE(kCustomPolicyRegistrationDelay);
POLICY_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kPolicyRegistrationDelay;

// Used to enable future_on policies on Desktop Android.
POLICY_EXPORT BASE_DECLARE_FEATURE(kFuturePoliciesOnDesktopAndroid);

// Used to add a captive portal check in SafeSitesNavigationThrottle.
POLICY_EXPORT BASE_DECLARE_FEATURE(kSafeSitesCaptivePortalCheck);

// Used to enable extension install policy support.
POLICY_EXPORT BASE_DECLARE_FEATURE(kEnableExtensionInstallPolicyFetching);

}  // namespace policy::features

#endif  // COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
