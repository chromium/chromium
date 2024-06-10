// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/v8_memory/v8_detailed_memory_any_seq.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

namespace v8_memory {

////////////////////////////////////////////////////////////////////////////////
// V8DetailedMemoryRequestAnySeq

V8DetailedMemoryRequestAnySeq::V8DetailedMemoryRequestAnySeq(
    const base::TimeDelta& min_time_between_requests,
    MeasurementMode mode,
    std::optional<RenderProcessHostId> process_to_measure) {
  std::optional<base::WeakPtr<ProcessNode>> process_node;
  if (process_to_measure) {
    // GetProcessNodeForRenderProcessHostId must be called from the UI thread.
    auto ui_task_runner = content::GetUIThreadTaskRunner({});
    if (!ui_task_runner->RunsTasksInCurrentSequence()) {
      ui_task_runner->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(
              &PerformanceManager::GetProcessNodeForRenderProcessHostId,
              process_to_measure.value()),
          base::BindOnce(
              &V8DetailedMemoryRequestAnySeq::InitializeWrappedRequest,
              weak_factory_.GetWeakPtr(), min_time_between_requests, mode));
      return;
    }
    process_node = PerformanceManager::GetProcessNodeForRenderProcessHostId(
        process_to_measure.value());
  }
  InitializeWrappedRequest(min_time_between_requests, mode,
                           std::move(process_node));
}

V8DetailedMemoryRequestAnySeq::~V8DetailedMemoryRequestAnySeq() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<V8DetailedMemoryRequest> request) {
                       request.reset();
                     },
                     std::move(request_)));
}

bool V8DetailedMemoryRequestAnySeq::HasObserver(
    V8DetailedMemoryObserverAnySeq* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return observers_.HasObserver(observer);
}

void V8DetailedMemoryRequestAnySeq::AddObserver(
    V8DetailedMemoryObserverAnySeq* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void V8DetailedMemoryRequestAnySeq::RemoveObserver(
    V8DetailedMemoryObserverAnySeq* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void V8DetailedMemoryRequestAnySeq::NotifyObserversOnMeasurementAvailable(
    base::PassKey<V8DetailedMemoryRequest>,
    RenderProcessHostId render_process_host_id,
    const V8DetailedMemoryProcessData& process_data,
    const V8DetailedMemoryObserverAnySeq::FrameDataMap& frame_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (V8DetailedMemoryObserverAnySeq& observer : observers_)
    observer.OnV8MemoryMeasurementAvailable(render_process_host_id,
                                            process_data, frame_data);
}

void V8DetailedMemoryRequestAnySeq::InitializeWrappedRequest(
    const base::TimeDelta& min_time_between_requests,
    MeasurementMode mode,
    std::optional<base::WeakPtr<ProcessNode>> process_to_measure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // After construction the V8DetailedMemoryRequest must only be accessed on
  // the graph sequence.
  request_ = std::make_unique<V8DetailedMemoryRequest>(
      base::PassKey<V8DetailedMemoryRequestAnySeq>(), min_time_between_requests,
      mode, std::move(process_to_measure), weak_factory_.GetWeakPtr());
}

////////////////////////////////////////////////////////////////////////////////
// V8DetailedMemoryRequestOneShotAnySeq

V8DetailedMemoryRequestOneShotAnySeq::V8DetailedMemoryRequestOneShotAnySeq(
    MeasurementMode mode)
    : mode_(mode) {}

V8DetailedMemoryRequestOneShotAnySeq::V8DetailedMemoryRequestOneShotAnySeq(
    RenderProcessHostId process_id,
    MeasurementCallback callback,
    MeasurementMode mode)
    : mode_(mode) {
  StartMeasurement(process_id, std::move(callback));
}

V8DetailedMemoryRequestOneShotAnySeq::~V8DetailedMemoryRequestOneShotAnySeq() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<V8DetailedMemoryRequestOneShot> request) {
            request.reset();
          },
          std::move(request_)));
}

void V8DetailedMemoryRequestOneShotAnySeq::StartMeasurement(
    RenderProcessHostId process_id,
    MeasurementCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // GetProcessNodeForRenderProcessHostId must be called from the UI thread.
  auto ui_task_runner = content::GetUIThreadTaskRunner({});
  if (ui_task_runner->RunsTasksInCurrentSequence()) {
    InitializeWrappedRequest(
        std::move(callback), mode_,
        PerformanceManager::GetProcessNodeForRenderProcessHostId(process_id));
  } else {
    ui_task_runner->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            &PerformanceManager::GetProcessNodeForRenderProcessHostId,
            process_id),
        base::BindOnce(
            &V8DetailedMemoryRequestOneShotAnySeq::InitializeWrappedRequest,
            weak_factory_.GetWeakPtr(), std::move(callback), mode_));
  }
}

void V8DetailedMemoryRequestOneShotAnySeq::InitializeWrappedRequest(
    MeasurementCallback callback,
    MeasurementMode mode,
    base::WeakPtr<ProcessNode> process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass ownership of |callback| to a wrapper, |wrapped_callback|, that will
  // be owned by the wrapped request. The wrapper will be invoked and destroyed
  // on the PM sequence. However, |callback| must be both called and destroyed
  // on this sequence, so indirect all accesses to it through SequenceBound.
  auto wrapped_callback = base::BindOnce(
      &V8DetailedMemoryRequestOneShotAnySeq::OnMeasurementAvailable,
      base::SequenceBound<MeasurementCallback>(
          base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback)));

  // After construction the V8DetailedMemoryRequest must only be accessed on
  // the graph sequence.
  request_ = std::make_unique<V8DetailedMemoryRequestOneShot>(
      base::PassKey<V8DetailedMemoryRequestOneShotAnySeq>(),
      std::move(process_node), std::move(wrapped_callback), mode);
}

// static
void V8DetailedMemoryRequestOneShotAnySeq::OnMeasurementAvailable(
    base::SequenceBound<MeasurementCallback> sequence_bound_callback,
    const ProcessNode* process_node,
    const V8DetailedMemoryProcessData* process_data) {
  DCHECK(process_node);
  DCHECK_ON_GRAPH_SEQUENCE(process_node->GetGraph());

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

  sequence_bound_callback.PostTaskWithThisObject(
      base::BindOnce(
          [](RenderProcessHostId process_id,
             const V8DetailedMemoryProcessData& process_data,
             const FrameDataMap& frame_data, MeasurementCallback* callback) {
            std::move(*callback).Run(process_id, process_data, frame_data);
          },
          process_node->GetRenderProcessHostId(), *process_data,
          std::move(all_frame_data)));
}

}  // namespace v8_memory

}  // namespace performance_manager
