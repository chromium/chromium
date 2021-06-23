// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/features.h"

namespace policy {

namespace features {

const base::Feature kCBCMPolicyInvalidations{"CBCMPolicyInvalidations",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCBCMRemoteCommands{"CBCMRemoteCommands",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPolicyBlocklistThrottleRequiresPoliciesLoaded{
    "PolicyBlocklistThrottleRequiresPoliciesLoaded",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<base::TimeDelta>
    kPolicyBlocklistThrottlePolicyLoadTimeout{
        &kPolicyBlocklistThrottleRequiresPoliciesLoaded,
        "PolicyBlocklistThrottlePolicyLoadTimeout",
        base::TimeDelta::FromSeconds(20)};

const base::Feature kUploadBrowserDeviceIdentifier{
    "UploadBrowserDeviceIdentifier", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCRDForManagedUserSessions{
    "CRDForManagedUserSessions", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

}  // namespace policy
