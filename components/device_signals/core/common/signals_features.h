// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_FEATURES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace enterprise_signals::features {

// Allows the reporting of client certificates for managed users.
BASE_DECLARE_FEATURE(kAllowClientCertificateReportingForUsers);

BASE_DECLARE_FEATURE(kProfileSignalsReportingEnabled);
BASE_DECLARE_FEATURE(kBrowserSignalsReportingEnabled);
BASE_DECLARE_FEATURE(kDetectedAgentSignalCollectionEnabled);
BASE_DECLARE_FEATURE(kSystemSignalCollectionImprovementEnabled);
BASE_DECLARE_FEATURE(kPolicyDataCollectionEnabled);

// Signals reporting related feature parameters.
extern const base::FeatureParam<bool> kTriggerOnCookieChange;
extern const base::FeatureParam<base::TimeDelta>
    kProfileSignalsReportingInterval;

// Returns true if additional device signals reporting for profile-level Chrome
// reports has been enabled.
bool IsProfileSignalsReportingEnabled();
// Returns true if additional device signals reporting for browser-level Chrome
// reports has been enabled.
bool IsBrowserSignalsReportingEnabled();
// Returns true if detected agent signal collection has been
// enabled.
bool IsDetectedAgentSignalCollectionEnabled();
// Returns true if system signal collection improvement feature has been
// enabled.
bool IsSystemSignalCollectionImprovementEnabled();
// Returns true if policy collection feature has been enabled.
bool IsPolicyDataCollectionEnabled();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kDeviceSignalsConsentDialog);

// Returns true if device signals consent dialog has been enabled for
// consent collection.
bool IsConsentDialogEnabled();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

// Feature flag for supporting the new private SecureConnect functions in
// unaffiliated contexts via the consent flow.
BASE_DECLARE_FEATURE(kNewEvSignalsUnaffiliatedEnabled);

}  // namespace enterprise_signals::features

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_FEATURES_H_
