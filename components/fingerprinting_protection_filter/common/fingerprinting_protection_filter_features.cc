// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"

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
             kEnableFingerprintingProtectionFilterInIncognito);
}

bool IsFingerprintingProtectionEnabledInIncognito(bool is_incognito) {
  if (!is_incognito) {
    return false;
  }
  return base::FeatureList::IsEnabled(
      kEnableFingerprintingProtectionFilterInIncognito);
}

bool IsFingerprintingProtectionEnabledInNonIncognito(bool is_incognito) {
  if (is_incognito) {
    return false;
  }
  return base::FeatureList::IsEnabled(kEnableFingerprintingProtectionFilter);
}

bool IsFingerprintingProtectionEnabledForIncognitoState(bool is_incognito) {
  return IsFingerprintingProtectionEnabledInIncognito(is_incognito) ||
         IsFingerprintingProtectionEnabledInNonIncognito(is_incognito);
}

bool IsFingerprintingProtectionConsoleLoggingEnabled() {
  // We don't care which feature flag the param is enabled on - if the
  // param is set to true with either feature flag, we should log when blocking.
  return kEnableConsoleLoggingIncognito.Get() ||
         kEnableConsoleLoggingNonIncognito.Get();
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

const base::FeatureParam<bool> kEnableOn3pcBlocked{
    &kEnableFingerprintingProtectionFilter, "enable_on_3pc_blocked", false};

const base::FeatureParam<bool> kEnableConsoleLoggingNonIncognito{
    &kEnableFingerprintingProtectionFilter, kEnableConsoleLoggingParam, false};

const base::FeatureParam<bool> kEnableConsoleLoggingIncognito{
    &kEnableFingerprintingProtectionFilterInIncognito,
    kEnableConsoleLoggingParam, false};

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
