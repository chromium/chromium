// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_navigation_data.h"

#include "base/base64.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

OptimizationGuideNavigationData::OptimizationGuideNavigationData(
    int64_t navigation_id,
    base::TimeTicks navigation_start)
    : navigation_id_(navigation_id), navigation_start_(navigation_start) {}

OptimizationGuideNavigationData::~OptimizationGuideNavigationData() {
  RecordMetrics();
}

void OptimizationGuideNavigationData::RecordMetrics() const {
  RecordOptimizationGuideUKM();
}

void OptimizationGuideNavigationData::RecordOptimizationGuideUKM() const {
  bool did_record_metric = false;
  ukm::SourceId ukm_source_id =
      ukm::ConvertToSourceId(navigation_id_, ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::OptimizationGuide builder(ukm_source_id);

  // Record hints fetch metrics.
  if (hints_fetch_start_.has_value()) {
    if (hints_fetch_latency().has_value()) {
      builder.SetNavigationHintsFetchRequestLatency(
          hints_fetch_latency()->InMilliseconds());
    } else {
      builder.SetNavigationHintsFetchRequestLatency(INT64_MAX);
    }
    did_record_metric = true;
  }
  if (hints_fetch_attempt_status_.has_value()) {
    builder.SetNavigationHintsFetchAttemptStatus(
        static_cast<int>(*hints_fetch_attempt_status_));
    did_record_metric = true;
  }

  // Record registered types/targets metrics.
  if (!registered_optimization_types_.empty()) {
    int64_t types_bitmask = 0;
    for (const auto& optimization_type : registered_optimization_types_) {
      // Optimization types that are out of range cannot be represented in this
      // bitmask.
      if (0 <= optimization_type && optimization_type < 64) {
        types_bitmask |= (int64_t{1} << static_cast<int>(optimization_type));
      }
    }
    builder.SetRegisteredOptimizationTypes(types_bitmask);
    did_record_metric = true;
  }
  if (!registered_optimization_targets_.empty()) {
    int64_t targets_bitmask = 0;
    for (const auto& optimization_target : registered_optimization_targets_) {
      targets_bitmask |= (int64_t{1} << static_cast<int>(optimization_target));
    }
    builder.SetRegisteredOptimizationTargets(targets_bitmask);
    did_record_metric = true;
  }

  // Only record UKM if a metric was recorded.
  if (did_record_metric)
    builder.Record(ukm::UkmRecorder::Get());
}

std::optional<base::TimeDelta>
OptimizationGuideNavigationData::hints_fetch_latency() const {
  if (!hints_fetch_start_ || !hints_fetch_end_) {
    // Either a fetch was not initiated for this navigation or the fetch did not
    // completely successfully.
    return std::nullopt;
  }

  if (*hints_fetch_end_ < *hints_fetch_start_) {
    // This can happen if a hints fetch was started for a redirect, but the
    // fetch had not successfully completed yet.
    return std::nullopt;
  }

  return *hints_fetch_end_ - *hints_fetch_start_;
}
