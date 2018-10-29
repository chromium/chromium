// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/page_load_statistics.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/subresource_filter/core/common/time_measurements.h"

namespace subresource_filter {

PageLoadStatistics::PageLoadStatistics(const mojom::ActivationState& state)
    : activation_state_(state) {}

PageLoadStatistics::~PageLoadStatistics() {}

void PageLoadStatistics::OnDocumentLoadStatistics(
    const mojom::DocumentLoadStatistics& statistics) {
  // Note: Chances of overflow are negligible.
  aggregated_document_statistics_.num_loads_total += statistics.num_loads_total;
  aggregated_document_statistics_.num_loads_evaluated +=
      statistics.num_loads_evaluated;
  aggregated_document_statistics_.num_loads_matching_rules +=
      statistics.num_loads_matching_rules;
  aggregated_document_statistics_.num_loads_disallowed +=
      statistics.num_loads_disallowed;

  aggregated_document_statistics_.evaluation_total_wall_duration +=
      statistics.evaluation_total_wall_duration;
  aggregated_document_statistics_.evaluation_total_cpu_duration +=
      statistics.evaluation_total_cpu_duration;
}

void PageLoadStatistics::OnDidFinishLoad() {
  if (activation_state_.activation_level != mojom::ActivationLevel::kDisabled) {
    UMA_HISTOGRAM_COUNTS_1000(
        "SubresourceFilter.PageLoad.NumSubresourceLoads.Total",
        aggregated_document_statistics_.num_loads_total);
    UMA_HISTOGRAM_COUNTS_1000(
        "SubresourceFilter.PageLoad.NumSubresourceLoads.Evaluated",
        aggregated_document_statistics_.num_loads_evaluated);
    UMA_HISTOGRAM_COUNTS_1000(
        "SubresourceFilter.PageLoad.NumSubresourceLoads.MatchedRules",
        aggregated_document_statistics_.num_loads_matching_rules);
    UMA_HISTOGRAM_COUNTS_1000(
        "SubresourceFilter.PageLoad.NumSubresourceLoads.Disallowed",
        aggregated_document_statistics_.num_loads_disallowed);
  }

  if (activation_state_.measure_performance) {
    DCHECK(activation_state_.activation_level !=
           mojom::ActivationLevel::kDisabled);
    UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
        "SubresourceFilter.PageLoad.SubresourceEvaluation.TotalWallDuration",
        aggregated_document_statistics_.evaluation_total_wall_duration,
        base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(10),
        50);
    UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
        "SubresourceFilter.PageLoad.SubresourceEvaluation.TotalCPUDuration",
        aggregated_document_statistics_.evaluation_total_cpu_duration,
        base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(10),
        50);
  } else {
    DCHECK(aggregated_document_statistics_.evaluation_total_wall_duration
               .is_zero());
    DCHECK(aggregated_document_statistics_.evaluation_total_cpu_duration
               .is_zero());
  }
}

}  // namespace subresource_filter
