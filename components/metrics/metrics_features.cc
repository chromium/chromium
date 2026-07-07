// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_features.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"

namespace metrics::features {

BASE_FEATURE(kStructuredMetrics,
             "EnableStructuredMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFlushPersistentSystemProfileOnWrite,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReportingServiceAlwaysFlush, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMetricsLogTrimming, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kNoResetMetricsUploadBackoffOnForeground,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMetricsLogJobSchedulerUpload, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMetricsLogJobSchedulerUploadBackoffOnStopTask,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Enabled by default - intended as a kill-switch.
BASE_FEATURE(kPerProfileMetrics, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRestructureMetricsConsentSettings,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kConsolidateMetricsServiceLocales,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_RUNTIME_MUTABLE_FEATURE(kNoopRuntimeMutableFeatureDefaultEnabled,
                             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_RUNTIME_MUTABLE_FEATURE(kNoopRuntimeMutableFeatureVariationsEnabled,
                             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

void OnNoopFeatureStateChanged(
    std::reference_wrapper<const base::Feature> feature,
    std::string_view field_trial_name,
    std::string_view group_name,
    base::FeatureList::OverrideState override_state) {
  DVLOG(1) << "Runtime mutable feature state changed: " << feature.get().name
           << ", trial: " << field_trial_name << ", group: " << group_name
           << ", state: " << override_state;
}

}  // namespace

void EnableNoopRuntimeMutableFeatures(base::FeatureList* feature_list) {
  feature_list->EnableRuntimeMutability(
      kNoopRuntimeMutableFeatureDefaultEnabled,
      base::BindRepeating(&OnNoopFeatureStateChanged));
  feature_list->EnableRuntimeMutability(
      kNoopRuntimeMutableFeatureVariationsEnabled,
      base::BindRepeating(&OnNoopFeatureStateChanged));
}

}  // namespace metrics::features
