// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/node_attached_data.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/properties.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace performance_manager {

class FrameNodeImpl;
class ProcessNodeImpl;
class WorkerNodeImpl;

// A process node follows the lifetime of a RenderProcessHost.
// It may reference zero or one processes at a time, but during its lifetime, it
// may reference more than one process. This can happen if the associated
// renderer crashes, and an associated frame is then reloaded or re-navigated.
// The state of the process node goes through:
// 1. Created, no PID.
// 2. Process started, have PID - in the case where the associated render
//    process fails to start, this state may not occur.
// 3. Process died or failed to start, have exit status.
// 4. Back to 2.
class ProcessNodeImpl
    : public PublicNodeImpl<ProcessNodeImpl, ProcessNode>,
      public TypedNodeBase<ProcessNodeImpl, ProcessNode, ProcessNodeObserver>,
      public mojom::ProcessCoordinationUnit {
 public:
  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kProcess; }

  ProcessNodeImpl(GraphImpl*, RenderProcessHostProxy render_process_proxy);

  ~ProcessNodeImpl() override;

  void Bind(mojo::PendingReceiver<mojom::ProcessCoordinationUnit> receiver);

  // mojom::ProcessCoordinationUnit implementation:
  void SetExpectedTaskQueueingDuration(base::TimeDelta duration) override;
  void SetMainThreadTaskLoadIsLow(bool main_thread_task_load_is_low) override;

  // CPU usage is expressed as the average percentage of cores occupied over the
  // last measurement interval. One core fully occupied would be 100, while two
  // cores at 5% each would be 10.
  void SetCPUUsage(double cpu_usage);
  void SetProcessExitStatus(int32_t exit_status);
  void SetProcess(base::Process process, base::Time launch_time);

  // Private implementation properties.
  void set_private_footprint_kb(uint64_t private_footprint_kb) {
    private_footprint_kb_ = private_footprint_kb;
  }
  uint64_t private_footprint_kb() const { return private_footprint_kb_; }
  void set_cumulative_cpu_usage(base::TimeDelta cumulative_cpu_usage) {
    cumulative_cpu_usage_ = cumulative_cpu_usage;
  }
  uint64_t resident_set_kb() const { return resident_set_kb_; }
  void set_resident_set_kb(uint64_t resident_set_kb) {
    resident_set_kb_ = resident_set_kb;
  }
  base::TimeDelta cumulative_cpu_usage() const { return cumulative_cpu_usage_; }

  const base::flat_set<FrameNodeImpl*>& frame_nodes() const;

  // Returns the render process id (equivalent to RenderProcessHost::GetID()),
  // or ChildProcessHost::kInvalidUniqueID if this is not a renderer.
  int GetRenderProcessId() const;

  // If this process is associated with only one page, returns that page.
  // Otherwise, returns nullptr.
  PageNodeImpl* GetPageNodeIfExclusive() const;

  // Use process_id() in preference to process().Pid(). It's always valid to
  // access, but will return kNullProcessId when the process is not valid. It
  // will also retain the process ID for a process that has exited.
  base::ProcessId process_id() const { return process_id_; }
  const base::Process& process() const { return process_.value(); }
  base::Time launch_time() const { return launch_time_; }
  base::Optional<int32_t> exit_status() const { return exit_status_; }

  base::TimeDelta expected_task_queueing_duration() const {
    return expected_task_queueing_duration_.value();
  }

  bool main_thread_task_load_is_low() const {
    return main_thread_task_load_is_low_.value();
  }

  const RenderProcessHostProxy& render_process_host_proxy() const {
    return render_process_host_proxy_;
  }

  base::TaskPriority priority() const { return priority_.value(); }

  double cpu_usage() const { return cpu_usage_; }

  // Add |frame_node| to this process.
  void AddFrame(FrameNodeImpl* frame_node);
  // Removes |frame_node| from the set of frames hosted by this process. Invoked
  // when the frame is removed from the graph.
  void RemoveFrame(FrameNodeImpl* frame_node);

  // Add |worker_node| to this process.
  void AddWorker(WorkerNodeImpl* worker_node);
  // Removes |worker_node| from the set of workers hosted by this process.
  // Invoked when the worker is removed from the graph.
  void RemoveWorker(WorkerNodeImpl* worker_node);

  void set_priority(base::TaskPriority priority);

  void OnAllFramesInProcessFrozenForTesting() { OnAllFramesInProcessFrozen(); }

 protected:
  void SetProcessImpl(base::Process process,
                      base::ProcessId process_id,
                      base::Time launch_time);

 private:
  friend class FrozenFrameAggregatorAccess;
  friend class ProcessMetricsDecoratorAccess;
  friend class ProcessPriorityAggregatorAccess;

  // ProcessNode implementation. These are private so that users of the impl use
  // the private getters rather than the public interface.
  base::ProcessId GetProcessId() const override;
  const base::Process& GetProcess() const override;
  base::Time GetLaunchTime() const override;
  base::Optional<int32_t> GetExitStatus() const override;
  void VisitFrameNodes(const FrameNodeVisitor& visitor) const override;
  base::flat_set<const FrameNode*> GetFrameNodes() const override;
  base::TimeDelta GetExpectedTaskQueueingDuration() const override;
  bool GetMainThreadTaskLoadIsLow() const override;
  double GetCpuUsage() const override;
  base::TimeDelta GetCumulativeCpuUsage() const override;
  uint64_t GetPrivateFootprintKb() const override;
  uint64_t GetResidentSetKb() const override;
  const RenderProcessHostProxy& GetRenderProcessHostProxy() const override;
  base::TaskPriority GetPriority() const override;

  void OnAllFramesInProcessFrozen();

  void LeaveGraph() override;

  mojo::Receiver<mojom::ProcessCoordinationUnit> receiver_{this};

  base::TimeDelta cumulative_cpu_usage_;
  uint64_t private_footprint_kb_ = 0u;
  uint64_t resident_set_kb_ = 0;

  base::ProcessId process_id_ = base::kNullProcessId;
  ObservedProperty::NotifiesAlways<
      base::Process,
      &ProcessNodeObserver::OnProcessLifetimeChange>
      process_;

  base::Time launch_time_;
  base::Optional<int32_t> exit_status_;

  const RenderProcessHostProxy render_process_host_proxy_;

  ObservedProperty::NotifiesAlways<
      base::TimeDelta,
      &ProcessNodeObserver::OnExpectedTaskQueueingDurationSample>
      expected_task_queueing_duration_;
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &ProcessNodeObserver::OnMainThreadTaskLoadIsLow>
      main_thread_task_load_is_low_{false};
  double cpu_usage_ = 0;

  // Process priority information. This is aggregated from the priority of
  // all workers and frames in a given process.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      base::TaskPriority,
      base::TaskPriority,
      &ProcessNodeObserver::OnPriorityChanged>
      priority_{base::TaskPriority::LOWEST};

  base::flat_set<FrameNodeImpl*> frame_nodes_;

  base::flat_set<WorkerNodeImpl*> worker_nodes_;

  // Inline storage for FrozenFrameAggregator user data.
  InternalNodeAttachedDataStorage<sizeof(uintptr_t) + 8> frozen_frame_data_;

  // Inline storage for ProcessPriorityAggregator user data.
  std::unique_ptr<NodeAttachedData> process_priority_data_;

  DISALLOW_COPY_AND_ASSIGN(ProcessNodeImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_
