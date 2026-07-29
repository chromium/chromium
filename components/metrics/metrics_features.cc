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

BASE_FEATURE(kIOSBackgroundMetrics, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_RUNTIME_MUTABLE_FEATURE(kNoopRuntimeMutableFeatureDefaultEnabled,
                             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_RUNTIME_MUTABLE_FEATURE(kNoopRuntimeMutableFeatureVariationsEnabled,
                             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_RUNTIME_MUTABLE_FEATURE(kNoopRuntimeMutable1,
                             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_RUNTIME_MUTABLE_FEATURE(kNoopRuntimeMutable2,
                             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_RUNTIME_MUTABLE_FEATURE(kNoopRuntimeMutable3,
                             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_RUNTIME_MUTABLE_FEATURE(kNoopRuntimeMutable4,
                             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_RUNTIME_MUTABLE_FEATURE(kNoopRuntimeMutable5,
                             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_RUNTIME_MUTABLE_FEATURE(kNoopRuntimeMutable6,
                             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_RUNTIME_MUTABLE_FEATURE(kNoopRuntimeMutable7,
                             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_RUNTIME_MUTABLE_FEATURE(kNoopRuntimeMutable8,
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
  feature_list->EnableRuntimeMutability(
      kNoopRuntimeMutable1, base::BindRepeating(&OnNoopFeatureStateChanged));
  feature_list->EnableRuntimeMutability(
      kNoopRuntimeMutable2, base::BindRepeating(&OnNoopFeatureStateChanged));
  feature_list->EnableRuntimeMutability(
      kNoopRuntimeMutable3, base::BindRepeating(&OnNoopFeatureStateChanged));
  feature_list->EnableRuntimeMutability(
      kNoopRuntimeMutable4, base::BindRepeating(&OnNoopFeatureStateChanged));
  feature_list->EnableRuntimeMutability(
      kNoopRuntimeMutable5, base::BindRepeating(&OnNoopFeatureStateChanged));
  feature_list->EnableRuntimeMutability(
      kNoopRuntimeMutable6, base::BindRepeating(&OnNoopFeatureStateChanged));
  feature_list->EnableRuntimeMutability(
      kNoopRuntimeMutable7, base::BindRepeating(&OnNoopFeatureStateChanged));
  feature_list->EnableRuntimeMutability(
      kNoopRuntimeMutable8, base::BindRepeating(&OnNoopFeatureStateChanged));
}

}  // namespace metrics::features
