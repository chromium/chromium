// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/process_metrics_decorator.h"

#include <memory>
#include <utility>

#include "base/byte_count.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"
#include "components/performance_manager/public/resource_attribution/frame_context.h"
#include "components/performance_manager/public/resource_attribution/process_context.h"
#include "components/performance_manager/public/resource_attribution/worker_context.h"

namespace performance_manager {

namespace {

// The process metrics refresh interval.
constexpr base::TimeDelta kMetricsRefreshInterval = base::Minutes(2);

}  // namespace

ProcessMetricsDecorator::ProcessMetricsDecorator() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}
ProcessMetricsDecorator::~ProcessMetricsDecorator() = default;

class ProcessMetricsDecorator::NodeMetricsUpdater {
 public:
  explicit NodeMetricsUpdater(
      const resource_attribution::MemorySummaryResult& memory_summary)
      : memory_summary_(memory_summary) {}

  void operator()(const resource_attribution::ProcessContext& context) const {
    if (!context.GetProcessNode()) {
      return;
    }
    auto* process_node = ProcessNodeImpl::FromNode(context.GetProcessNode());
    process_node->set_private_footprint(memory_summary_->private_footprint);
    process_node->set_resident_set(memory_summary_->resident_set_size);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
    process_node->set_private_swap(memory_summary_->private_swap);
#endif
  }

  void operator()(const resource_attribution::FrameContext& context) const {
    if (!context.GetFrameNode()) {
      return;
    }
    auto* frame_node = FrameNodeImpl::FromNode(context.GetFrameNode());
    frame_node->SetPrivateFootprintEstimate(memory_summary_->private_footprint);
    frame_node->SetResidentSetEstimate(memory_summary_->resident_set_size);
  }

  void operator()(const resource_attribution::WorkerContext& context) const {
    if (!context.GetWorkerNode()) {
      return;
    }
    auto* worker_node = WorkerNodeImpl::FromNode(context.GetWorkerNode());
    worker_node->SetPrivateFootprintEstimate(
        memory_summary_->private_footprint);
    worker_node->SetResidentSetEstimate(memory_summary_->resident_set_size);
  }

  void operator()(const auto& context) const { NOTREACHED(); }

 private:
  const raw_ref<const resource_attribution::MemorySummaryResult>
      memory_summary_;
};

// Concrete implementation of a
// ProcessMetricsDecorator::ScopedMetricsInterestToken
class ProcessMetricsDecorator::ScopedMetricsInterestTokenImpl
    : public ProcessMetricsDecorator::ScopedMetricsInterestToken {
 public:
  explicit ScopedMetricsInterestTokenImpl(Graph* graph);
  ScopedMetricsInterestTokenImpl(const ScopedMetricsInterestTokenImpl& other) =
      delete;
  ScopedMetricsInterestTokenImpl& operator=(
      const ScopedMetricsInterestTokenImpl&) = delete;
  ~ScopedMetricsInterestTokenImpl() override;

 protected:
  raw_ptr<Graph> graph_;
};

ProcessMetricsDecorator::ScopedMetricsInterestTokenImpl::
    ScopedMetricsInterestTokenImpl(Graph* graph)
    : graph_(graph) {
  auto* decorator = graph->GetRegisteredObjectAs<ProcessMetricsDecorator>();
  CHECK(decorator);
  decorator->OnMetricsInterestTokenCreated();
}

ProcessMetricsDecorator::ScopedMetricsInterestTokenImpl::
    ~ScopedMetricsInterestTokenImpl() {
  auto* decorator = graph_->GetRegisteredObjectAs<ProcessMetricsDecorator>();
  // This could be destroyed after removing the decorator from the graph.
  if (decorator) {
    decorator->OnMetricsInterestTokenReleased();
  }
}

// static
std::unique_ptr<ProcessMetricsDecorator::ScopedMetricsInterestToken>
ProcessMetricsDecorator::RegisterInterestForProcessMetrics(Graph* graph) {
  return std::make_unique<ScopedMetricsInterestTokenImpl>(graph);
}

void ProcessMetricsDecorator::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(
      this, "ProcessMetricsDecorator");
}

void ProcessMetricsDecorator::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
}

base::Value::Dict ProcessMetricsDecorator::DescribeSystemNodeData(
    const SystemNode*) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict ret;
  ret.Set("interest_token_count",
          base::NumberToString(metrics_interest_token_count_));
  return ret;
}

bool ProcessMetricsDecorator::IsTimerRunningForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return scoped_query_.has_value();
}

base::TimeDelta ProcessMetricsDecorator::GetTimerDelayForTesting() const {
  return kMetricsRefreshInterval;
}

void ProcessMetricsDecorator::OnMetricsInterestTokenCreated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++metrics_interest_token_count_;
  if (metrics_interest_token_count_ == 1) {
    CHECK(!scoped_query_.has_value());
    scoped_query_ =
        resource_attribution::QueryBuilder()
            .AddAllContextsOfType<resource_attribution::ProcessContext>()
            .AddAllContextsOfType<resource_attribution::FrameContext>()
            .AddAllContextsOfType<resource_attribution::WorkerContext>()
            .AddResourceType(resource_attribution::ResourceType::kMemorySummary)
            .CreateScopedQuery();
    query_observer_.Observe(base::OptionalToPtr(scoped_query_));
    // Take the first metrics measurement immediately.
    scoped_query_->QueryOnce();
    scoped_query_->Start(kMetricsRefreshInterval,
                         /* observe_other_queries= */ true);
  }
}

void ProcessMetricsDecorator::OnMetricsInterestTokenReleased() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(scoped_query_.has_value());
  CHECK_GT(metrics_interest_token_count_, 0U);
  --metrics_interest_token_count_;
  if (metrics_interest_token_count_ == 0) {
    query_observer_.Reset();
    scoped_query_.reset();
  }
}

void ProcessMetricsDecorator::OnResourceUsageUpdated(
    const resource_attribution::QueryResultMap& results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GT(metrics_interest_token_count_, 0U);
  if (results.empty()) {
    return;
  }
  for (const auto& [resource_context, result] : results) {
    std::visit(NodeMetricsUpdater(result.memory_summary_result.value()),
               resource_context);
  }
  GraphImpl::FromGraph(GetOwningGraph())
      ->GetSystemNodeImpl()
      ->OnProcessMemoryMetricsAvailable();
}

void ProcessMetricsDecorator::RequestImmediateMetrics(
    base::OnceClosure on_metrics_received) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Callers should have an interest token before calling
  // RequestImmediateMetrics().
  CHECK_GT(metrics_interest_token_count_, 0U);
  resource_attribution::QueryBuilder()
      .AddAllContextsOfType<resource_attribution::ProcessContext>()
      .AddAllContextsOfType<resource_attribution::FrameContext>()
      .AddAllContextsOfType<resource_attribution::WorkerContext>()
      .AddResourceType(resource_attribution::ResourceType::kMemorySummary)
      .QueryOnce(
          base::BindOnce(&ProcessMetricsDecorator::OnResourceUsageUpdated,
                         weak_factory_.GetWeakPtr())
              .Then(std::move(on_metrics_received)));
}

}  // namespace performance_manager
