// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/shared/browser/page_load_statistics.h"

#include <string_view>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/strcat.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"

namespace subresource_filter {

PageLoadStatistics::PageLoadStatistics(const mojom::ActivationState& state,
                                       std::string_view uma_filter_tag)
    : activation_state_(state), uma_filter_tag_(uma_filter_tag) {}

PageLoadStatistics::~PageLoadStatistics() = default;

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
    base::UmaHistogramCounts1000(
        base::StrCat({uma_filter_tag_, ".PageLoad.NumSubresourceLoads.Total"}),
        aggregated_document_statistics_.num_loads_total);
    base::UmaHistogramCounts1000(
        base::StrCat(
            {uma_filter_tag_, ".PageLoad.NumSubresourceLoads.Evaluated"}),
        aggregated_document_statistics_.num_loads_evaluated);
    base::UmaHistogramCounts1000(
        base::StrCat(
            {uma_filter_tag_, ".PageLoad.NumSubresourceLoads.MatchedRules"}),
        aggregated_document_statistics_.num_loads_matching_rules);
    base::UmaHistogramCounts1000(
        base::StrCat(
            {uma_filter_tag_, ".PageLoad.NumSubresourceLoads.Disallowed"}),
        aggregated_document_statistics_.num_loads_disallowed);
  }

  if (activation_state_.measure_performance) {
    CHECK(
        activation_state_.activation_level != mojom::ActivationLevel::kDisabled,
        base::NotFatalUntil::M129);
    base::UmaHistogramCustomTimes(
        base::StrCat({uma_filter_tag_,
                      ".PageLoad.SubresourceEvaluation.TotalWallDuration"}),
        aggregated_document_statistics_.evaluation_total_wall_duration,
        base::Microseconds(1), base::Seconds(10), 50);

    base::UmaHistogramCustomTimes(
        base::StrCat({uma_filter_tag_,
                      ".PageLoad.SubresourceEvaluation.TotalCPUDuration"}),
        aggregated_document_statistics_.evaluation_total_cpu_duration,
        base::Microseconds(1), base::Seconds(10), 50);
  }
  // Theoretically, we should be able to add an else case that CHECK()s that the
  // evaluation durations are zero. However, this causes crashes as the renderer
  // and browser appear to sometimes get out of sync. See crbug.com/372883698.
}

}  // namespace subresource_filter
