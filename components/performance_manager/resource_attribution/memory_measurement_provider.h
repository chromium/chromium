// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_MEMORY_MEASUREMENT_PROVIDER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_MEMORY_MEASUREMENT_PROVIDER_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/resource_attribution/memory_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"

namespace resource_attribution {

class MemoryMeasurementProvider
    : public performance_manager::NodeDataDescriberDefaultImpl {
 public:
  explicit MemoryMeasurementProvider(Graph* graph);
  ~MemoryMeasurementProvider() override;

  MemoryMeasurementProvider(const MemoryMeasurementProvider& other) = delete;
  MemoryMeasurementProvider& operator=(const MemoryMeasurementProvider&) =
      delete;

  // The given `factory` will be used to create a MemoryMeasurementDelegate for
  // ProcessNodes to be measured.
  void SetDelegateFactoryForTesting(
      MemoryMeasurementDelegate::Factory* factory);

  // Results of a memory summary query. Each QueryResults object will contain a
  // MemorySummaryResult.
  using ResultCallback = base::OnceCallback<void(QueryResultMap)>;

  // Requests memory summaries for all processes. `callback` will be invoked
  // with the results.
  void RequestMemorySummary(ResultCallback callback);

  // NodeDataDescriber:
  base::Value::Dict DescribeFrameNodeData(const FrameNode* node) const override;
  base::Value::Dict DescribePageNodeData(const PageNode* node) const override;
  base::Value::Dict DescribeProcessNodeData(
      const ProcessNode* node) const override;
  base::Value::Dict DescribeWorkerNodeData(
      const WorkerNode* node) const override;

 private:
  void OnMemorySummary(
      ResultCallback callback,
      MemoryMeasurementDelegate::MemorySummaryMap process_summaries);

  // Returns description of the most recent measurement of `context` for
  // NodeDataDescriber, or an empty dict if there is none.
  base::Value::Dict DescribeContextData(const ResourceContext& context) const;

  SEQUENCE_CHECKER(sequence_checker_);

  // Delegate that measures memory usage of ProcessNodes.
  std::unique_ptr<MemoryMeasurementDelegate> measurement_delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);

  raw_ptr<Graph> graph_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // The last measurement, cached for the node data describers.
  // TODO(crbug.com/325328567): Use a central cache for all Resource Attribution
  // measurements.
  QueryResultMap cached_results_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<MemoryMeasurementProvider> weak_factory_{this};
};

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_MEMORY_MEASUREMENT_PROVIDER_H_
