// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
#define COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace features {

// Feature that controls whether the browser registers for FCM invalidations for
// Machine Level Policies. If enabled, |kCBCMServiceAccounts| must also be
// enabled.
POLICY_EXPORT extern const base::Feature kCBCMPolicyInvalidations;

// Feature that controls if remote commands are enabled in CBCM. If enabled,
// the browser will register for remote commands FCM invalidations, and fetch
// remote commands when fetching policies.
POLICY_EXPORT extern const base::Feature kCBCMRemoteCommands;

// PolicyBlocklistThrottle defers navigations until policies are loaded.
POLICY_EXPORT extern const base::Feature
    kPolicyBlocklistThrottleRequiresPoliciesLoaded;

// Max time to defer the navigation while waiting for policies to load.
POLICY_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kPolicyBlocklistThrottlePolicyLoadTimeout;

// Update browser device identifier during enrollment and fetching policies.
POLICY_EXPORT extern const base::Feature kUploadBrowserDeviceIdentifier;

}  // namespace features
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
