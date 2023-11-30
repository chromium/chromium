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
#include "components/performance_manager/resource_attribution/worker_client_pages.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace performance_manager::resource_attribution {

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
  std::map<ResourceContext, QueryResult> results;

  // Adds the memory from `summary` to a MemorySummaryResult for `context`.
  // Returns true if a new result was created, false if one already existed.
  auto accumulate_summary = [&results, now = base::TimeTicks::Now()](
                                const ResourceContext& context,
                                MemorySummaryMeasurement summary) -> bool {
    // Create a result with metadata if the key isn't in the map yet.
    const auto [it, inserted] = results.try_emplace(
        context, QueryResult(MemorySummaryResult{
                     .metadata = {.measurement_time = now}}));
    MemorySummaryResult& result = absl::get<MemorySummaryResult>(it->second);
    result.resident_set_size_kb += summary.resident_set_size_kb;
    result.private_footprint_kb += summary.private_footprint_kb;
    return inserted;
  };

  // Iterate and record all process results.
  for (const auto& [process_context, process_summary] : process_summaries) {
    bool inserted = accumulate_summary(process_context, process_summary);
    CHECK(inserted);

    // Split results between all frames and workers in the process.
    const ProcessNode* process_node = process_context.GetProcessNode();
    if (!process_node) {
      continue;
    }
    resource_attribution::SplitResourceAmongFramesAndWorkers(
        process_summary, process_node,
        [&](const FrameNode* f, MemorySummaryMeasurement summary) {
          bool inserted = accumulate_summary(f->GetResourceContext(), summary);
          CHECK(inserted);
          accumulate_summary(f->GetPageNode()->GetResourceContext(), summary);
        },
        [&](const WorkerNode* w, MemorySummaryMeasurement summary) {
          bool inserted = accumulate_summary(w->GetResourceContext(), summary);
          CHECK(inserted);
          for (const PageNode* page_node : GetWorkerClientPages(w)) {
            accumulate_summary(page_node->GetResourceContext(), summary);
          }
        });
  }
  std::move(callback).Run(results);
}

}  // namespace performance_manager::resource_attribution
