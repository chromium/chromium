// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/graph_impl.h"

#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"

namespace ukm {
class UkmEntryBuilder;
}  // namespace ukm

namespace performance_manager {

namespace {

// A unique type ID for this implementation.
const uintptr_t kGraphImplType = reinterpret_cast<uintptr_t>(&kGraphImplType);

base::Value::Dict DescribeNodeWithDescriber(const NodeDataDescriber& describer,
                                            const Node* node) {
  switch (node->GetNodeType()) {
    case NodeTypeEnum::kFrame:
      return describer.DescribeNodeData(FrameNodeImpl::FromNode(node));
    case NodeTypeEnum::kPage:
      return describer.DescribeNodeData(PageNodeImpl::FromNode(node));
    case NodeTypeEnum::kProcess:
      return describer.DescribeNodeData(ProcessNodeImpl::FromNode(node));
    case NodeTypeEnum::kSystem:
      return describer.DescribeNodeData(SystemNodeImpl::FromNode(node));
    case NodeTypeEnum::kWorker:
      return describer.DescribeNodeData(WorkerNodeImpl::FromNode(node));
  }
  NOTREACHED();
}

class NodeDataDescriberRegistryImpl : public NodeDataDescriberRegistry {
 public:
  ~NodeDataDescriberRegistryImpl() override;

  // NodeDataDescriberRegistry impl:
  void RegisterDescriber(const NodeDataDescriber* describer,
                         std::string_view name) override;
  void UnregisterDescriber(const NodeDataDescriber* describer) override;
  base::Value::Dict DescribeNodeData(const Node* node) const override;

  size_t size() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return describers_.size();
  }

 private:
  base::flat_map<const NodeDataDescriber*, std::string> describers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

NodeDataDescriberRegistryImpl::~NodeDataDescriberRegistryImpl() {
  // All describers should have unregistered before the graph is destroyed.
  DCHECK(describers_.empty());
}

void NodeDataDescriberRegistryImpl::RegisterDescriber(
    const NodeDataDescriber* describer,
    std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  for (const auto& kv : describers_) {
    DCHECK_NE(kv.second, name) << "Name must be unique";
  }
#endif
  bool inserted =
      describers_.insert(std::make_pair(describer, std::string(name))).second;
  DCHECK(inserted);
}

void NodeDataDescriberRegistryImpl::UnregisterDescriber(
    const NodeDataDescriber* describer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t erased = describers_.erase(describer);
  DCHECK_EQ(1u, erased);
}

base::Value::Dict NodeDataDescriberRegistryImpl::DescribeNodeData(
    const Node* node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict result;
  for (const auto& [describer, describer_name] : describers_) {
    base::Value::Dict description = DescribeNodeWithDescriber(*describer, node);
    if (!description.empty()) {
      DCHECK_EQ(nullptr, result.FindDict(describer_name));
      result.Set(describer_name, std::move(description));
    }
  }
  return result;
}

}  // namespace

GraphImpl::GraphImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GraphImpl::~GraphImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(lifecycle_state_, LifecycleState::kTearDownCalled);

  // All graph registered and owned objects should have been cleaned up.
  DCHECK(graph_owned_.empty());
  DCHECK(registered_objects_.empty());

  // All process and frame nodes should have been removed already.
  DCHECK(processes_by_pid_.empty());
  DCHECK(frames_by_id_.empty());

  // All nodes should have been removed.
  for (const NodeSet& nodes : nodes_) {
    DCHECK(nodes.empty());
  }
}

void GraphImpl::SetUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateSystemNode();

  AddFrameNodeObserver(&initializing_frame_node_observer_manager_);

  execution_context_registry_impl_.SetUp(this);

  CHECK_EQ(lifecycle_state_, LifecycleState::kBeforeSetUp);
  lifecycle_state_ = LifecycleState::kSetUpCalled;
}

void GraphImpl::TearDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clean up graph owned objects. This causes their TakeFromGraph callbacks to
  // be invoked, and ideally they clean up any observers they may have, etc.
  graph_owned_.ReleaseObjects(this);

  execution_context_registry_impl_.TearDown(this);

  RemoveFrameNodeObserver(&initializing_frame_node_observer_manager_);

  // At this point, all typed observers should be empty.
  DCHECK(frame_node_observers_.empty());
  DCHECK(page_node_observers_.empty());
  DCHECK(process_node_observers_.empty());
  DCHECK(system_node_observers_.empty());

  // Remove the system node from the graph, this should be the only node left.
  ReleaseSystemNode();

  for (const NodeSet& nodes : nodes_) {
    DCHECK(nodes.empty());
  }

  CHECK_EQ(lifecycle_state_, LifecycleState::kSetUpCalled);
  lifecycle_state_ = LifecycleState::kTearDownCalled;
}

void GraphImpl::AddFrameNodeObserver(FrameNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  frame_node_observers_.AddObserver(observer);
}

void GraphImpl::AddPageNodeObserver(PageNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  page_node_observers_.AddObserver(observer);
}

void GraphImpl::AddProcessNodeObserver(ProcessNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  process_node_observers_.AddObserver(observer);
}

void GraphImpl::AddSystemNodeObserver(SystemNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  system_node_observers_.AddObserver(observer);
}

void GraphImpl::AddWorkerNodeObserver(WorkerNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  worker_node_observers_.AddObserver(observer);
}

void GraphImpl::RemoveFrameNodeObserver(FrameNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  frame_node_observers_.RemoveObserver(observer);
}

void GraphImpl::RemovePageNodeObserver(PageNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  page_node_observers_.RemoveObserver(observer);
}

void GraphImpl::RemoveProcessNodeObserver(ProcessNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  process_node_observers_.RemoveObserver(observer);
}

void GraphImpl::RemoveSystemNodeObserver(SystemNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  system_node_observers_.RemoveObserver(observer);
}

void GraphImpl::RemoveWorkerNodeObserver(WorkerNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  worker_node_observers_.RemoveObserver(observer);
}

void GraphImpl::PassToGraphImpl(std::unique_ptr<GraphOwned> graph_owned) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph_owned_.PassObject(std::move(graph_owned), this);
}

std::unique_ptr<GraphOwned> GraphImpl::TakeFromGraph(GraphOwned* graph_owned) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return graph_owned_.TakeObject(graph_owned, this);
}

void GraphImpl::RegisterObject(GraphRegistered* object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  registered_objects_.RegisterObject(object);
}

void GraphImpl::UnregisterObject(GraphRegistered* object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  registered_objects_.UnregisterObject(object);
}

const SystemNode* GraphImpl::GetSystemNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(system_node_.get());
  return system_node_.get();
}

Graph::NodeSetView<const ProcessNode*> GraphImpl::GetAllProcessNodes() const {
  return NodeSetView<const ProcessNode*>(
      GetNodesOfType(NodeTypeEnum::kProcess));
}

Graph::NodeSetView<const FrameNode*> GraphImpl::GetAllFrameNodes() const {
  return NodeSetView<const FrameNode*>(GetNodesOfType(NodeTypeEnum::kFrame));
}

Graph::NodeSetView<const PageNode*> GraphImpl::GetAllPageNodes() const {
  return NodeSetView<const PageNode*>(GetNodesOfType(NodeTypeEnum::kPage));
}

Graph::NodeSetView<const WorkerNode*> GraphImpl::GetAllWorkerNodes() const {
  return NodeSetView<const WorkerNode*>(GetNodesOfType(NodeTypeEnum::kWorker));
}

bool GraphImpl::HasOnlySystemNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetNodesOfType(NodeTypeEnum::kProcess).empty() ||
      !GetNodesOfType(NodeTypeEnum::kPage).empty() ||
      !GetNodesOfType(NodeTypeEnum::kFrame).empty() ||
      !GetNodesOfType(NodeTypeEnum::kWorker).empty()) {
    return false;
  }

  const NodeSet& system_nodes = GetNodesOfType(NodeTypeEnum::kSystem);
  return system_nodes.size() == 1 &&
         *system_nodes.begin() == GetSystemNodeImpl();
}

ukm::UkmRecorder* GraphImpl::GetUkmRecorder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ukm_recorder();
}

NodeDataDescriberRegistry* GraphImpl::GetNodeDataDescriberRegistry() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!describer_registry_)
    describer_registry_ = std::make_unique<NodeDataDescriberRegistryImpl>();

  return describer_registry_.get();
}

uintptr_t GraphImpl::GetImplType() const {
  return kGraphImplType;
}

const void* GraphImpl::GetImpl() const {
  return this;
}

#if DCHECK_IS_ON()
bool GraphImpl::IsOnGraphSequence() const {
  return sequence_checker_.CalledOnValidSequence();
}
#endif

void GraphImpl::AddInitializingFrameNodeObserver(
    InitializingFrameNodeObserver* frame_node_observer) {
  initializing_frame_node_observer_manager_.AddObserver(frame_node_observer);
}

void GraphImpl::RemoveInitializingFrameNodeObserver(
    InitializingFrameNodeObserver* frame_node_observer) {
  initializing_frame_node_observer_manager_.RemoveObserver(frame_node_observer);
}

GraphRegistered* GraphImpl::GetRegisteredObject(uintptr_t type_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return registered_objects_.GetRegisteredObject(type_id);
}

// static
GraphImpl* GraphImpl::FromGraph(const Graph* graph) {
  CHECK_EQ(kGraphImplType, graph->GetImplType());
  return reinterpret_cast<GraphImpl*>(const_cast<void*>(graph->GetImpl()));
}

bool GraphImpl::NodeInGraph(const NodeBase* node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const NodeSet& nodes = GetNodesOfType(node->GetNodeType());
  return base::Contains(nodes, node->ToNode());
}

ProcessNodeImpl* GraphImpl::GetProcessNodeByPid(base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FindPtrOrNull(processes_by_pid_, pid);
}

FrameNodeImpl* GraphImpl::GetFrameNodeById(
    RenderProcessHostId render_process_id,
    int render_frame_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FindPtrOrNull(
      frames_by_id_, ProcessAndFrameId(render_process_id, render_frame_id));
}

Graph::NodeSetView<ProcessNodeImpl*> GraphImpl::GetAllProcessNodeImpls() const {
  return NodeSetView<ProcessNodeImpl*>(GetNodesOfType(NodeTypeEnum::kProcess));
}

Graph::NodeSetView<FrameNodeImpl*> GraphImpl::GetAllFrameNodeImpls() const {
  return NodeSetView<FrameNodeImpl*>(GetNodesOfType(NodeTypeEnum::kFrame));
}

Graph::NodeSetView<PageNodeImpl*> GraphImpl::GetAllPageNodeImpls() const {
  return NodeSetView<PageNodeImpl*>(GetNodesOfType(NodeTypeEnum::kPage));
}

Graph::NodeSetView<WorkerNodeImpl*> GraphImpl::GetAllWorkerNodeImpls() const {
  return NodeSetView<WorkerNodeImpl*>(GetNodesOfType(NodeTypeEnum::kWorker));
}

void GraphImpl::AddNewNode(NodeBase* new_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!node_in_transition_);

  // Add the node to the graph.
  NodeSet& nodes = GetNodesOfType(new_node->GetNodeType());
  auto it = nodes.insert(new_node->ToNode());
  DCHECK(it.second);  // Inserted successfully

  // Advance the node through its lifecycle until it is active in the graph. See
  // NodeBase and NodeState for full details of the lifecycle.
  node_in_transition_ = new_node;
  node_in_transition_state_ = NodeState::kNotInGraph;
  new_node->JoinGraph(this);
  node_in_transition_state_ = NodeState::kInitializing;
  new_node->OnJoiningGraph();
  node_in_transition_state_ = NodeState::kJoiningGraph;
  DispatchNodeAddedNotifications(new_node);
  node_in_transition_ = nullptr;
  node_in_transition_state_ = NodeState::kNotInGraph;
}

void GraphImpl::RemoveNode(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(NodeInGraph(node));
  DCHECK_EQ(this, node->graph());
  DCHECK(!node_in_transition_);

  // Walk the node through the latter half of its lifecycle. See NodeBase and
  // NodeState for full details of the lifecycle.
  node->OnBeforeLeavingGraph();
  node_in_transition_ = node;
  node_in_transition_state_ = NodeState::kLeavingGraph;
  DispatchNodeRemovedNotifications(node);
  node->RemoveNodeAttachedData();
  node->LeaveGraph();
  node_in_transition_ = nullptr;
  node_in_transition_state_ = NodeState::kNotInGraph;

  // Remove the node itself.
  NodeSet& nodes = GetNodesOfType(node->GetNodeType());
  size_t erased = nodes.erase(node->ToNode());
  DCHECK_EQ(1u, erased);
}

void GraphImpl::NotifyFrameNodeInitializing(const FrameNode* frame_node) {
  initializing_frame_node_observer_manager_.NotifyFrameNodeInitializing(
      frame_node);
}

void GraphImpl::NotifyFrameNodeTearingDown(const FrameNode* frame_node) {
  initializing_frame_node_observer_manager_.NotifyFrameNodeTearingDown(
      frame_node);
}

size_t GraphImpl::NodeDataDescriberCountForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!describer_registry_)
    return 0;
  auto* registry = static_cast<const NodeDataDescriberRegistryImpl*>(
      describer_registry_.get());
  return registry->size();
}

Graph::NodeSet& GraphImpl::GetNodesOfType(NodeTypeEnum node_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nodes_.at(base::strict_cast<size_t>(node_type));
}

const Graph::NodeSet& GraphImpl::GetNodesOfType(NodeTypeEnum node_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nodes_.at(base::strict_cast<size_t>(node_type));
}

NodeState GraphImpl::GetNodeState(const NodeBase* node) const {
  DCHECK_EQ(this, node->graph());
  // If this is a transitioning node (being added to or removed from the graph)
  // then return the appropriate state.
  if (node == node_in_transition_)
    return node_in_transition_state_;
  // Otherwise, this is a node at steady state.
  return NodeState::kActiveInGraph;
}

template <>
const GraphImpl::ObserverList<FrameNodeObserver>& GraphImpl::GetObservers()
    const {
  return frame_node_observers_;
}

template <>
const GraphImpl::ObserverList<PageNodeObserver>& GraphImpl::GetObservers()
    const {
  return page_node_observers_;
}

template <>
const GraphImpl::ObserverList<ProcessNodeObserver>& GraphImpl::GetObservers()
    const {
  return process_node_observers_;
}

template <>
const GraphImpl::ObserverList<SystemNodeObserver>& GraphImpl::GetObservers()
    const {
  return system_node_observers_;
}

template <>
const GraphImpl::ObserverList<WorkerNodeObserver>& GraphImpl::GetObservers()
    const {
  return worker_node_observers_;
}

GraphImpl::ProcessAndFrameId::ProcessAndFrameId(
    RenderProcessHostId render_process_id,
    int render_frame_id)
    : render_process_id(render_process_id), render_frame_id(render_frame_id) {}

bool GraphImpl::ProcessAndFrameId::operator<(
    const ProcessAndFrameId& other) const {
  return std::tie(render_process_id, render_frame_id) <
         std::tie(other.render_process_id, other.render_frame_id);
}

GraphImpl::ProcessAndFrameId::~ProcessAndFrameId() = default;

GraphImpl::ProcessAndFrameId::ProcessAndFrameId(
    const GraphImpl::ProcessAndFrameId& other) = default;

GraphImpl::ProcessAndFrameId& GraphImpl::ProcessAndFrameId::operator=(
    const GraphImpl::ProcessAndFrameId& other) = default;

void GraphImpl::DispatchNodeAddedNotifications(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This handles the strongly typed observer notifications.
  switch (node->GetNodeType()) {
    case NodeTypeEnum::kFrame: {
      auto* frame_node = FrameNodeImpl::FromNodeBase(node);
      for (auto& observer : frame_node_observers_) {
        observer.OnFrameNodeAdded(frame_node);
      }
    } break;
    case NodeTypeEnum::kPage: {
      auto* page_node = PageNodeImpl::FromNodeBase(node);
      for (auto& observer : page_node_observers_) {
        observer.OnPageNodeAdded(page_node);
      }
    } break;
    case NodeTypeEnum::kProcess: {
      auto* process_node = ProcessNodeImpl::FromNodeBase(node);
      for (auto& observer : process_node_observers_) {
        observer.OnProcessNodeAdded(process_node);
      }
    } break;
    case NodeTypeEnum::kSystem:
      break;
    case NodeTypeEnum::kWorker: {
      auto* worker_node = WorkerNodeImpl::FromNodeBase(node);
      for (auto& observer : worker_node_observers_) {
        observer.OnWorkerNodeAdded(worker_node);
      }
    } break;
  }
}

void GraphImpl::DispatchNodeRemovedNotifications(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (node->GetNodeType()) {
    case NodeTypeEnum::kFrame: {
      auto* frame_node = FrameNodeImpl::FromNodeBase(node);
      for (auto& observer : frame_node_observers_) {
        observer.OnBeforeFrameNodeRemoved(frame_node);
      }
    } break;
    case NodeTypeEnum::kPage: {
      auto* page_node = PageNodeImpl::FromNodeBase(node);
      for (auto& observer : page_node_observers_) {
        observer.OnBeforePageNodeRemoved(page_node);
      }
    } break;
    case NodeTypeEnum::kProcess: {
      auto* process_node = ProcessNodeImpl::FromNodeBase(node);
      for (auto& observer : process_node_observers_) {
        observer.OnBeforeProcessNodeRemoved(process_node);
      }
    } break;
    case NodeTypeEnum::kSystem:
      break;
    case NodeTypeEnum::kWorker: {
      auto* worker_node = WorkerNodeImpl::FromNodeBase(node);
      for (auto& observer : worker_node_observers_) {
        observer.OnBeforeWorkerNodeRemoved(worker_node);
      }
    } break;
  }
}

int64_t GraphImpl::GetNextNodeSerializationId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ++current_node_serialization_id_;
}

void GraphImpl::BeforeProcessPidChange(ProcessNodeImpl* process,
                                       base::ProcessId new_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // On Windows, PIDs are aggressively reused, and because not all process
  // creation/death notifications are synchronized, it's possible for more than
  // one process node to have the same PID. To handle this, the second and
  // subsequent registration override earlier registrations, while
  // unregistration will only unregister the current holder of the PID.
  if (process->GetProcessId() != base::kNullProcessId) {
    auto it = processes_by_pid_.find(process->GetProcessId());
    if (it != processes_by_pid_.end() && it->second == process)
      processes_by_pid_.erase(it);
  }
  if (new_pid != base::kNullProcessId)
    processes_by_pid_[new_pid] = process;
}

void GraphImpl::RegisterFrameNodeForId(RenderProcessHostId render_process_id,
                                       int render_frame_id,
                                       FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto insert_result = frames_by_id_.insert(
      {ProcessAndFrameId(render_process_id, render_frame_id), frame_node});
  DCHECK(insert_result.second);
}

void GraphImpl::UnregisterFrameNodeForId(RenderProcessHostId render_process_id,
                                         int render_frame_id,
                                         FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const ProcessAndFrameId process_and_frame_id(render_process_id,
                                               render_frame_id);
  DCHECK_EQ(frames_by_id_.find(process_and_frame_id)->second, frame_node);
  frames_by_id_.erase(process_and_frame_id);
}

void GraphImpl::CreateSystemNode() {
  CHECK(!system_node_);
  // Create the singleton system node instance. Ownership is taken by the
  // graph.
  system_node_ = std::make_unique<SystemNodeImpl>();
  AddNewNode(system_node_.get());
}

void GraphImpl::ReleaseSystemNode() {
  CHECK(system_node_);
  RemoveNode(system_node_.get());
  system_node_.reset();
}

}  // namespace performance_manager
