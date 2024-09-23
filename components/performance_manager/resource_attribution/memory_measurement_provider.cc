// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/memory_measurement_provider.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/resource_attribution/attribution_helpers.h"
#include "components/performance_manager/resource_attribution/node_data_describers.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/resource_attribution/worker_client_pages.h"
#include "content/public/browser/browsing_instance_id.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace resource_attribution {

namespace {

using performance_manager::features::kResourceAttributionIncludeOrigins;

template <typename FrameOrWorkerNode>
std::optional<OriginInBrowsingInstanceContext>
OriginInBrowsingInstanceContextForNode(
    const FrameOrWorkerNode* node,
    content::BrowsingInstanceId browsing_instance) {
  if (!base::FeatureList::IsEnabled(kResourceAttributionIncludeOrigins)) {
    return std::nullopt;
  }
  const std::optional<url::Origin> origin = node->GetOrigin();
  if (!origin.has_value()) {
    return std::nullopt;
  }
  return OriginInBrowsingInstanceContext(origin.value(), browsing_instance);
}

}  // namespace

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
  measurement_delegate_->RequestMemorySummary(
      base::BindOnce(&MemoryMeasurementProvider::OnMemorySummary,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

base::Value::Dict MemoryMeasurementProvider::DescribeFrameNodeData(
    const FrameNode* node) const {
  return DescribeContextData(node->GetResourceContext());
}

base::Value::Dict MemoryMeasurementProvider::DescribePageNodeData(
    const PageNode* node) const {
  return DescribeContextData(node->GetResourceContext());
}

base::Value::Dict MemoryMeasurementProvider::DescribeProcessNodeData(
    const ProcessNode* node) const {
  return DescribeContextData(node->GetResourceContext());
}

base::Value::Dict MemoryMeasurementProvider::DescribeWorkerNodeData(
    const WorkerNode* node) const {
  return DescribeContextData(node->GetResourceContext());
}

void MemoryMeasurementProvider::OnMemorySummary(
    ResultCallback callback,
    MemoryMeasurementDelegate::MemorySummaryMap process_summaries) {
  using MemorySummaryMeasurement =
      MemoryMeasurementDelegate::MemorySummaryMeasurement;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
          std::optional<OriginInBrowsingInstanceContext>
              origin_in_browsing_instance_context =
                  OriginInBrowsingInstanceContextForNode(
                      f, f->GetBrowsingInstanceId());
          if (origin_in_browsing_instance_context.has_value()) {
            accumulate_summary(origin_in_browsing_instance_context.value(),
                               summary, MeasurementAlgorithm::kSum);
          }
        },
        [&](const WorkerNode* w, MemorySummaryMeasurement summary) {
          bool inserted = accumulate_summary(w->GetResourceContext(), summary,
                                             MeasurementAlgorithm::kSplit);
          CHECK(inserted);

          auto [client_pages, client_browsing_instances] =
              GetWorkerClientPagesAndBrowsingInstances(w);

          for (const PageNode* page_node : client_pages) {
            accumulate_summary(page_node->GetResourceContext(), summary,
                               MeasurementAlgorithm::kSum);
          }

          for (content::BrowsingInstanceId browsing_instance :
               client_browsing_instances) {
            std::optional<OriginInBrowsingInstanceContext>
                origin_in_browsing_instance_context =
                    OriginInBrowsingInstanceContextForNode(w,
                                                           browsing_instance);
            if (origin_in_browsing_instance_context.has_value()) {
              accumulate_summary(origin_in_browsing_instance_context.value(),
                                 summary, MeasurementAlgorithm::kSum);
            }
          }
        });
  }
  cached_results_ = results;
  std::move(callback).Run(std::move(results));
}

base::Value::Dict MemoryMeasurementProvider::DescribeContextData(
    const ResourceContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict dict;
  const auto it = cached_results_.find(context);
  if (it != cached_results_.end()) {
    const MemorySummaryResult& result =
        it->second.memory_summary_result.value();
    dict.Merge(DescribeResultMetadata(result.metadata));
    dict.Set("resident_set_size_kb",
             base::NumberToString(result.resident_set_size_kb));
    dict.Set("private_footprint_kb",
             base::NumberToString(result.private_footprint_kb));
  }
  return dict;
}

}  // namespace resource_attribution
