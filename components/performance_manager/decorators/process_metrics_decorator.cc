// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/process_metrics_decorator.h"

#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"
#include "components/performance_manager/resource_attribution/attribution_impl_helpers.h"
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
  CHECK_EQ(state_, State::kStopped);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(
      this, "ProcessMetricsDecorator");
}

void ProcessMetricsDecorator::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopTimer();
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  CHECK_EQ(state_, State::kStopped);
}

base::Value::Dict ProcessMetricsDecorator::DescribeSystemNodeData(
    const SystemNode*) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict ret;
  ret.Set("interest_token_count",
          base::NumberToString(metrics_interest_token_count_));
  ret.Set("time_to_next_refresh",
          TimeDeltaFromNowToValue(refresh_timer_.desired_run_time()));
  ret.Set("state", static_cast<int>(state_));
  return ret;
}

bool ProcessMetricsDecorator::IsTimerRunningForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return refresh_timer_.IsRunning();
}

base::TimeDelta ProcessMetricsDecorator::GetTimerDelayForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return refresh_timer_.GetCurrentDelay();
}

void ProcessMetricsDecorator::OnMetricsInterestTokenCreated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++metrics_interest_token_count_;
  if (metrics_interest_token_count_ == 1) {
    // Take the first metrics measurement immediately.
    CHECK_EQ(state_, State::kStopped);
    RefreshMetrics();
  }
  CHECK_NE(state_, State::kStopped);
}

void ProcessMetricsDecorator::OnMetricsInterestTokenReleased() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(state_, State::kStopped);
  CHECK_GT(metrics_interest_token_count_, 0U);
  --metrics_interest_token_count_;
  if (metrics_interest_token_count_ == 0) {
    StopTimer();
  }
}

void ProcessMetricsDecorator::RequestImmediateMetrics(
    base::OnceClosure on_metrics_received) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Callers should have an interest token before calling
  // RequestImmediateMetrics().
  CHECK_GT(metrics_interest_token_count_, 0U);
  if (state_ == State::kWaitingForResponse) {
    // A measurement is already being taken and will be available immediately.
    return;
  }
  if (!last_memory_refresh_time_.is_null() &&
      base::TimeTicks::Now() - last_memory_refresh_time_ <
          kMinImmediateRefreshDelay) {
    // The most recent measurement is fresh enough.
    return;
  }

  // Stop the timer so the next metrics sample will be 2 minutes after this to
  // avoid re-sampling shortly after updating the metrics.
  StopTimer();

  // Asynchronously update memory metrics.
  state_ = State::kWaitingForResponse;
  RequestProcessesMemoryMetrics(
      /*immediate_request=*/true,
      base::BindOnce(&ProcessMetricsDecorator::DidGetMemoryUsage,
                     weak_factory_.GetWeakPtr())
          .Then(std::move(on_metrics_received)));
}

void ProcessMetricsDecorator::StartTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The timer should only be started immediately after processing the response
  // to a refresh, to start the countdown to the next refresh.
  CHECK_EQ(state_, State::kWaitingForResponse);
  CHECK_GT(metrics_interest_token_count_, 0U);
  CHECK(!refresh_timer_.IsRunning());
  refresh_timer_.Start(
      FROM_HERE, kMetricsRefreshInterval,
      base::BindRepeating(&ProcessMetricsDecorator::RefreshMetrics,
                          base::Unretained(this)));
  state_ = State::kWaitingForRefresh;
}

void ProcessMetricsDecorator::StopTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  refresh_timer_.Stop();
  state_ = State::kStopped;
}

void ProcessMetricsDecorator::RefreshMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This is either triggered by `refresh_timer_` firing or called directly when
  // the first interest token is created.
  CHECK(state_ == State::kWaitingForRefresh || state_ == State::kStopped);
  CHECK_GT(metrics_interest_token_count_, 0U);
  CHECK(!refresh_timer_.IsRunning());

  // Asynchronously update memory metrics.
  state_ = State::kWaitingForResponse;
  RequestProcessesMemoryMetrics(
      /*immediate_request=*/false,
      base::BindOnce(&ProcessMetricsDecorator::DidGetMemoryUsage,
                     weak_factory_.GetWeakPtr()));
}

void ProcessMetricsDecorator::RequestProcessesMemoryMetrics(
    bool immediate_request,
    ProcessMemoryDumpCallback callback) {
  // This function is replaced during testing, so all state checking should
  // be done in the calling function, RefreshMetrics().
  auto* mem_instrumentation =
      memory_instrumentation::MemoryInstrumentation::GetInstance();
  // The memory instrumentation service is not available in unit tests unless
  // explicitly created.
  if (mem_instrumentation) {
    mem_instrumentation->RequestPrivateMemoryFootprint(
        base::kNullProcessId,
        base::BindOnce(std::move(callback), immediate_request));
  } else {
    std::move(callback).Run(immediate_request, false, nullptr);
  }
}

void ProcessMetricsDecorator::DidGetMemoryUsage(
    bool immediate_request,
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> process_dumps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Check if the updates were stopped while the request was being handled.
  if (state_ == State::kStopped) {
    return;
  }

  // Always schedule the next measurement, even if this one didn't succeed.
  CHECK_EQ(state_, State::kWaitingForResponse);
  StartTimer();

  if (!success) {
    return;
  }

  if (immediate_request) {
    last_memory_refresh_time_ = base::TimeTicks::Now();
  }

  CHECK(process_dumps);
  auto* graph_impl = GraphImpl::FromGraph(GetOwningGraph());

  // Refresh the process nodes with the data contained in |process_dumps|.
  // Processes for which we don't receive any data will retain the previously
  // set value.
  // TODO(sebmarchand): Check if we should set the data to 0 instead, or add a
  // timestamp to the data.
  for (const auto& process_dump_iter : process_dumps->process_dumps()) {
    // Check if there's a process node associated with this PID.
    ProcessNodeImpl* process_node =
        graph_impl->GetProcessNodeByPid(process_dump_iter.pid());
    if (!process_node) {
      continue;
    }

    // Equally split the RSS and PMF of the process to its frames and workers.
    // TODO(anthonyvd): This should be more sophisticated, like attributing the
    // RSS and PMF to each node proportionally to its V8 heap size.
    uint64_t process_rss = process_dump_iter.os_dump().resident_set_kb;
    process_node->set_resident_set_kb(process_rss);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
    process_node->set_private_swap_kb(
        process_dump_iter.os_dump().private_footprint_swap_kb);
#endif
    resource_attribution::SplitResourceAmongFrameAndWorkerImpls(
        process_rss, process_node, &FrameNodeImpl::SetResidentSetKbEstimate,
        &WorkerNodeImpl::SetResidentSetKbEstimate);
    uint64_t process_pmf = process_dump_iter.os_dump().private_footprint_kb;
    process_node->set_private_footprint_kb(process_pmf);
    resource_attribution::SplitResourceAmongFrameAndWorkerImpls(
        process_pmf, process_node,
        &FrameNodeImpl::SetPrivateFootprintKbEstimate,
        &WorkerNodeImpl::SetPrivateFootprintKbEstimate);
  }

  GraphImpl::FromGraph(GetOwningGraph())
      ->GetSystemNodeImpl()
      ->OnProcessMemoryMetricsAvailable();
}

}  // namespace performance_manager
