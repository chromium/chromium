// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/memory_measurement_provider.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/resource_attribution/attribution_helpers.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/resource_attribution/worker_client_pages.h"

namespace resource_attribution {

MemoryMeasurementProvider::MemoryMeasurementProvider(Graph* graph)
    : graph_(graph) {
  measurement_delegate_ =
      MemoryMeasurementDelegate::GetDefaultFactory()->CreateDelegate(
          graph_.get());
}

MemoryMeasurementProvider::~MemoryMeasurementProvider() = default;

void MemoryMeasurementProvider::SetDelegateFactoryForTesting(
    MemoryMeasurementDelegate::Factory* factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(factory);
  measurement_delegate_ = factory->CreateDelegate(graph_.get());
}

void MemoryMeasurementProvider::RequestMemorySummary(ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  measurement_delegate_->RequestMemorySummary(base::BindOnce(
      &MemoryMeasurementProvider::OnMemorySummary, std::move(callback)));
}

void MemoryMeasurementProvider::OnMemorySummary(
    ResultCallback callback,
    MemoryMeasurementDelegate::MemorySummaryMap process_summaries) {
  using MemorySummaryMeasurement =
      MemoryMeasurementDelegate::MemorySummaryMeasurement;
  QueryResultMap results;

  // Adds the memory from `summary` to a MemorySummaryResult for `context`.
  // Returns true if a new result was created, false if one already existed.
  auto accumulate_summary = [&results, now = base::TimeTicks::Now()](
                                const ResourceContext& context,
                                MemorySummaryMeasurement summary,
                                MeasurementAlgorithm algorithm) -> bool {
    // Create a result with metadata if the key isn't in the map yet.
    const auto [it, inserted] = results.try_emplace(
        context, QueryResults{.memory_summary_result = MemorySummaryResult{
                                  .metadata = ResultMetadata(now, algorithm),
                              }});
    MemorySummaryResult& result = it->second.memory_summary_result.value();
    if (!inserted) {
      CHECK_LE(result.metadata.measurement_time, now);
      CHECK_EQ(result.metadata.algorithm, algorithm);
    }
    result.resident_set_size_kb += summary.resident_set_size_kb;
    result.private_footprint_kb += summary.private_footprint_kb;
    return inserted;
  };

  // Iterate and record all process results.
  for (const auto& [process_context, process_summary] : process_summaries) {
    bool inserted =
        accumulate_summary(process_context, process_summary,
                           MeasurementAlgorithm::kDirectMeasurement);
    CHECK(inserted);

    // Split results between all frames and workers in the process.
    const ProcessNode* process_node = process_context.GetProcessNode();
    if (!process_node) {
      continue;
    }
    resource_attribution::SplitResourceAmongFramesAndWorkers(
        process_summary, process_node,
        [&](const FrameNode* f, MemorySummaryMeasurement summary) {
          bool inserted = accumulate_summary(f->GetResourceContext(), summary,
                                             MeasurementAlgorithm::kSplit);
          CHECK(inserted);
          accumulate_summary(f->GetPageNode()->GetResourceContext(), summary,
                             MeasurementAlgorithm::kSum);
        },
        [&](const WorkerNode* w, MemorySummaryMeasurement summary) {
          bool inserted = accumulate_summary(w->GetResourceContext(), summary,
                                             MeasurementAlgorithm::kSplit);
          CHECK(inserted);
          for (const PageNode* page_node : GetWorkerClientPages(w)) {
            accumulate_summary(page_node->GetResourceContext(), summary,
                               MeasurementAlgorithm::kSum);
          }
        });
  }
  std::move(callback).Run(std::move(results));
}

}  // namespace resource_attribution
