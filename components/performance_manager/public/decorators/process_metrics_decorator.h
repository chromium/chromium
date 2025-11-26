// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PROCESS_METRICS_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PROCESS_METRICS_DECORATOR_H_

#include <memory>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/resource_attribution/queries.h"

namespace performance_manager {

class Graph;
class SystemNode;

// The ProcessMetricsDecorator is responsible for adorning process nodes with
// performance metrics.
class ProcessMetricsDecorator
    : public GraphOwnedAndRegistered<ProcessMetricsDecorator>,
      public NodeDataDescriberDefaultImpl,
      public resource_attribution::QueryResultObserver {
 public:
  ProcessMetricsDecorator();

  ProcessMetricsDecorator(const ProcessMetricsDecorator&) = delete;
  ProcessMetricsDecorator& operator=(const ProcessMetricsDecorator&) = delete;

  ~ProcessMetricsDecorator() override;

  // A token used to express an interest for process metrics. Process metrics
  // will only be updated as long as there's at least one token in existence.
  //
  // These objects shouldn't be created directly, they should be acquired by
  // calling RegisterInterestForProcessMetrics.
  class ScopedMetricsInterestToken {
   public:
    ScopedMetricsInterestToken(const ScopedMetricsInterestToken& other) =
        delete;
    ScopedMetricsInterestToken& operator=(const ScopedMetricsInterestToken&) =
        delete;
    virtual ~ScopedMetricsInterestToken() = default;

   protected:
    ScopedMetricsInterestToken() = default;
  };

  // Allows a process to register an interest for process metrics. Metrics are
  // only guaranteed to be refreshed for the lifetime of the returned token.
  static std::unique_ptr<ScopedMetricsInterestToken>
  RegisterInterestForProcessMetrics(Graph* graph);

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber
  base::Value::Dict DescribeSystemNodeData(
      const SystemNode* node) const override;

  bool IsTimerRunningForTesting() const;
  base::TimeDelta GetTimerDelayForTesting() const;

  // Immediately refreshes the metrics for all the process nodes.
  // TODO(crbug.com/441134587):
  // Only MemoryMetricsRefreshWaiter uses it, and it can be replaced by
  // TabResourceUsageRefreshWaiter. The function can be removed further.
  void RequestImmediateMetrics(
      base::OnceClosure on_metrics_received = base::DoNothing());

 protected:
  class ScopedMetricsInterestTokenImpl;
  class NodeMetricsUpdater;

  // Called whenever a ScopedMetricsInterestToken is created/released.
  void OnMetricsInterestTokenCreated();
  void OnMetricsInterestTokenReleased();

  // QueryResultObserver:
  void OnResourceUsageUpdated(
      const resource_attribution::QueryResultMap& results) override;

 private:
  // The number of clients currently interested by the metrics tracked by this
  // class.
  size_t metrics_interest_token_count_ GUARDED_BY_CONTEXT(sequence_checker_) =
      0;

  std::optional<resource_attribution::ScopedResourceUsageQuery> scoped_query_
      GUARDED_BY_CONTEXT(sequence_checker_);

  resource_attribution::ScopedQueryObservation query_observer_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ProcessMetricsDecorator> weak_factory_{this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PROCESS_METRICS_DECORATOR_H_
