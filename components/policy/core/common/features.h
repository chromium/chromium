// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
#define COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "components/policy/policy_export.h"

namespace policy::features {

// Enable the PolicyBlocklistThrottle optimization to hide the DEFER latency
// on WillStartRequest and WillRedirectRequest. See https://crbug.com/349964973.
POLICY_EXPORT BASE_DECLARE_FEATURE(kPolicyBlocklistProceedUntilResponse);

// Enables the fact that the ProfileSeparationDomainExceptionList retroactively
// signs out accounts that require a new profile. This is used as a kill switch.
POLICY_EXPORT BASE_DECLARE_FEATURE(
    kProfileSeparationDomainExceptionListRetroactive);

}  // namespace policy::features

#endif  // COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
