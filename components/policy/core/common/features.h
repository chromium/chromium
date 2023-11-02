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

// Enable force installed Chrome apps policy migration.
POLICY_EXPORT BASE_DECLARE_FEATURE(kDefaultChromeAppsMigration);

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

// Enable MetricsReportingEnabled policy to alter MetricsReportingState on
// Android.
POLICY_EXPORT BASE_DECLARE_FEATURE(
    kActivateMetricsReportingEnabledPolicyAndroid);

// Causes the DMToken to be deleted (rather than invalidated) when a browser is
// deleted from CBCM.
POLICY_EXPORT BASE_DECLARE_FEATURE(kDmTokenDeletion);

}  // namespace features
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_FEATURES_H_
