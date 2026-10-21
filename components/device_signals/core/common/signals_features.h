// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_FEATURES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_signals::features {

// Allows the reporting of client certificates for managed users.
BASE_DECLARE_FEATURE(kAllowClientCertificateReportingForUsers);

BASE_DECLARE_FEATURE(kBrowserSignalsReportingEnabled);
BASE_DECLARE_FEATURE(kDetectedAgentSignalCollectionEnabled);
BASE_DECLARE_FEATURE(kPolicyDataCollectionEnabled);
BASE_DECLARE_FEATURE(kCertificateCollectionEnabled);
BASE_DECLARE_FEATURE(kContentBindingVersioningEnabled);
BASE_DECLARE_FEATURE(kContentBindingVersioningEnabledIOS);

// Returns true if additional device signals reporting for browser-level Chrome
// reports has been enabled.
bool IsBrowserSignalsReportingEnabled();
// Returns true if detected agent signal collection has been
// enabled.
bool IsDetectedAgentSignalCollectionEnabled();
// Returns true if policy collection feature has been enabled.
bool IsPolicyDataCollectionEnabled();
// Returns true if certificate collection feature has been enabled.
bool IsCertificateCollectionEnabled();
// Returns true if content binding versioning feature has been enabled.
bool IsContentBindingVersioningEnabled();
// Returns true if content binding versioning feature has been enabled on iOS.
bool IsContentBindingVersioningEnabledIOS();

// Feature flag for supporting the new private SecureConnect functions in
// unaffiliated contexts via the consent flow.
BASE_DECLARE_FEATURE(kNewEvSignalsUnaffiliatedEnabled);

}  // namespace enterprise_signals::features

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_FEATURES_H_
