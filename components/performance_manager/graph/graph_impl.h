// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_GRAPH_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_GRAPH_IMPL_H_

#include <stdint.h>

#include <array>
#include <map>
#include <memory>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/execution_context/execution_context_registry_impl.h"
#include "components/performance_manager/graph/initializing_frame_node_observer.h"
#include "components/performance_manager/owned_objects.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_state.h"
#include "components/performance_manager/public/graph/node_type.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/registered_objects.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace performance_manager {

class FrameNodeImpl;
class Node;
class NodeBase;
class PageNodeImpl;
class ProcessNodeImpl;
class SystemNodeImpl;
class WorkerNodeImpl;

// Represents a graph of the nodes representing a single browser. Maintains a
// set of nodes that can be retrieved in different ways, some indexed. Keeps
// a list of observers that are notified of node addition and removal.
class GraphImpl : public Graph {
 public:
  // An ObserverList that DCHECK's if any observers are still in it when the
  // graph is deleted. `allow_reentrancy` is true because some observers update
  // node properties that also trigger observers. (For example
  // FrameVisibilityVoter::OnFrameVisibilityChanged() can call
  // FrameNodeImpl::SetPriorityAndReason().)
  template <typename Observer>
  using ObserverList = base::ObserverList<Observer,
                                          /*check_empty=*/true,
                                          /*allow_reentrancy=*/true>;

  GraphImpl();
  ~GraphImpl() override;

  // Disallow copy and assign.
  GraphImpl(const GraphImpl&) = delete;
  GraphImpl& operator=(const GraphImpl&) = delete;

  // Set up the graph.
  void SetUp();

  // Tear down the graph to prepare for deletion.
  void TearDown();

  // Graph implementation:
  void AddFrameNodeObserver(FrameNodeObserver* observer) override;
  void AddPageNodeObserver(PageNodeObserver* observer) override;
  void AddProcessNodeObserver(ProcessNodeObserver* observer) override;
  void AddSystemNodeObserver(SystemNodeObserver* observer) override;
  void AddWorkerNodeObserver(WorkerNodeObserver* observer) override;
  void RemoveFrameNodeObserver(FrameNodeObserver* observer) override;
  void RemovePageNodeObserver(PageNodeObserver* observer) override;
  void RemoveProcessNodeObserver(ProcessNodeObserver* observer) override;
  void RemoveSystemNodeObserver(SystemNodeObserver* observer) override;
  void RemoveWorkerNodeObserver(WorkerNodeObserver* observer) override;
  void PassToGraphImpl(std::unique_ptr<GraphOwned> graph_owned) override;
  std::unique_ptr<GraphOwned> TakeFromGraph(GraphOwned* graph_owned) override;
  void RegisterObject(GraphRegistered* object) override;
  void UnregisterObject(GraphRegistered* object) override;
  const SystemNode* GetSystemNode() const override;
  NodeSetView<const ProcessNode*> GetAllProcessNodes() const override;
  NodeSetView<const FrameNode*> GetAllFrameNodes() const override;
  NodeSetView<const PageNode*> GetAllPageNodes() const override;
  NodeSetView<const WorkerNode*> GetAllWorkerNodes() const override;

  bool HasOnlySystemNode() const override;
  ukm::UkmRecorder* GetUkmRecorder() const override;
  NodeDataDescriberRegistry* GetNodeDataDescriberRegistry() const override;
  uintptr_t GetImplType() const override;
  const void* GetImpl() const override;
#if DCHECK_IS_ON()
  bool IsOnGraphSequence() const override;
#endif
  void AddInitializingFrameNodeObserver(
      InitializingFrameNodeObserver* frame_node_observer) override;
  void RemoveInitializingFrameNodeObserver(
      InitializingFrameNodeObserver* frame_node_observer) override;
  GraphRegistered* GetRegisteredObject(uintptr_t type_id) override;

  // Helper function for safely downcasting to the implementation. This also
  // casts away constness. This will CHECK on an invalid cast.
  static GraphImpl* FromGraph(const Graph* graph);

  void set_ukm_recorder(ukm::UkmRecorder* ukm_recorder) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ukm_recorder_ = ukm_recorder;
  }
  ukm::UkmRecorder* ukm_recorder() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ukm_recorder_;
  }

  SystemNodeImpl* GetSystemNodeImpl() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return system_node_.get();
  }
  NodeSetView<ProcessNodeImpl*> GetAllProcessNodeImpls() const;
  NodeSetView<FrameNodeImpl*> GetAllFrameNodeImpls() const;
  NodeSetView<PageNodeImpl*> GetAllPageNodeImpls() const;
  NodeSetView<WorkerNodeImpl*> GetAllWorkerNodeImpls() const;

  // Retrieves the process node with PID |pid|, if any.
  ProcessNodeImpl* GetProcessNodeByPid(base::ProcessId pid);

  // Retrieves the frame node with the routing ids of the process and the frame.
  FrameNodeImpl* GetFrameNodeById(RenderProcessHostId render_process_id,
                                  int render_frame_id);

  // Returns true if |node| is in this graph.
  bool NodeInGraph(const NodeBase* node) const;

  // Management functions for node owners, any node added to the graph must be
  // removed from the graph before it's deleted.
  void AddNewNode(NodeBase* new_node);
  void RemoveNode(NodeBase* node);

  // Sends the `OnFrameNodeInitializing()` and `OnFrameNodeTearingDown()`
  // notifications to initializing frame node observers (See
  // InitializingFrameNodeObserver for details).
  void NotifyFrameNodeInitializing(const FrameNode* frame_node);
  void NotifyFrameNodeTearingDown(const FrameNode* frame_node);

  // Allows explicitly invoking SystemNode destruction for testing.
  void ReleaseSystemNodeForTesting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ReleaseSystemNode();
  }

  size_t GraphOwnedCountForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return graph_owned_.size();
  }

  size_t GraphRegisteredCountForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return registered_objects_.size();
  }

  // Returns the number of registered NodeDataDescribers, for testing.
  size_t NodeDataDescriberCountForTesting() const;

 protected:
  friend class NodeBase;

  // Returns the underlying set holding all nodes of type `node_type`.
  NodeSet& GetNodesOfType(NodeTypeEnum node_type);
  const NodeSet& GetNodesOfType(NodeTypeEnum node_type) const;

  // Used to implement `NodeBase::GetNodeState()` and `Node::GetNodeState()`.
  NodeState GetNodeState(const NodeBase* node) const;

  // Provides access to per-node-class typed observers. Exposed to nodes via
  // TypedNodeBase.
  template <typename Observer>
  const ObserverList<Observer>& GetObservers() const;

 private:
  struct ProcessAndFrameId {
    ProcessAndFrameId(RenderProcessHostId render_process_id,
                      int render_frame_id);
    ~ProcessAndFrameId();

    ProcessAndFrameId(const ProcessAndFrameId& other);
    ProcessAndFrameId& operator=(const ProcessAndFrameId& other);

    bool operator<(const ProcessAndFrameId& other) const;
    RenderProcessHostId render_process_id;
    int render_frame_id;
  };

  using ProcessByPidMap = std::map<base::ProcessId, ProcessNodeImpl*>;
  using FrameById =
      std::map<ProcessAndFrameId, raw_ptr<FrameNodeImpl, CtnExperimental>>;

  void DispatchNodeAddedNotifications(NodeBase* node);
  void DispatchNodeRemovedNotifications(NodeBase* node);

  // Returns a new serialization ID.
  friend class NodeBase;
  int64_t GetNextNodeSerializationId();

  // Process PID map for use by ProcessNodeImpl.
  friend class ProcessNodeImpl;
  void BeforeProcessPidChange(ProcessNodeImpl* process,
                              base::ProcessId new_pid);

  // Frame id map for use by FrameNodeImpl.
  friend class FrameNodeImpl;
  void RegisterFrameNodeForId(RenderProcessHostId render_process_id,
                              int render_frame_id,
                              FrameNodeImpl* frame_node);
  void UnregisterFrameNodeForId(RenderProcessHostId render_process_id,
                                int render_frame_id,
                                FrameNodeImpl* frame_node);

  void CreateSystemNode() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void ReleaseSystemNode() VALID_CONTEXT_REQUIRED(sequence_checker_);

  enum class LifecycleState {
    kBeforeSetUp,
    kSetUpCalled,
    kTearDownCalled,
  };

  // Tracks the lifecycle state of this instance to enforce calls to `SetUp()`
  // and `TearDown()`.
  LifecycleState lifecycle_state_ = LifecycleState::kBeforeSetUp;

  std::unique_ptr<SystemNodeImpl> system_node_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::array<NodeSet, kValidNodeTypeCount> nodes_
      GUARDED_BY_CONTEXT(sequence_checker_);
  ProcessByPidMap processes_by_pid_ GUARDED_BY_CONTEXT(sequence_checker_);
  FrameById frames_by_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<ukm::UkmRecorder> ukm_recorder_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // Typed observers.
  ObserverList<FrameNodeObserver> frame_node_observers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  ObserverList<PageNodeObserver> page_node_observers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  ObserverList<ProcessNodeObserver> process_node_observers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  ObserverList<SystemNodeObserver> system_node_observers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  ObserverList<WorkerNodeObserver> worker_node_observers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Graph-owned objects. For now we only expect O(10) clients, hence the
  // flat_map.
  OwnedObjects<GraphOwned,
               /* CallbackArgType = */ Graph*,
               &GraphOwned::PassToGraphImpl,
               &GraphOwned::TakeFromGraphImpl>
      graph_owned_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Allocated on first use.
  mutable std::unique_ptr<NodeDataDescriberRegistry> describer_registry_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Storage for GraphRegistered objects.
  RegisteredObjects<GraphRegistered> registered_objects_
      GUARDED_BY_CONTEXT(sequence_checker_);

  InitializingFrameNodeObserverManager
      initializing_frame_node_observer_manager_;

  execution_context::ExecutionContextRegistryImpl
      execution_context_registry_impl_;

  // The most recently assigned serialization ID.
  int64_t current_node_serialization_id_ GUARDED_BY_CONTEXT(sequence_checker_) =
      0u;

  // The identity of the node currently being added to or removed from the
  // graph, if any. This is used to prevent re-entrant notifications.
  raw_ptr<const NodeBase> node_in_transition_ = nullptr;

  // The state of the node being added or removed. Any node in the graph not
  // explicitly in transition is automatically in the kActiveInGraph state.
  NodeState node_in_transition_state_ = NodeState::kNotInGraph;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_GRAPH_IMPL_H_
