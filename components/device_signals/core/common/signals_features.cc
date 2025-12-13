// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/signals_features.h"

namespace enterprise_signals::features {

BASE_FEATURE(kAllowClientCertificateReportingForUsers,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the addition of device signals fields to Profile-level Chrome
// Reports.
BASE_FEATURE(kProfileSignalsReportingEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the collection of detected agent signals in Chrome report.
BASE_FEATURE(kDetectedAgentSignalCollectionEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the addition of device signals fields to Browser-level Chrome
// Reports.
BASE_FEATURE(kBrowserSignalsReportingEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the improvements made during system signals collection in Chrome.
BASE_FEATURE(kSystemSignalCollectionImprovementEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the collection of policies in a Chrome Profile signals report.
BASE_FEATURE(kPolicyDataCollectionEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether a signals-only profile report will be triggered when a valid
// cookie change is observed.
constexpr base::FeatureParam<bool> kTriggerOnCookieChange{
    &kProfileSignalsReportingEnabled, "trigger_on_cookie_change", true};

// Controls the minimum interval that signals should be reported via profile
// reports.
// Example: "ProfileSignalsReportingEnabled:report_interval/3600" for 3600
// seconds.
constexpr base::FeatureParam<base::TimeDelta> kProfileSignalsReportingInterval{
    &kProfileSignalsReportingEnabled, "report_interval", base::Hours(4)};

bool IsProfileSignalsReportingEnabled() {
  return base::FeatureList::IsEnabled(kProfileSignalsReportingEnabled);
}

bool IsBrowserSignalsReportingEnabled() {
  return base::FeatureList::IsEnabled(kBrowserSignalsReportingEnabled);
}

bool IsDetectedAgentSignalCollectionEnabled() {
  return base::FeatureList::IsEnabled(kDetectedAgentSignalCollectionEnabled);
}

bool IsSystemSignalCollectionImprovementEnabled() {
  return base::FeatureList::IsEnabled(
      kSystemSignalCollectionImprovementEnabled);
}

bool IsPolicyDataCollectionEnabled() {
  return base::FeatureList::IsEnabled(kPolicyDataCollectionEnabled);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
// Enables the triggering of device signals consent dialog when conditions met
// This feature also requires UnmanagedDeviceSignalsConsentFlowEnabled policy to
// be enabled
BASE_FEATURE(kDeviceSignalsConsentDialog, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsConsentDialogEnabled() {
  return base::FeatureList::IsEnabled(kDeviceSignalsConsentDialog);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

BASE_FEATURE(kNewEvSignalsUnaffiliatedEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace enterprise_signals::features
