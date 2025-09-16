// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/features.h"

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

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

BASE_FEATURE(kUseCECFlagInPolicyData, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a configurable delay for policy registration.
BASE_FEATURE(kCustomPolicyRegistrationDelay, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kPolicyRegistrationDelay{
    &kCustomPolicyRegistrationDelay, "PolicyRegistrationDelay", base::Hours(6)};

}  // namespace policy::features
