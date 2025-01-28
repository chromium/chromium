// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "fingerprinting_protection_filter_features.h"

namespace fingerprinting_protection_filter::features {

// When enabled, loads the Fingerprinting Protection component and evaluates
// resource requests on certain pages against the Fingerprinting Protection
// blocklist, possibly blocks via a subresource filter.
BASE_FEATURE(kEnableFingerprintingProtectionFilter,
             "EnableFingerprintingProtectionFilter",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFingerprintingProtectionFilterInIncognito,
             "EnableFingerprintingProtectionFilterInIncognito",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsFingerprintingProtectionFeatureEnabled() {
  return base::FeatureList::IsEnabled(kEnableFingerprintingProtectionFilter) ||
         base::FeatureList::IsEnabled(
             kEnableFingerprintingProtectionFilterInIncognito) ||
         base::FeatureList::IsEnabled(
             privacy_sandbox::kFingerprintingProtectionUx);
}

bool IsFingerprintingProtectionEnabledForIncognitoState(bool is_incognito) {
  if (is_incognito) {
    return base::FeatureList::IsEnabled(
               kEnableFingerprintingProtectionFilterInIncognito) ||
           base::FeatureList::IsEnabled(
               privacy_sandbox::kFingerprintingProtectionUx);
  }
  return base::FeatureList::IsEnabled(kEnableFingerprintingProtectionFilter);
}

bool IsFingerprintingProtectionConsoleLoggingEnabled() {
  // We don't care which feature flag the param is enabled on - if the
  // param is set to true with either feature flag, we should log when blocking.
  return kEnableConsoleLoggingIncognito.Get() ||
         kEnableConsoleLoggingNonIncognito.Get();
}

bool IsFingerprintingProtectionRefreshHeuristicExceptionEnabled(
    bool is_incognito) {
  return GetFingerprintingProtectionRefreshHeuristicThreshold(is_incognito) > 0;
}

int GetFingerprintingProtectionRefreshHeuristicThreshold(bool is_incognito) {
  if (is_incognito) {
    return kRefreshHeuristicExceptionThresholdIncognito.Get();
  }

  return kRefreshHeuristicExceptionThresholdNonIncognito.Get();
}

bool SampleEnablePerformanceMeasurements(bool is_incognito) {
  if (!base::ThreadTicks::IsSupported()) {
    // Can't do accurate performance measurements if ThreadTicks not supported.
    return false;
  }

  // Get sampling rate based on whether we're in incognito.
  const base::Feature& feature =
      is_incognito ? features::kEnableFingerprintingProtectionFilterInIncognito
                   : features::kEnableFingerprintingProtectionFilter;
  const double sampling_rate = GetFieldTrialParamByFeatureAsDouble(
      feature, features::kPerformanceMeasurementRateParam, 0.0);

  // Randomly sample.
  return base::RandDouble() < sampling_rate;
}

constexpr base::FeatureParam<subresource_filter::mojom::ActivationLevel>::Option
    kActivationLevelOptions[] = {
        {subresource_filter::mojom::ActivationLevel::kDisabled, "disabled"},
        {subresource_filter::mojom::ActivationLevel::kDryRun, "dry_run"},
        {subresource_filter::mojom::ActivationLevel::kEnabled, "enabled"}};

const base::FeatureParam<subresource_filter::mojom::ActivationLevel>
    kActivationLevel{&kEnableFingerprintingProtectionFilter, "activation_level",
                     subresource_filter::mojom::ActivationLevel::kEnabled,
                     &kActivationLevelOptions};

const base::FeatureParam<bool> kEnableOnlyIf3pcBlocked{
    &kEnableFingerprintingProtectionFilter, "enable_only_if_3pc_blocked",
    false};

const base::FeatureParam<bool> kEnableConsoleLoggingNonIncognito{
    &kEnableFingerprintingProtectionFilter, kEnableConsoleLoggingParam, false};

const base::FeatureParam<bool> kEnableConsoleLoggingIncognito{
    &kEnableFingerprintingProtectionFilterInIncognito,
    kEnableConsoleLoggingParam, false};

const base::FeatureParam<std::string> kExperimentVersionNonIncognito{
    &kEnableFingerprintingProtectionFilter, kExperimentVersionParam, ""};

const base::FeatureParam<std::string> kExperimentVersionIncognito{
    &kEnableFingerprintingProtectionFilterInIncognito, kExperimentVersionParam,
    ""};

const base::FeatureParam<int> kRefreshHeuristicExceptionThresholdNonIncognito{
    &kEnableFingerprintingProtectionFilter,
    kRefreshHeuristicExceptionThresholdParam, 0};

const base::FeatureParam<int> kRefreshHeuristicExceptionThresholdIncognito{
    &kEnableFingerprintingProtectionFilterInIncognito,
    kRefreshHeuristicExceptionThresholdParam, 0};

const base::FeatureParam<double> kPerformanceMeasurementRateNonIncognito{
    &kEnableFingerprintingProtectionFilter, kPerformanceMeasurementRateParam,
    0.0};

const base::FeatureParam<double> kPerformanceMeasurementRateIncognito{
    &kEnableFingerprintingProtectionFilterInIncognito,
    kPerformanceMeasurementRateParam, 0.0};

BASE_FEATURE(kUseCnameAliasesForFingerprintingProtectionFilter,
             "UseCnameAliasesForFingerprintingProtectionFilter",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace fingerprinting_protection_filter::features
