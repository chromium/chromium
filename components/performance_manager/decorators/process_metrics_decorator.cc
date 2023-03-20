// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/process_metrics_decorator.h"

#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace performance_manager {

namespace {

// The process metrics refresh interval.
constexpr base::TimeDelta kMetricsRefreshInterval = base::Minutes(2);

}  // namespace

ProcessMetricsDecorator::ProcessMetricsDecorator() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}
ProcessMetricsDecorator::~ProcessMetricsDecorator() = default;

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
  DCHECK(decorator);
  decorator->OnMetricsInterestTokenCreated();
}

ProcessMetricsDecorator::ScopedMetricsInterestTokenImpl::
    ~ScopedMetricsInterestTokenImpl() {
  auto* decorator = graph_->GetRegisteredObjectAs<ProcessMetricsDecorator>();
  // This could be destroyed after removing the decorator from the graph.
  if (decorator)
    decorator->OnMetricsInterestTokenReleased();
}

// static
std::unique_ptr<ProcessMetricsDecorator::ScopedMetricsInterestToken>
ProcessMetricsDecorator::RegisterInterestForProcessMetrics(Graph* graph) {
  return std::make_unique<ScopedMetricsInterestTokenImpl>(graph);
}

void ProcessMetricsDecorator::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph_ = graph;
  graph_->RegisterObject(this);
}

void ProcessMetricsDecorator::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopTimer();
  graph_->UnregisterObject(this);
  graph_ = nullptr;
}

void ProcessMetricsDecorator::OnMetricsInterestTokenCreated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++metrics_interest_token_count_;
  if (metrics_interest_token_count_ == 1) {
    // Take the first metrics measurement immediately.
    CHECK(!refresh_timer_.IsRunning());
    RefreshMetrics();
  }
}

void ProcessMetricsDecorator::OnMetricsInterestTokenReleased() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(metrics_interest_token_count_, 0U);
  --metrics_interest_token_count_;
  if (metrics_interest_token_count_ == 0) {
    StopTimer();
  }
}

void ProcessMetricsDecorator::StartTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  refresh_timer_.Start(
      FROM_HERE, kMetricsRefreshInterval,
      base::BindRepeating(&ProcessMetricsDecorator::RefreshMetrics,
                          base::Unretained(this)));
}

void ProcessMetricsDecorator::StopTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  refresh_timer_.Stop();
}

void ProcessMetricsDecorator::RefreshMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestProcessesMemoryMetrics(base::BindOnce(
      &ProcessMetricsDecorator::DidGetMemoryUsage, weak_factory_.GetWeakPtr()));
}

void ProcessMetricsDecorator::RequestProcessesMemoryMetrics(
    memory_instrumentation::MemoryInstrumentation::RequestGlobalDumpCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(sebmarchand): Use the synchronous calls once they are available.
  auto* mem_instrumentation =
      memory_instrumentation::MemoryInstrumentation::GetInstance();
  // The memory instrumentation service is not available in unit tests unless
  // explicitly created.
  if (mem_instrumentation) {
    mem_instrumentation->RequestPrivateMemoryFootprint(base::kNullProcessId,
                                                       std::move(callback));
  }
}

void ProcessMetricsDecorator::DidGetMemoryUsage(
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> process_dumps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(process_dumps);
  // Check if the decorator was removed from the graph while the request was
  // being handled.
  if (!graph_) {
    return;
  }

  // Always schedule the next measurement, even if this one didn't succeed.
  if (metrics_interest_token_count_) {
    StartTimer();
  }

  if (!success) {
    return;
  }

  auto* graph_impl = GraphImpl::FromGraph(graph_);

  // Refresh the process nodes with the data contained in |process_dumps|.
  // Processes for which we don't receive any data will retain the previously
  // set value.
  // TODO(sebmarchand): Check if we should set the data to 0 instead, or add a
  // timestamp to the data.
  for (const auto& process_dump_iter : process_dumps->process_dumps()) {
    // Check if there's a process node associated with this PID.
    auto* process_node =
        graph_impl->GetProcessNodeByPid(process_dump_iter.pid());
    if (!process_node) {
      continue;
    }

    uint64_t process_pmf = process_dump_iter.os_dump().private_footprint_kb;
    process_node->set_private_footprint_kb(process_pmf);

    uint64_t process_rss = process_dump_iter.os_dump().resident_set_kb;
    process_node->set_resident_set_kb(process_rss);

    // Now attribute the RSS and PMF of the process to its frames and workers.
    // Only renderers can host frames and workers.
    if (process_node->process_type() != content::PROCESS_TYPE_RENDERER) {
      continue;
    }

    size_t frame_and_worker_node_count = process_node->frame_nodes().size() +
                                         process_node->worker_nodes().size();
    if (frame_and_worker_node_count > 0) {
      // For now, equally split the process' RSS and PMF among all of its frames
      // and workers.
      // TODO(anthonyvd): This should be more sophisticated, like attributing
      // the RSS and PMF to each node proportionally to its V8 heap size.
      uint64_t rss_estimate_part = process_rss / frame_and_worker_node_count;
      uint64_t pmf_estimate_part = process_pmf / frame_and_worker_node_count;

      for (FrameNodeImpl* frame : process_node->frame_nodes()) {
        frame->SetResidentSetKbEstimate(rss_estimate_part);
        frame->SetPrivateFootprintKbEstimate(pmf_estimate_part);
      }

      for (WorkerNodeImpl* worker : process_node->worker_nodes()) {
        worker->SetResidentSetKbEstimate(rss_estimate_part);
        worker->SetPrivateFootprintKbEstimate(pmf_estimate_part);
      }
    }
  }

  GraphImpl::FromGraph(graph_)
      ->GetSystemNodeImpl()
      ->OnProcessMemoryMetricsAvailable();
}

}  // namespace performance_manager
