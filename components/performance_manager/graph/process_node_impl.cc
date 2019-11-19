// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/process_node_impl.h"

#include <utility>

#include "base/logging.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"

namespace performance_manager {

ProcessNodeImpl::ProcessNodeImpl(GraphImpl* graph,
                                 RenderProcessHostProxy render_process_proxy)
    : TypedNodeBase(graph),
      render_process_host_proxy_(std::move(render_process_proxy)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProcessNodeImpl::~ProcessNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProcessNodeImpl::SetCPUUsage(double cpu_usage) {
  cpu_usage_ = cpu_usage;
}

void ProcessNodeImpl::Bind(
    mojo::PendingReceiver<mojom::ProcessCoordinationUnit> receiver) {
  // A RenderProcessHost can be reused if the backing process suddenly dies, in
  // which case we will receive a new receiver from the newly spawned process.
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void ProcessNodeImpl::SetExpectedTaskQueueingDuration(
    base::TimeDelta duration) {
  expected_task_queueing_duration_.SetAndNotify(this, duration);
}

void ProcessNodeImpl::SetMainThreadTaskLoadIsLow(
    bool main_thread_task_load_is_low) {
  main_thread_task_load_is_low_.SetAndMaybeNotify(this,
                                                  main_thread_task_load_is_low);
}

void ProcessNodeImpl::SetProcessExitStatus(int32_t exit_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This may occur as the first event seen in the case where the process
  // fails to start or suffers a startup crash.
  exit_status_ = exit_status;

  // Close the process handle to kill the zombie.
  process_.SetAndNotify(this, base::Process());

  // No more message should be received from this process.
  receiver_.reset();
}

void ProcessNodeImpl::SetProcess(base::Process process,
                                 base::Time launch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(process.IsValid());
  // Either this is the initial process associated with this process node,
  // or it's a subsequent process. In the latter case, there must have been
  // an exit status associated with the previous process.
  DCHECK(!process_.value().IsValid() || exit_status_.has_value());

  base::ProcessId pid = process.Pid();
  SetProcessImpl(std::move(process), pid, launch_time);
}

const base::flat_set<FrameNodeImpl*>& ProcessNodeImpl::frame_nodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return frame_nodes_;
}

PageNodeImpl* ProcessNodeImpl::GetPageNodeIfExclusive() const {
  PageNodeImpl* page_node = nullptr;
  for (auto* frame_node : frame_nodes_) {
    if (!page_node)
      page_node = frame_node->page_node();
    if (page_node != frame_node->page_node())
      return nullptr;
  }
  return page_node;
}

int ProcessNodeImpl::GetRenderProcessId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return render_process_host_proxy_.render_process_host_id();
}

void ProcessNodeImpl::AddFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool inserted = frame_nodes_.insert(frame_node).second;
  DCHECK(inserted);
}

void ProcessNodeImpl::RemoveFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(frame_nodes_, frame_node));
  frame_nodes_.erase(frame_node);
}

void ProcessNodeImpl::AddWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool inserted = worker_nodes_.insert(worker_node).second;
  DCHECK(inserted);
}

void ProcessNodeImpl::RemoveWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(worker_nodes_, worker_node));
  worker_nodes_.erase(worker_node);
}

void ProcessNodeImpl::set_priority(base::TaskPriority priority) {
  priority_.SetAndMaybeNotify(this, priority);
}

void ProcessNodeImpl::SetProcessImpl(base::Process process,
                                     base::ProcessId new_pid,
                                     base::Time launch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  graph()->BeforeProcessPidChange(this, new_pid);

  // Clear the exit status for the previous process (if any).
  exit_status_.reset();

  // Also clear the measurement data (if any), as it references the previous
  // process.
  private_footprint_kb_ = 0;
  resident_set_kb_ = 0;
  cumulative_cpu_usage_ = base::TimeDelta();

  process_id_ = new_pid;
  launch_time_ = launch_time;

  // Set the process variable last, as it will fire the notification.
  process_.SetAndNotify(this, std::move(process));
}

base::ProcessId ProcessNodeImpl::GetProcessId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_id();
}

const base::Process& ProcessNodeImpl::GetProcess() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process();
}

base::Time ProcessNodeImpl::GetLaunchTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return launch_time();
}

base::Optional<int32_t> ProcessNodeImpl::GetExitStatus() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return exit_status();
}

void ProcessNodeImpl::VisitFrameNodes(const FrameNodeVisitor& visitor) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto* frame_impl : frame_nodes()) {
    const FrameNode* frame = frame_impl;
    if (!visitor.Run(frame))
      return;
  }
}

base::flat_set<const FrameNode*> ProcessNodeImpl::GetFrameNodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<const FrameNode*> frames;
  const base::flat_set<FrameNodeImpl*>& frame_impls = frame_nodes();
  for (auto* frame_impl : frame_impls) {
    const FrameNode* frame = frame_impl;
    frames.insert(frame);
  }
  return frames;
}

base::TimeDelta ProcessNodeImpl::GetExpectedTaskQueueingDuration() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return expected_task_queueing_duration();
}

bool ProcessNodeImpl::GetMainThreadTaskLoadIsLow() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return main_thread_task_load_is_low();
}

double ProcessNodeImpl::GetCpuUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cpu_usage();
}

base::TimeDelta ProcessNodeImpl::GetCumulativeCpuUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cumulative_cpu_usage();
}

uint64_t ProcessNodeImpl::GetPrivateFootprintKb() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return private_footprint_kb();
}

uint64_t ProcessNodeImpl::GetResidentSetKb() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resident_set_kb();
}

const RenderProcessHostProxy& ProcessNodeImpl::GetRenderProcessHostProxy()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return render_process_host_proxy();
}

base::TaskPriority ProcessNodeImpl::GetPriority() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return priority();
}

void ProcessNodeImpl::OnAllFramesInProcessFrozen() {
  for (auto* observer : GetObservers())
    observer->OnAllFramesInProcessFrozen(this);
}

void ProcessNodeImpl::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NodeBase::LeaveGraph();

  // Make as if we're transitioning to the null PID before we die to clear this
  // instance from the PID map.
  if (process_id_ != base::kNullProcessId)
    graph()->BeforeProcessPidChange(this, base::kNullProcessId);

  // All child frames should have been removed before the process is removed.
  DCHECK(frame_nodes_.empty());
}

}  // namespace performance_manager
