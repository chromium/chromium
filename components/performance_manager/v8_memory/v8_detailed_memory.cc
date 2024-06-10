// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory_any_seq.h"
#include "components/performance_manager/v8_memory/v8_detailed_memory_decorator.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/process_type.h"

namespace performance_manager {

namespace v8_memory {

namespace {

#if DCHECK_IS_ON()
// May only be used from the performance manager sequence.
bool g_test_eager_measurement_requests_enabled = false;
#endif

}  // namespace

namespace internal {

void SetEagerMemoryMeasurementEnabledForTesting(bool enabled) {
#if DCHECK_IS_ON()
  g_test_eager_measurement_requests_enabled = enabled;
#endif
}

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// V8DetailedMemoryRequest

V8DetailedMemoryRequest::V8DetailedMemoryRequest(
    const base::TimeDelta& min_time_between_requests,
    MeasurementMode mode)
    : min_time_between_requests_(min_time_between_requests), mode_(mode) {
#if DCHECK_IS_ON()
  DCHECK_GT(min_time_between_requests_, base::TimeDelta());
  DCHECK(!min_time_between_requests_.is_inf());
  DCHECK(mode != MeasurementMode::kEagerForTesting ||
         g_test_eager_measurement_requests_enabled);
#endif
}

V8DetailedMemoryRequest::V8DetailedMemoryRequest(
    const base::TimeDelta& min_time_between_requests,
    Graph* graph)
    : V8DetailedMemoryRequest(min_time_between_requests,
                              MeasurementMode::kDefault) {
  StartMeasurement(graph);
}

V8DetailedMemoryRequest::V8DetailedMemoryRequest(
    const base::TimeDelta& min_time_between_requests,
    MeasurementMode mode,
    Graph* graph)
    : V8DetailedMemoryRequest(min_time_between_requests, mode) {
  StartMeasurement(graph);
}

// This constructor is called from the V8DetailedMemoryRequestAnySeq's
// sequence.
V8DetailedMemoryRequest::V8DetailedMemoryRequest(
    base::PassKey<V8DetailedMemoryRequestAnySeq>,
    const base::TimeDelta& min_time_between_requests,
    MeasurementMode mode,
    std::optional<base::WeakPtr<ProcessNode>> process_to_measure,
    base::WeakPtr<V8DetailedMemoryRequestAnySeq> off_sequence_request)
    : V8DetailedMemoryRequest(min_time_between_requests, mode) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  off_sequence_request_ = std::move(off_sequence_request);
  off_sequence_request_sequence_ =
      base::SequencedTaskRunner::GetCurrentDefault();
  // Unretained is safe since |this| will be destroyed on the graph sequence
  // from an async task posted after this.
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(&V8DetailedMemoryRequest::StartMeasurementFromOffSequence,
                     base::Unretained(this), std::move(process_to_measure)));
}

V8DetailedMemoryRequest::V8DetailedMemoryRequest(
    base::PassKey<V8DetailedMemoryRequestOneShot>,
    MeasurementMode mode,
    base::OnceClosure on_owner_unregistered_closure)
    : min_time_between_requests_(base::TimeDelta()),
      mode_(mode),
      on_owner_unregistered_closure_(std::move(on_owner_unregistered_closure)) {
  // Do not forward to the standard constructor because it disallows the empty
  // TimeDelta.
}

V8DetailedMemoryRequest::~V8DetailedMemoryRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (decorator_)
    decorator_->RemoveMeasurementRequest(
        base::PassKey<V8DetailedMemoryRequest>(), this);
  // TODO(crbug.com/40130181): Delete the decorator and its NodeAttachedData
  // when the last request is destroyed. Make sure this doesn't mess up any
  // measurement that's already in progress.
}

void V8DetailedMemoryRequest::StartMeasurement(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StartMeasurementImpl(graph, nullptr);
}

void V8DetailedMemoryRequest::StartMeasurementForProcess(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(process_node);
  DCHECK_EQ(process_node->GetProcessType(), content::PROCESS_TYPE_RENDERER);
  StartMeasurementImpl(process_node->GetGraph(), process_node);
}

void V8DetailedMemoryRequest::AddObserver(V8DetailedMemoryObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void V8DetailedMemoryRequest::RemoveObserver(
    V8DetailedMemoryObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void V8DetailedMemoryRequest::OnOwnerUnregistered(
    base::PassKey<V8DetailedMemoryRequestQueue>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  decorator_ = nullptr;
  if (on_owner_unregistered_closure_)
    std::move(on_owner_unregistered_closure_).Run();
}

void V8DetailedMemoryRequest::NotifyObserversOnMeasurementAvailable(
    base::PassKey<V8DetailedMemoryRequestQueue>,
    const ProcessNode* process_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* process_data =
      V8DetailedMemoryProcessData::ForProcessNode(process_node);
  DCHECK(process_data);

  // If this request was made from off-sequence, notify its off-sequence
  // observers with a copy of the process and frame data.
  if (off_sequence_request_.MaybeValid()) {
    using FrameAndData = std::pair<content::GlobalRenderFrameHostId,
                                   V8DetailedMemoryExecutionContextData>;
    std::vector<FrameAndData> all_frame_data;
    for (const FrameNode* frame_node : process_node->GetFrameNodes()) {
      const auto* frame_data =
          V8DetailedMemoryExecutionContextData::ForFrameNode(frame_node);
      if (frame_data) {
        all_frame_data.push_back(std::make_pair(
            frame_node->GetRenderFrameHostProxy().global_frame_routing_id(),
            *frame_data));
      }
    }
    off_sequence_request_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&V8DetailedMemoryRequestAnySeq::
                           NotifyObserversOnMeasurementAvailable,
                       off_sequence_request_,
                       base::PassKey<V8DetailedMemoryRequest>(),
                       process_node->GetRenderProcessHostId(), *process_data,
                       V8DetailedMemoryObserverAnySeq::FrameDataMap(
                           std::move(all_frame_data))));
  }

  // The observer could delete the request so this must be the last thing in
  // the function.
  for (V8DetailedMemoryObserver& observer : observers_)
    observer.OnV8MemoryMeasurementAvailable(process_node, process_data);
}

void V8DetailedMemoryRequest::StartMeasurementFromOffSequence(
    std::optional<base::WeakPtr<ProcessNode>> process_to_measure,
    Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!process_to_measure) {
    // No process was given so measure all renderers in the graph.
    StartMeasurement(graph);
  } else if (!process_to_measure.value()) {
    // V8DetailedMemoryRequestAnySeq was called with a process ID that wasn't
    // found in the graph, or has already been destroyed. Do nothing.
  } else {
    DCHECK_EQ(graph, process_to_measure.value()->GetGraph());
    StartMeasurementForProcess(process_to_measure.value().get());
  }
}

void V8DetailedMemoryRequest::StartMeasurementImpl(
    Graph* graph,
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(nullptr, decorator_);
  DCHECK(!process_node || graph == process_node->GetGraph());
  decorator_ = V8DetailedMemoryDecorator::GetFromGraph(graph);
  if (!decorator_) {
    // Create the decorator when the first measurement starts.
    auto decorator_ptr = std::make_unique<V8DetailedMemoryDecorator>();
    decorator_ = decorator_ptr.get();
    graph->PassToGraph(std::move(decorator_ptr));
  }

  decorator_->AddMeasurementRequest(base::PassKey<V8DetailedMemoryRequest>(),
                                    this, process_node);
}

////////////////////////////////////////////////////////////////////////////////
// V8DetailedMemoryRequestOneShot

V8DetailedMemoryRequestOneShot::V8DetailedMemoryRequestOneShot(
    MeasurementMode mode)
    : mode_(mode) {
  InitializeRequest();
}

V8DetailedMemoryRequestOneShot::V8DetailedMemoryRequestOneShot(
    const ProcessNode* process,
    MeasurementCallback callback,
    MeasurementMode mode)
    : mode_(mode) {
  InitializeRequest();
  StartMeasurement(process, std::move(callback));
}

V8DetailedMemoryRequestOneShot::~V8DetailedMemoryRequestOneShot() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DeleteRequest();
}

void V8DetailedMemoryRequestOneShot::StartMeasurement(
    const ProcessNode* process,
    MeasurementCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request_);
  DCHECK(process);
  DCHECK_EQ(process->GetProcessType(), content::PROCESS_TYPE_RENDERER);
#if DCHECK_IS_ON()
  process_ = process;
#endif

  callback_ = std::move(callback);
  request_->StartMeasurementForProcess(process);
}

void V8DetailedMemoryRequestOneShot::OnV8MemoryMeasurementAvailable(
    const ProcessNode* process_node,
    const V8DetailedMemoryProcessData* process_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK_EQ(process_node, process_);
#endif

  // Don't send another request now that a response has been received.
  DeleteRequest();

  std::move(callback_).Run(process_node, process_data);
}

// This constructor is called from the V8DetailedMemoryRequestOneShotAnySeq's
// sequence.
V8DetailedMemoryRequestOneShot::V8DetailedMemoryRequestOneShot(
    base::PassKey<V8DetailedMemoryRequestOneShotAnySeq>,
    base::WeakPtr<ProcessNode> process,
    MeasurementCallback callback,
    MeasurementMode mode)
    : mode_(mode) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // Unretained is safe since |this| will be destroyed on the graph sequence
  // from an async task posted after this.
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(&V8DetailedMemoryRequestOneShot::InitializeRequest,
                     base::Unretained(this)));
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          &V8DetailedMemoryRequestOneShot::StartMeasurementFromOffSequence,
          base::Unretained(this), std::move(process), std::move(callback)));
}

void V8DetailedMemoryRequestOneShot::InitializeRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_ = std::make_unique<V8DetailedMemoryRequest>(
      base::PassKey<V8DetailedMemoryRequestOneShot>(), mode_,
      base::BindOnce(&V8DetailedMemoryRequestOneShot::OnOwnerUnregistered,
                     // Unretained is safe because |this| owns the request
                     // object that will invoke the closure.
                     base::Unretained(this)));
  request_->AddObserver(this);
}

void V8DetailedMemoryRequestOneShot::StartMeasurementFromOffSequence(
    base::WeakPtr<ProcessNode> process,
    MeasurementCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (process)
    StartMeasurement(process.get(), std::move(callback));
}

void V8DetailedMemoryRequestOneShot::DeleteRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (request_)
    request_->RemoveObserver(this);
  request_.reset();
}

void V8DetailedMemoryRequestOneShot::OnOwnerUnregistered() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // No results will arrive so clean up the request and callback. This frees
  // any resources that were owned by the callback.
  DeleteRequest();
  std::move(callback_).Reset();
}

////////////////////////////////////////////////////////////////////////////////
// V8DetailedMemoryExecutionContextData

V8DetailedMemoryExecutionContextData::V8DetailedMemoryExecutionContextData() =
    default;
V8DetailedMemoryExecutionContextData::~V8DetailedMemoryExecutionContextData() =
    default;

V8DetailedMemoryExecutionContextData::V8DetailedMemoryExecutionContextData(
    const V8DetailedMemoryExecutionContextData&) = default;
V8DetailedMemoryExecutionContextData::V8DetailedMemoryExecutionContextData(
    V8DetailedMemoryExecutionContextData&&) = default;
V8DetailedMemoryExecutionContextData&
V8DetailedMemoryExecutionContextData::operator=(
    const V8DetailedMemoryExecutionContextData&) = default;
V8DetailedMemoryExecutionContextData&
V8DetailedMemoryExecutionContextData::operator=(
    V8DetailedMemoryExecutionContextData&&) = default;

// static
const V8DetailedMemoryExecutionContextData*
V8DetailedMemoryExecutionContextData::ForFrameNode(const FrameNode* node) {
  return V8DetailedMemoryDecorator::GetExecutionContextData(node);
}

// static
const V8DetailedMemoryExecutionContextData*
V8DetailedMemoryExecutionContextData::ForWorkerNode(const WorkerNode* node) {
  return V8DetailedMemoryDecorator::GetExecutionContextData(node);
}

// static
const V8DetailedMemoryExecutionContextData*
V8DetailedMemoryExecutionContextData::ForExecutionContext(
    const execution_context::ExecutionContext* ec) {
  return V8DetailedMemoryDecorator::GetExecutionContextData(ec);
}

V8DetailedMemoryExecutionContextData*
V8DetailedMemoryExecutionContextData::CreateForTesting(const FrameNode* node) {
  return V8DetailedMemoryDecorator::CreateExecutionContextDataForTesting(node);
}

V8DetailedMemoryExecutionContextData*
V8DetailedMemoryExecutionContextData::CreateForTesting(const WorkerNode* node) {
  return V8DetailedMemoryDecorator::CreateExecutionContextDataForTesting(node);
}

////////////////////////////////////////////////////////////////////////////////
// V8DetailedMemoryProcessData

const V8DetailedMemoryProcessData* V8DetailedMemoryProcessData::ForProcessNode(
    const ProcessNode* node) {
  return V8DetailedMemoryDecorator::GetProcessData(node);
}

V8DetailedMemoryProcessData* V8DetailedMemoryProcessData::GetOrCreateForTesting(
    const ProcessNode* process_node) {
  return V8DetailedMemoryDecorator::CreateProcessDataForTesting(process_node);
}

}  // namespace v8_memory

}  // namespace performance_manager
