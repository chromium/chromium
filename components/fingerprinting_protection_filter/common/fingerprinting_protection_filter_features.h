// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_FINGERPRINTING_PROTECTION_FILTER_FEATURES_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_FINGERPRINTING_PROTECTION_FILTER_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace subresource_filter::mojom {
enum class ActivationLevel;
}  // namespace subresource_filter::mojom

namespace fingerprinting_protection_filter::features {

const char kEnableConsoleLoggingParam[] = "enable_console_logging";
const char kExperimentVersionParam[] = "experimental_version";
const char kPerformanceMeasurementRateParam[] = "performance_measurement_rate";
const char kRefreshHeuristicExceptionThresholdParam[] =
    "refresh_heuristic_exception_threshold";

// The primary toggle to enable/disable the Fingerprinting Protection Filter.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
BASE_DECLARE_FEATURE(kEnableFingerprintingProtectionFilter);

// Toggle to enable/disable the Fingerprinting Protection Filter in Incognito.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
BASE_DECLARE_FEATURE(kEnableFingerprintingProtectionFilterInIncognito);

// Returns true if either of the Fingerprinting Protection feature flags are
// enabled, or if the Tracking Protection kFingerprintingProtectionUx flag is
// enabled.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
bool IsFingerprintingProtectionFeatureEnabled();

// Returns true if Fingerprinting Protection is enabled for the given incognito
// state. Also returns true if the Tracking Protection
// kFingerprintingProtectionUx flag is enabled when `is_incognito` is true.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
bool IsFingerprintingProtectionEnabledForIncognitoState(bool is_incognito);

// Returns true if the enable logging param is enabled for either the
// non-incognito or incognito feature.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
bool IsFingerprintingProtectionConsoleLoggingEnabled();

// Returns true if exceptions should be added/respected for sites based on the
// refresh heuristic exception.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
bool IsFingerprintingProtectionRefreshHeuristicExceptionEnabled(
    bool is_incognito);

// Returns the value of the refresh heuristic exception threshold.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
int GetFingerprintingProtectionRefreshHeuristicThreshold(bool is_incognito);

// Randomly determines whether performance measurements will be enabled,
// using the rate specified by the regular or incognito feature flag parameter,
// depending on the value of `is_incognito`.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
bool SampleEnablePerformanceMeasurements(bool is_incognito);

COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
extern const base::FeatureParam<subresource_filter::mojom::ActivationLevel>
    kActivationLevel;

// Toggle whether to enable fingerprinting protection only when legacy 3pcd
// (i.e. not the tracking protection version) is enabled.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
extern const base::FeatureParam<bool> kEnableOnlyIf3pcBlocked;

// Toggle whether to enable console logging of blocked resources in the
// ActivationState.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
extern const base::FeatureParam<bool> kEnableConsoleLoggingNonIncognito;
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
extern const base::FeatureParam<bool> kEnableConsoleLoggingIncognito;

// String used to build the component's installer attributes and possibly
// determine which version of the fingerprinting protection list is retrieved.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
extern const base::FeatureParam<std::string> kExperimentVersionNonIncognito;
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
extern const base::FeatureParam<std::string> kExperimentVersionIncognito;

// Serves as both a toggle for the refresh heuristic exception and as the
// threshold for it. If set to 0, no exceptions are saved or applied for
// the refresh heuristic. Otherwise, this is the number of refreshes for the
// same eTLD+1 in a single WebContents before an exception should be added. See
// FingerprintingProtectionWebContentsHelper for usage.
//
// Context: We want to improve user experience on broken sites by heuristically
// detecting breakage and adding an exception in that case.
//
// Default value 0 (i.e. no exceptions added).
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
extern const base::FeatureParam<int>
    kRefreshHeuristicExceptionThresholdNonIncognito;
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
extern const base::FeatureParam<int>
    kRefreshHeuristicExceptionThresholdIncognito;

// A number in the range [0, 1], indicating the fraction of page loads that
// should have extended performance measurements enabled for timing-based
// histograms in non-incognito mode.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
extern const base::FeatureParam<double> kPerformanceMeasurementRateNonIncognito;

// A number in the range [0, 1], indicating the fraction of page loads that
// should have extended performance measurements enabled for timing-based
// histograms in incognito mode.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
extern const base::FeatureParam<double> kPerformanceMeasurementRateIncognito;

// Toggle to enable CNAME alias checks. Enabling this feature will block URL
// aliases matching fingerprinting protection filtering rules.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
BASE_DECLARE_FEATURE(kUseCnameAliasesForFingerprintingProtectionFilter);
}  // namespace fingerprinting_protection_filter::features

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_FINGERPRINTING_PROTECTION_FILTER_FEATURES_H_
