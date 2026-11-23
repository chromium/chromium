// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/metrics/page_resource_monitor.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace performance_manager::metrics {

namespace {

using PageMeasurementBackgroundState =
    PageResourceMonitor::PageMeasurementBackgroundState;

using PageContext = resource_attribution::PageContext;
using QueryResultMap = resource_attribution::QueryResultMap;
using ResourceContext = resource_attribution::ResourceContext;
using ResourceType = resource_attribution::ResourceType;

// CPU usage metrics are provided as a double in the [0.0, number of cores *
// 100.0] range. The CPU usage is usually below 1%, so the UKM is
// reported out of 10,000 instead of out of 100 to make analyzing the data
// easier. This is the same scale factor used by the
// PerformanceMonitor.AverageCPU10 histograms recorded in
// chrome/browser/metrics/power/process_metrics_recorder_util.cc.
constexpr int kCPUUsageFactor = 100 * 100;

// The time between calls to OnResourceUsageUpdated()
constexpr base::TimeDelta kCollectionDelay = base::Minutes(2);

PageMeasurementBackgroundState GetBackgroundStateForMeasurementPeriod(
    const PageNode* page_node,
    base::TimeTicks time_of_last_resource_usage) {
  auto [is_visible, last_visibility_change_time] =
      page_node->GetVisibilityState();
  if (time_of_last_resource_usage < last_visibility_change_time) {
    return PageMeasurementBackgroundState::kMixedForegroundBackground;
  }
  if (is_visible) {
    return PageMeasurementBackgroundState::kForeground;
  }
  // Check if the page was audible for the entire measurement period.
  auto [is_audible, last_audible_change_time] = page_node->GetAudibleState();
  if (last_audible_change_time.has_value() &&
      time_of_last_resource_usage < last_audible_change_time.value()) {
    return PageMeasurementBackgroundState::kBackgroundMixedAudible;
  }
  if (is_audible) {
    return PageMeasurementBackgroundState::kAudibleInBackground;
  }
  return PageMeasurementBackgroundState::kBackground;
}

resource_attribution::QueryBuilder CPUQueryBuilder() {
  resource_attribution::QueryBuilder builder;
  builder.AddAllContextsOfType<PageContext>().AddResourceType(
      ResourceType::kCPUTime);
  return builder;
}

const PageNode* PageNodeFromContext(const ResourceContext& context) {
  // The query returned by CPUQueryBuilder() should only measure PageContexts.
  // AsContext() asserts that `context` is a PageContext.
  return resource_attribution::AsContext<PageContext>(context).GetPageNode();
}

bool ContextIsTab(const ResourceContext& context) {
  const PageNode* page_node = PageNodeFromContext(context);
  return page_node && page_node->GetType() == PageType::kTab;
}

}  // namespace

PageResourceMonitor::PageResourceMonitor()
    : resource_query_(CPUQueryBuilder()
                          .AddResourceType(ResourceType::kMemorySummary)
                          .CreateScopedQuery()),
      // Only calculate results for tabs, not extensions.
      cpu_proportion_tracker_(base::BindRepeating(&ContextIsTab)) {
  query_observation_.Observe(&resource_query_);
  resource_query_.Start(kCollectionDelay);
  CPUQueryBuilder().QueryOnce(
      base::BindOnce(&PageResourceMonitor::StartFirstCPUInterval,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

PageResourceMonitor::~PageResourceMonitor() = default;

void PageResourceMonitor::OnResourceUsageUpdated(
    const QueryResultMap& results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto now = base::TimeTicks::Now();
  const auto page_cpu_usage =
      cpu_proportion_tracker_.StartNextInterval(now, results);

  // Calculate the overall CPU usage.
  double total_cpu_usage = 0;
  for (const auto& [page_context, cpu_usage] : page_cpu_usage) {
    total_cpu_usage += cpu_usage;
  }

  // Contexts in `page_cpu_usage` are a subset of contexts in `results`.
  for (const auto& [page_context, result] : results) {
    const PageNode* page_node = PageNodeFromContext(page_context);
    if (!page_node) {
      // Page was deleted before measurement completed. Nothing to log.
      continue;
    }
    if (page_node->GetType() != PageType::kTab) {
      continue;
    }
    const ukm::SourceId source_id = page_node->GetUkmSourceID();
    auto ukm = ukm::builders::PerformanceManager_PageResourceUsage2(source_id);
    ukm.SetBackgroundState(
        static_cast<int64_t>(GetBackgroundStateForMeasurementPeriod(
            page_node, time_of_last_resource_usage_)));
    ukm.SetMeasurementAlgorithm(
        static_cast<int64_t>(PageMeasurementAlgorithm::kEvenSplitAndAggregate));
    // Add CPU usage, if this page included it.
    const auto it = page_cpu_usage.find(page_context);
    if (it != page_cpu_usage.end()) {
      ukm.SetRecentCPUUsage(kCPUUsageFactor * it->second);
      ukm.SetTotalRecentCPUUsageAllPages(kCPUUsageFactor * total_cpu_usage);
    }
    // Add memory summary, if this page included it.
    if (result.memory_summary_result.has_value()) {
      ukm.SetResidentSetSizeEstimate(
          result.memory_summary_result->resident_set_size.InKiB());
      ukm.SetPrivateFootprintEstimate(
          result.memory_summary_result->private_footprint.InKiB());
    }
    ukm.Record(ukm::UkmRecorder::Get());
  }

  time_of_last_resource_usage_ = now;
}

base::TimeDelta PageResourceMonitor::GetCollectionDelayForTesting() const {
  return kCollectionDelay;
}

void PageResourceMonitor::StartFirstCPUInterval(base::TimeTicks time,
                                                const QueryResultMap& results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cpu_proportion_tracker_.StartFirstInterval(time, results);
}

}  // namespace performance_manager::metrics
