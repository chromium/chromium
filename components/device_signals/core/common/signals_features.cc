// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/signals_features.h"

namespace enterprise_signals::features {

BASE_FEATURE(kAllowClientCertificateReportingForUsers,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the collection of detected agent signals in Chrome report.
BASE_FEATURE(kDetectedAgentSignalCollectionEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the addition of device signals fields to Browser-level Chrome
// Reports.
BASE_FEATURE(kBrowserSignalsReportingEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the collection of policies in a Chrome Profile signals report.
BASE_FEATURE(kPolicyDataCollectionEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the collection of certificates in a Chrome Profile signals report.
BASE_FEATURE(kCertificateCollectionEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the versioned content bindings in signal reports.
BASE_FEATURE(kContentBindingVersioningEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the versioned content bindings in signal reports on iOS.
BASE_FEATURE(kContentBindingVersioningEnabledIOS,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBrowserSignalsReportingEnabled() {
  return base::FeatureList::IsEnabled(kBrowserSignalsReportingEnabled);
}

bool IsDetectedAgentSignalCollectionEnabled() {
  return base::FeatureList::IsEnabled(kDetectedAgentSignalCollectionEnabled);
}

bool IsPolicyDataCollectionEnabled() {
  return base::FeatureList::IsEnabled(kPolicyDataCollectionEnabled);
}

bool IsCertificateCollectionEnabled() {
  return base::FeatureList::IsEnabled(kCertificateCollectionEnabled);
}

bool IsContentBindingVersioningEnabled() {
  return base::FeatureList::IsEnabled(kContentBindingVersioningEnabled);
}

bool IsContentBindingVersioningEnabledIOS() {
  return base::FeatureList::IsEnabled(kContentBindingVersioningEnabledIOS);
}

BASE_FEATURE(kNewEvSignalsUnaffiliatedEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace enterprise_signals::features
