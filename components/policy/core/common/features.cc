// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/features.h"

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"

namespace policy::features {

BASE_FEATURE(kPolicyBlocklistProceedUntilResponse,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kProfileSeparationDomainExceptionListRetroactive,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnhancedSecurityEventFields,
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kUseCECFlagInPolicyData, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kInitializePoliciesForSignedInUserInNewEntryPoints,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables a configurable delay for policy registration.
BASE_FEATURE(kCustomPolicyRegistrationDelay, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kPolicyRegistrationDelay{
    &kCustomPolicyRegistrationDelay, "PolicyRegistrationDelay", base::Hours(6)};

// Used to add a captive portal check in SafeSitesNavigationThrottle.
BASE_FEATURE(kSafeSitesCaptivePortalCheck, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_DESKTOP_ANDROID)
// TODO(https://crbug.com/452666657): Remove this feature flag after launching
// policies to supported on Android Desktop.
BASE_FEATURE(kFuturePoliciesOnDesktopAndroid,
             base::FEATURE_DISABLED_BY_DEFAULT);

// A blocklist of policies to be blocked/ignored on Desktop Android.
BASE_FEATURE(kDesktopAndroidPolicy, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDesktopAndroidPolicyBlocklist{
    &kDesktopAndroidPolicy, "blocklist", ""};
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)

// Used to enable extension install policy support.
BASE_FEATURE(kEnableExtensionInstallPolicyFetching,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, uses ManagementService to determine whether to honor sensitive
// policies. When disabled, falls back to the original ShouldHonorPolicies()
// behavior.
BASE_FEATURE(kUseManagementServiceForSensitivePolicies,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace policy::features
