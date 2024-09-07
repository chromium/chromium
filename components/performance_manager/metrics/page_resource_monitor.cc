// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/metrics/page_resource_monitor.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/resource_attribution/cpu_proportion_tracker.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/system_cpu/cpu_probe.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace performance_manager::metrics {

namespace {

using system_cpu::CpuProbe;
using system_cpu::CpuSample;
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
// PerformanceMonitor.AverageCPU8 histograms recorded in
// chrome/browser/metrics/power/process_metrics_recorder_util.cc.
constexpr int kCPUUsageFactor = 100 * 100;

// The time between calls to OnResourceUsageUpdated()
constexpr base::TimeDelta kCollectionDelay = base::Minutes(2);

PageMeasurementBackgroundState GetBackgroundStateForMeasurementPeriod(
    const PageNode* page_node,
    base::TimeDelta time_since_last_measurement) {
  if (page_node->GetTimeSinceLastVisibilityChange() <
      time_since_last_measurement) {
    return PageMeasurementBackgroundState::kMixedForegroundBackground;
  }
  if (page_node->IsVisible()) {
    return PageMeasurementBackgroundState::kForeground;
  }
  // Check if the page was audible for the entire measurement period.
  if (page_node->GetTimeSinceLastAudibleChange().value_or(
          base::TimeDelta::Max()) < time_since_last_measurement) {
    return PageMeasurementBackgroundState::kBackgroundMixedAudible;
  }
  if (page_node->IsAudible()) {
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

class PageResourceMonitor::CPUResultConverter {
 public:
  // A callback that's invoked with the converted results.
  using ResultCallback = base::OnceCallback<void(const PageCPUUsageMap&,
                                                 std::optional<CpuSample>)>;

  explicit CPUResultConverter(std::unique_ptr<CpuProbe> system_cpu_probe);
  ~CPUResultConverter() = default;

  base::WeakPtr<CPUResultConverter> GetWeakPtr();

  bool HasSystemCPUProbe() const;

  // Invokes `result_callback_` with the converted `results`.
  void OnResourceUsageUpdated(ResultCallback result_callback,
                              const QueryResultMap& results);

 private:
  void StartFirstInterval(base::TimeTicks time, const QueryResultMap& results);
  void StartNextInterval(ResultCallback result_callback,
                         base::TimeTicks time,
                         const QueryResultMap& results,
                         std::optional<CpuSample> system_cpu);

  std::unique_ptr<CpuProbe> system_cpu_probe_;
  resource_attribution::CPUProportionTracker proportion_tracker_;
  base::WeakPtrFactory<CPUResultConverter> weak_factory_{this};
};

PageResourceMonitor::PageResourceMonitor(bool enable_system_cpu_probe)
    : resource_query_(CPUQueryBuilder()
                          .AddResourceType(ResourceType::kMemorySummary)
                          .CreateScopedQuery()) {
  query_observation_.Observe(&resource_query_);
  resource_query_.Start(kCollectionDelay);
  cpu_result_converter_ = std::make_unique<CPUResultConverter>(
      enable_system_cpu_probe ? CpuProbe::Create() : nullptr);
}

PageResourceMonitor::~PageResourceMonitor() = default;

void PageResourceMonitor::OnResourceUsageUpdated(
    const QueryResultMap& results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cpu_result_converter_->OnResourceUsageUpdated(
      base::BindOnce(&PageResourceMonitor::OnPageResourceUsageResult,
                     weak_factory_.GetWeakPtr(), results),
      results);
}

base::TimeDelta PageResourceMonitor::GetCollectionDelayForTesting() const {
  return kCollectionDelay;
}

void PageResourceMonitor::OnPageResourceUsageResult(
    const QueryResultMap& results,
    const PageCPUUsageMap& page_cpu_usage,
    std::optional<CpuSample> system_cpu) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Calculate the overall CPU usage.
  double total_cpu_usage = 0;
  for (const auto& [page_context, cpu_usage] : page_cpu_usage) {
    total_cpu_usage += cpu_usage;
  }

  // Contexts in `page_cpu_usage` are a subset of contexts in `results`.
  const auto now = base::TimeTicks::Now();
  for (const auto& [page_context, result] : results) {
    const PageNode* page_node = PageNodeFromContext(page_context);
    if (!page_node) {
      // Page was deleted while waiting for system CPU. Nothing to log.
      continue;
    }
    if (page_node->GetType() != PageType::kTab) {
      continue;
    }
    const ukm::SourceId source_id = page_node->GetUkmSourceID();
    auto ukm = ukm::builders::PerformanceManager_PageResourceUsage2(source_id);
    ukm.SetBackgroundState(
        static_cast<int64_t>(GetBackgroundStateForMeasurementPeriod(
            page_node, now - time_of_last_resource_usage_)));
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
          result.memory_summary_result->resident_set_size_kb);
      ukm.SetPrivateFootprintEstimate(
          result.memory_summary_result->private_footprint_kb);
    }
    ukm.Record(ukm::UkmRecorder::Get());
  }

  time_of_last_resource_usage_ = now;
}

PageResourceMonitor::CPUResultConverter::CPUResultConverter(
    std::unique_ptr<CpuProbe> system_cpu_probe)
    : system_cpu_probe_(std::move(system_cpu_probe)),
      // Only calculate results for tabs, not extensions.
      proportion_tracker_(base::BindRepeating(&ContextIsTab)) {
  CPUQueryBuilder().QueryOnce(
      base::BindOnce(&CPUResultConverter::StartFirstInterval, GetWeakPtr(),
                     base::TimeTicks::Now()));
  if (system_cpu_probe_) {
    system_cpu_probe_->StartSampling();
  }
}

base::WeakPtr<PageResourceMonitor::CPUResultConverter>
PageResourceMonitor::CPUResultConverter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool PageResourceMonitor::CPUResultConverter::HasSystemCPUProbe() const {
  return static_cast<bool>(system_cpu_probe_);
}

void PageResourceMonitor::CPUResultConverter::OnResourceUsageUpdated(
    CPUResultConverter::ResultCallback result_callback,
    const QueryResultMap& results) {
  auto next_update_callback = base::BindOnce(
      &CPUResultConverter::StartNextInterval, GetWeakPtr(),
      std::move(result_callback), base::TimeTicks::Now(), results);
  if (system_cpu_probe_) {
    system_cpu_probe_->RequestSample(std::move(next_update_callback));
  } else {
    std::move(next_update_callback).Run(std::nullopt);
  }
}

void PageResourceMonitor::CPUResultConverter::StartFirstInterval(
    base::TimeTicks time,
    const QueryResultMap& results) {
  proportion_tracker_.StartFirstInterval(time, results);
}

void PageResourceMonitor::CPUResultConverter::StartNextInterval(
    CPUResultConverter::ResultCallback result_callback,
    base::TimeTicks time,
    const QueryResultMap& results,
    std::optional<CpuSample> system_cpu) {
  std::move(result_callback)
      .Run(proportion_tracker_.StartNextInterval(time, results),
           std::move(system_cpu));
}

}  // namespace performance_manager::metrics
