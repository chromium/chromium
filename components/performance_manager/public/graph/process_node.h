// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PROCESS_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PROCESS_NODE_H_

#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/observer_list_types.h"
#include "base/process/process.h"
#include "base/task/task_traits.h"
#include "components/performance_manager/public/graph/node.h"
#include "components/performance_manager/public/graph/node_set_view.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/resource_attribution/process_context.h"
#include "content/public/common/process_type.h"

namespace base {
class Process;
}  // namespace base

namespace performance_manager {

class FrameNode;
class WorkerNode;
class ProcessNodeObserver;
class RenderProcessHostProxy;
class BrowserChildProcessHostProxy;

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
//
// It is only valid to access this object on the sequence of the graph that owns
// it.
class ProcessNode : public TypedNode<ProcessNode> {
 public:
  using NodeSet = base::flat_set<const Node*>;
  template <class ReturnType>
  using NodeSetView = NodeSetView<NodeSet, ReturnType>;

  using Observer = ProcessNodeObserver;
  class ObserverDefaultImpl;

  // The type of content a renderer can host.
  enum class ContentType : uint32_t {
    // Hosted an extension.
    kExtension = 1 << 0,
    // Hosted a frame with no parent.
    kMainFrame = 1 << 1,
    // Hosted a frame with a parent.
    kSubframe = 1 << 2,
    // Hosted a frame (main frame or subframe) with a committed navigation. A
    // "speculative" frame will not have a committed navigation.
    kNavigatedFrame = 1 << 3,
    // Hosted a frame that was tagged as an ad.
    kAd = 1 << 4,
    // Hosted a worker (service worker, dedicated worker, shared worker).
    kWorker = 1 << 5,
  };

  using ContentTypes =
      base::EnumSet<ContentType, ContentType::kExtension, ContentType::kWorker>;

  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kProcess; }

  ProcessNode();

  ProcessNode(const ProcessNode&) = delete;
  ProcessNode& operator=(const ProcessNode&) = delete;

  ~ProcessNode() override;

  // Returns the type of this process.
  virtual content::ProcessType GetProcessType() const = 0;

  // Returns the process ID associated with this process. Use this in preference
  // to querying GetProcess.Pid(). It's always valid to access, but will return
  // kNullProcessId if the process has yet started. It will also retain the
  // process ID for a process that has exited (at least until the underlying
  // RenderProcessHost gets reused in the case of a crash). Refrain from using
  // this as a unique identifier as on some platforms PIDs are reused
  // aggressively.
  virtual base::ProcessId GetProcessId() const = 0;

  // Returns the base::Process backing this process. This will be an invalid
  // process if it has not yet started, or if it has exited.
  virtual const base::Process& GetProcess() const = 0;

  // Gets the unique token identifying this node for resource attribution. This
  // token will not be reused after the node is destroyed.
  virtual resource_attribution::ProcessContext GetResourceContext() const = 0;

  // Returns a time captured as early as possible after the process is launched.
  virtual base::TimeTicks GetLaunchTime() const = 0;

  // Returns the exit status of this process. This will be empty if the process
  // has not yet exited.
  virtual std::optional<int32_t> GetExitStatus() const = 0;

  // Returns the non-localized name of the process used for metrics reporting
  // metrics as specified in content::ChildProcessData during process creation.
  virtual const std::string& GetMetricsName() const = 0;

  // Returns the set of frame nodes that are hosted in this process.
  virtual NodeSetView<const FrameNode*> GetFrameNodes() const = 0;

  // Returns the set of worker nodes that are hosted in this process.
  virtual NodeSetView<const WorkerNode*> GetWorkerNodes() const = 0;

  // Returns true if the main thread task load is low (below some threshold
  // of usage). See ProcessNodeObserver::OnMainThreadTaskLoadIsLow.
  virtual bool GetMainThreadTaskLoadIsLow() const = 0;

  // Returns the most recently measured private memory footprint of the process.
  // This is roughly private, anonymous, non-discardable, resident or swapped
  // memory in kilobytes. For more details, see https://goo.gl/3kPb9S.
  //
  // Note: This is only valid if at least one component has expressed interest
  // for process memory metrics by calling
  // ProcessMetricsDecorator::RegisterInterestForProcessMetrics.
  virtual uint64_t GetPrivateFootprintKb() const = 0;

  // Returns the most recently measured resident set of the process, in
  // kilobytes.
  virtual uint64_t GetResidentSetKb() const = 0;

  // Returns the most recently measured size of private swap, in kilobytes. Will
  // only be non-zero on Linux, ChromeOS, and Android.
  virtual uint64_t GetPrivateSwapKb() const = 0;

  // Returns the render process id (equivalent to RenderProcessHost::GetID()),
  // or ChildProcessHost::kInvalidUniqueID if this is not a renderer.
  virtual RenderProcessHostId GetRenderProcessHostId() const = 0;

  // Returns a proxy to the RenderProcessHost associated with this node. The
  // proxy may only be dereferenced on the UI thread. The proxy is only valid
  // for renderer processes.
  virtual const RenderProcessHostProxy& GetRenderProcessHostProxy() const = 0;

  // Returns a proxy to the BrowserChildProcessHost associated with this node.
  // The proxy may only be dereferenced on the UI thread. The proxy is only
  // valid for non-renderer child processes.
  virtual const BrowserChildProcessHostProxy& GetBrowserChildProcessHostProxy()
      const = 0;

  // Returns the current priority of the process.
  virtual base::TaskPriority GetPriority() const = 0;

  // Returns a bit field indicating what type of content this process has
  // hosted, either currently or in the past.
  virtual ContentTypes GetHostedContentTypes() const = 0;
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class ProcessNodeObserver : public base::CheckedObserver {
 public:
  ProcessNodeObserver();

  ProcessNodeObserver(const ProcessNodeObserver&) = delete;
  ProcessNodeObserver& operator=(const ProcessNodeObserver&) = delete;

  ~ProcessNodeObserver() override;

  // Node lifetime notifications.

  // Called when a |process_node| is added to the graph. Observers must not make
  // any property changes or cause re-entrant notifications during the scope of
  // this call.
  virtual void OnProcessNodeAdded(const ProcessNode* process_node) = 0;

  // The process associated with |process_node| has been started or has exited.
  // This implies some or all of the process, process_id, launch time and/or
  // exit status properties have changed.
  virtual void OnProcessLifetimeChange(const ProcessNode* process_node) = 0;

  // Called before a |process_node| is removed from the graph. Observers must
  // not make any property changes or cause re-entrant notifications during the
  // scope of this call.
  virtual void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) = 0;

  // Notifications of property changes.
  // Invoked when the |main_thread_task_load_is_low| property changes.
  virtual void OnMainThreadTaskLoadIsLow(const ProcessNode* process_node) = 0;

  // Invoked when the process priority changes.
  virtual void OnPriorityChanged(const ProcessNode* process_node,
                                 base::TaskPriority previous_value) = 0;

  // Events with no property changes.

  // Fired when all frames in a process have transitioned to being frozen.
  virtual void OnAllFramesInProcessFrozen(const ProcessNode* process_node) = 0;
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class ProcessNode::ObserverDefaultImpl : public ProcessNodeObserver {
 public:
  ObserverDefaultImpl();

  ObserverDefaultImpl(const ObserverDefaultImpl&) = delete;
  ObserverDefaultImpl& operator=(const ObserverDefaultImpl&) = delete;

  ~ObserverDefaultImpl() override;

  // ProcessNodeObserver implementation:
  void OnProcessNodeAdded(const ProcessNode* process_node) override {}
  void OnProcessLifetimeChange(const ProcessNode* process_node) override {}
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override {}
  void OnMainThreadTaskLoadIsLow(const ProcessNode* process_node) override {}
  void OnPriorityChanged(const ProcessNode* process_node,
                         base::TaskPriority previous_value) override {}
  void OnAllFramesInProcessFrozen(const ProcessNode* process_node) override {}
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PROCESS_NODE_H_
