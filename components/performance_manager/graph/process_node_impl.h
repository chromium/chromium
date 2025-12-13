// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_

#include <optional>
#include <string>
#include <variant>

#include "base/byte_count.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/decorators/process_priority_aggregator_data.h"
#include "components/performance_manager/freezing/frozen_data.h"
#include "components/performance_manager/graph/node_attached_data_storage.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/node_inline_data.h"
#include "components/performance_manager/graph/properties.h"
#include "components/performance_manager/public/browser_child_process_host_proxy.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/public/mojom/v8_contexts.mojom.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/resource_attribution/cpu_measurement_data.h"
#include "components/performance_manager/scenarios/loading_scenario_data.h"
#include "components/performance_manager/scenarios/performance_scenario_data.h"
#include "content/public/browser/background_tracing_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace performance_manager {

class FrameNodeImpl;
class FrozenFrameAggregator;
class ProcessNodeImpl;
class WorkerNodeImpl;

// Tag used to create a process node for the browser process.
struct BrowserProcessNodeTag {};

// A process node follows the lifetime of a chrome process.
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
      public mojom::ProcessCoordinationUnit,
      public mojom::ChildProcessCoordinationUnit,
      public SupportsNodeInlineData<
          ProcessPriorityAggregatorData,
          FrozenData,
          PerformanceScenarioData,
          resource_attribution::CPUMeasurementData,
          resource_attribution::SharedCPUTimeResultData,
          LoadingScenarioCounts,
          // Keep this last to avoid merge conflicts.
          NodeAttachedDataStorage> {
 public:
  using PassKey = base::PassKey<ProcessNodeImpl>;

  using TypedNodeBase<ProcessNodeImpl, ProcessNode, ProcessNodeObserver>::
      FromNode;

  // Constructor for the browser process.
  explicit ProcessNodeImpl(BrowserProcessNodeTag tag);

  // Constructor for a renderer process.
  ProcessNodeImpl(RenderProcessHostProxy proxy, base::TaskPriority priority);

  // Constructor for a non-renderer child process.
  ProcessNodeImpl(content::ProcessType process_type,
                  BrowserChildProcessHostProxy proxy);

  ProcessNodeImpl(const ProcessNodeImpl&) = delete;
  ProcessNodeImpl& operator=(const ProcessNodeImpl&) = delete;

  ~ProcessNodeImpl() override;

  void BindRenderProcessCoordinationUnit(
      mojo::PendingReceiver<mojom::ProcessCoordinationUnit> receiver);
  void BindChildProcessCoordinationUnit(
      mojo::PendingReceiver<mojom::ChildProcessCoordinationUnit> receiver);

  // mojom::ProcessCoordinationUnit implementation:
  void SetMainThreadTaskLoadIsLow(bool main_thread_task_load_is_low) override;
  void OnV8ContextCreated(
      mojom::V8ContextDescriptionPtr description,
      mojom::IframeAttributionDataPtr iframe_attribution_data) override;
  void OnV8ContextDetached(
      const blink::V8ContextToken& v8_context_token) override;
  void OnV8ContextDestroyed(
      const blink::V8ContextToken& v8_context_token) override;
  void OnRemoteIframeAttached(
      const blink::LocalFrameToken& parent_frame_token,
      const blink::RemoteFrameToken& remote_frame_token,
      mojom::IframeAttributionDataPtr iframe_attribution_data) override;
  void OnRemoteIframeDetached(
      const blink::LocalFrameToken& parent_frame_token,
      const blink::RemoteFrameToken& remote_frame_token) override;

  // mojom::ChildProcessCoordinationUnit implementation:
  void InitializeChildProcessCoordination(
      InitializeChildProcessCoordinationCallback callback) override;

  // Partial ProcessNode implementation:
  content::ProcessType GetProcessType() const override;
  base::ProcessId GetProcessId() const override;
  const base::Process& GetProcess() const override;
  resource_attribution::ProcessContext GetResourceContext() const override;
  base::TimeTicks GetLaunchTime() const override;
  std::optional<int32_t> GetExitStatus() const override;
  const std::string& GetMetricsName() const override;
  bool GetMainThreadTaskLoadIsLow() const override;
  base::ByteCount GetPrivateFootprint() const override;
  base::ByteCount GetResidentSet() const override;
  base::ByteCount GetPrivateSwap() const override;
  RenderProcessHostId GetRenderProcessHostId() const override;
  const RenderProcessHostProxy& GetRenderProcessHostProxy() const override;
  const BrowserChildProcessHostProxy& GetBrowserChildProcessHostProxy()
      const override;
  base::TaskPriority GetPriority() const override;
  ContentTypes GetHostedContentTypes() const override;

  // Private implementation properties.
  NodeSetView<FrameNodeImpl*> frame_nodes() const;
  NodeSetView<WorkerNodeImpl*> worker_nodes() const;
  perfetto::Track tracing_track() const;

  void SetProcessExitStatus(int32_t exit_status);
  void SetProcessMetricsName(const std::string& metrics_name);
  void SetProcess(base::Process process, base::TimeTicks launch_time);

  void set_private_footprint(base::ByteCount private_footprint) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    private_footprint_ = private_footprint;
  }
  void set_resident_set(base::ByteCount resident_set) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    resident_set_ = resident_set;
  }
  void set_private_swap(base::ByteCount private_swap) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    private_swap_ = private_swap;
  }

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

  // Adds a new type of hosted content to the |hosted_content_types| bit field.
  void add_hosted_content_type(ContentType content_type);

  void OnAllFramesInProcessFrozen(base::PassKey<FrozenFrameAggregator>) {
    OnAllFramesInProcessFrozen();
  }

  void OnAllFramesInProcessFrozenForTesting() { OnAllFramesInProcessFrozen(); }

  base::WeakPtr<ProcessNodeImpl> GetWeakPtr();

  static PassKey CreatePassKeyForTesting() { return PassKey(); }

 protected:
  void SetProcessImpl(base::Process process,
                      base::ProcessId process_id,
                      base::TimeTicks launch_time);

 private:
  friend class ProcessMetricsDecoratorAccess;

  using AnyChildProcessHostProxy =
      std::variant<RenderProcessHostProxy, BrowserChildProcessHostProxy>;

  static perfetto::Track GetTracingTrack(content::ProcessType process_type,
                                         const AnyChildProcessHostProxy& proxy);

  // Shared constructor for all process types.
  ProcessNodeImpl(content::ProcessType process_type,
                  AnyChildProcessHostProxy proxy,
                  base::TaskPriority priority);

  // Rest of ProcessNode implementation. These are private so that users of the
  // impl use the private getters rather than the public interface.
  NodeSetView<const FrameNode*> GetFrameNodes() const override;
  NodeSetView<const WorkerNode*> GetWorkerNodes() const override;

  void OnAllFramesInProcessFrozen();

  // NodeBase:
  void OnInitializingProperties() override;
  void OnUninitializingEdges() override;
  void CleanUpNodeState() override;

  // Receiver for renderer-only messages.
  mojo::Receiver<mojom::ProcessCoordinationUnit> render_process_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  // Receiver for messages from all child processes.
  mojo::Receiver<mojom::ChildProcessCoordinationUnit> child_process_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  base::ByteCount private_footprint_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ByteCount resident_set_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ByteCount private_swap_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::ProcessId process_id_ GUARDED_BY_CONTEXT(sequence_checker_) =
      base::kNullProcessId;
  ObservedProperty::NotifiesAlways<
      base::Process,
      &ProcessNodeObserver::OnProcessLifetimeChange>
      process_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::TimeTicks launch_time_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<int32_t> exit_status_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string metrics_name_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The type of the process that this node represents.
  const content::ProcessType process_type_;

  // The proxy that allows access to either the RenderProcessHost or the
  // BrowserChildProcessHost associated with this process, if `this` is a
  // process node for a child process (process_type() != PROCESS_TYPE_BROWSER).
  const AnyChildProcessHostProxy child_process_host_proxy_;

  // The Perfetto ProcessTrack for this process.
  perfetto::Track tracing_track_ GUARDED_BY_CONTEXT(sequence_checker_);

  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &ProcessNodeObserver::OnMainThreadTaskLoadIsLow>
      main_thread_task_load_is_low_ GUARDED_BY_CONTEXT(sequence_checker_){
          false};

  // Process priority information. This is aggregated from the priority of
  // all workers and frames in a given process by the ProcessPriorityAggregator.
  // Initially high priority until the first execution context it hosts
  // determine the right priority.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      base::TaskPriority,
      &ProcessNodeObserver::OnPriorityChanged,
      TracedWrapper<base::TaskPriority>>
      priority_ GUARDED_BY_CONTEXT(sequence_checker_);

  // A bit field that indicates which type of content this process has hosted,
  // either currently or in the past.
  ContentTypes hosted_content_types_ GUARDED_BY_CONTEXT(sequence_checker_);

  NodeSet frame_nodes_ GUARDED_BY_CONTEXT(sequence_checker_);

  NodeSet worker_nodes_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<ProcessNodeImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_
