// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/graph_impl.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
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

template <typename NodeObserverType>
void AddObserverImpl(std::vector<NodeObserverType*>* observers,
                     NodeObserverType* observer) {
  DCHECK(observers);
  DCHECK(observer);
  DCHECK(!base::Contains(*observers, observer));
  observers->push_back(observer);
}

template <typename NodeObserverType>
void RemoveObserverImpl(std::vector<NodeObserverType*>* observers,
                        NodeObserverType* observer) {
  DCHECK(observers);
  DCHECK(observer);
  // We expect to find the observer in the array.
  auto it = base::ranges::find(*observers, observer);
  DCHECK(it != observers->end());
  observers->erase(it);
  // There should only have been one copy of the observer.
  DCHECK(!base::Contains(*observers, observer));
}

class NodeDataDescriberRegistryImpl : public NodeDataDescriberRegistry {
 public:
  ~NodeDataDescriberRegistryImpl() override;

  // NodeDataDescriberRegistry impl:
  void RegisterDescriber(const NodeDataDescriber* describer,
                         base::StringPiece name) override;
  void UnregisterDescriber(const NodeDataDescriber* describer) override;
  base::Value::Dict DescribeNodeData(const Node* node) const override;

  size_t size() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return describers_.size();
  }

 private:
  template <typename NodeType, typename NodeImplType>
  base::Value::Dict DescribeNodeImpl(
      base::Value::Dict (NodeDataDescriber::*DescribeFn)(const NodeType*) const,
      const NodeImplType* node) const VALID_CONTEXT_REQUIRED(sequence_checker_);

  base::flat_map<const NodeDataDescriber*, std::string> describers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

template <typename NodeType, typename NodeImplType>
base::Value::Dict NodeDataDescriberRegistryImpl::DescribeNodeImpl(
    base::Value::Dict (NodeDataDescriber::*Describe)(const NodeType*) const,
    const NodeImplType* node) const {
  base::Value::Dict result;

  for (const auto& name_and_describer : describers_) {
    base::Value::Dict description = (name_and_describer.first->*Describe)(node);
    if (!description.empty()) {
      DCHECK_EQ(nullptr, result.FindDict(name_and_describer.second));
      result.Set(name_and_describer.second, std::move(description));
    }
  }

  return result;
}

NodeDataDescriberRegistryImpl::~NodeDataDescriberRegistryImpl() {
  // All describers should have unregistered before the graph is destroyed.
  DCHECK(describers_.empty());
}

void NodeDataDescriberRegistryImpl::RegisterDescriber(
    const NodeDataDescriber* describer,
    base::StringPiece name) {
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
  const NodeBase* node_base = NodeBase::FromNode(node);
  switch (node_base->type()) {
    case NodeTypeEnum::kInvalidType:
      NOTREACHED();
      return base::Value::Dict();
    case NodeTypeEnum::kFrame:
      return DescribeNodeImpl(&NodeDataDescriber::DescribeFrameNodeData,
                              FrameNodeImpl::FromNodeBase(node_base));
    case NodeTypeEnum::kPage:
      return DescribeNodeImpl(&NodeDataDescriber::DescribePageNodeData,
                              PageNodeImpl::FromNodeBase(node_base));
    case NodeTypeEnum::kProcess:
      return DescribeNodeImpl(&NodeDataDescriber::DescribeProcessNodeData,
                              ProcessNodeImpl::FromNodeBase(node_base));
    case NodeTypeEnum::kSystem:
      return DescribeNodeImpl(&NodeDataDescriber::DescribeSystemNodeData,
                              SystemNodeImpl::FromNodeBase(node_base));
    case NodeTypeEnum::kWorker:
      return DescribeNodeImpl(&NodeDataDescriber::DescribeWorkerNodeData,
                              WorkerNodeImpl::FromNodeBase(node_base));
  }
}

}  // namespace

GraphImpl::GraphImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GraphImpl::~GraphImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // All graph registered and owned objects should have been cleaned up.
  DCHECK(graph_owned_.empty());
  DCHECK(registered_objects_.empty());

  // At this point, all typed observers should be empty.
  DCHECK(graph_observers_.empty());
  DCHECK(frame_node_observers_.empty());
  DCHECK(page_node_observers_.empty());
  DCHECK(process_node_observers_.empty());
  DCHECK(system_node_observers_.empty());

  // All process and frame nodes should have been removed already.
  DCHECK(processes_by_pid_.empty());
  DCHECK(frames_by_id_.empty());

  // All nodes should have been removed.
  DCHECK(nodes_.empty());
}

void GraphImpl::SetUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateSystemNode();
}

void GraphImpl::TearDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Notify graph observers that the graph is being destroyed.
  for (auto* observer : graph_observers_)
    observer->OnBeforeGraphDestroyed(this);

  // Clean up graph owned objects. This causes their TakeFromGraph callbacks to
  // be invoked, and ideally they clean up any observers they may have, etc.
  graph_owned_.ReleaseObjects(this);

  // At this point, all typed observers should be empty.
  DCHECK(graph_observers_.empty());
  DCHECK(frame_node_observers_.empty());
  DCHECK(page_node_observers_.empty());
  DCHECK(process_node_observers_.empty());
  DCHECK(system_node_observers_.empty());

  // Remove the system node from the graph, this should be the only node left.
  ReleaseSystemNode();

  DCHECK(nodes_.empty());
}

void GraphImpl::AddGraphObserver(GraphObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddObserverImpl(&graph_observers_, observer);
}

void GraphImpl::AddFrameNodeObserver(FrameNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddObserverImpl(&frame_node_observers_, observer);
}

void GraphImpl::AddPageNodeObserver(PageNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddObserverImpl(&page_node_observers_, observer);
}

void GraphImpl::AddProcessNodeObserver(ProcessNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddObserverImpl(&process_node_observers_, observer);
}

void GraphImpl::AddSystemNodeObserver(SystemNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddObserverImpl(&system_node_observers_, observer);
}

void GraphImpl::AddWorkerNodeObserver(WorkerNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddObserverImpl(&worker_node_observers_, observer);
}

void GraphImpl::RemoveGraphObserver(GraphObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserverImpl(&graph_observers_, observer);
}

void GraphImpl::RemoveFrameNodeObserver(FrameNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserverImpl(&frame_node_observers_, observer);
}

void GraphImpl::RemovePageNodeObserver(PageNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserverImpl(&page_node_observers_, observer);
}

void GraphImpl::RemoveProcessNodeObserver(ProcessNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserverImpl(&process_node_observers_, observer);
}

void GraphImpl::RemoveSystemNodeObserver(SystemNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserverImpl(&system_node_observers_, observer);
}

void GraphImpl::RemoveWorkerNodeObserver(WorkerNodeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserverImpl(&worker_node_observers_, observer);
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

std::vector<const ProcessNode*> GraphImpl::GetAllProcessNodes() const {
  return GetAllNodesOfType<ProcessNodeImpl, const ProcessNode*>();
}

std::vector<const FrameNode*> GraphImpl::GetAllFrameNodes() const {
  return GetAllNodesOfType<FrameNodeImpl, const FrameNode*>();
}

std::vector<const PageNode*> GraphImpl::GetAllPageNodes() const {
  return GetAllNodesOfType<PageNodeImpl, const PageNode*>();
}

std::vector<const WorkerNode*> GraphImpl::GetAllWorkerNodes() const {
  return GetAllNodesOfType<WorkerNodeImpl, const WorkerNode*>();
}

bool GraphImpl::HasOnlySystemNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nodes_.size() == 1 && *nodes_.begin() == GetSystemNodeImpl();
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

GraphRegistered* GraphImpl::GetRegisteredObject(uintptr_t type_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return registered_objects_.GetRegisteredObject(type_id);
}

// static
GraphImpl* GraphImpl::FromGraph(const Graph* graph) {
  CHECK_EQ(kGraphImplType, graph->GetImplType());
  return reinterpret_cast<GraphImpl*>(const_cast<void*>(graph->GetImpl()));
}

bool GraphImpl::NodeInGraph(const NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto& it = nodes_.find(const_cast<NodeBase*>(node));
  return it != nodes_.end();
}

ProcessNodeImpl* GraphImpl::GetProcessNodeByPid(base::ProcessId pid) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = processes_by_pid_.find(pid);
  if (it == processes_by_pid_.end())
    return nullptr;

  return it->second;
}

FrameNodeImpl* GraphImpl::GetFrameNodeById(
    RenderProcessHostId render_process_id,
    int render_frame_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it =
      frames_by_id_.find(ProcessAndFrameId(render_process_id, render_frame_id));
  if (it == frames_by_id_.end())
    return nullptr;

  return it->second;
}

std::vector<ProcessNodeImpl*> GraphImpl::GetAllProcessNodeImpls() const {
  return GetAllNodesOfType<ProcessNodeImpl, ProcessNodeImpl*>();
}

std::vector<FrameNodeImpl*> GraphImpl::GetAllFrameNodeImpls() const {
  return GetAllNodesOfType<FrameNodeImpl, FrameNodeImpl*>();
}

std::vector<PageNodeImpl*> GraphImpl::GetAllPageNodeImpls() const {
  return GetAllNodesOfType<PageNodeImpl, PageNodeImpl*>();
}

std::vector<WorkerNodeImpl*> GraphImpl::GetAllWorkerNodeImpls() const {
  return GetAllNodesOfType<WorkerNodeImpl, WorkerNodeImpl*>();
}

size_t GraphImpl::GetNodeAttachedDataCountForTesting(const Node* node,
                                                     const void* key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!node && !key)
    return node_attached_data_map_.size();

  size_t count = 0;
  for (auto& node_data : node_attached_data_map_) {
    if (node && node_data.first.first != node)
      continue;
    if (key && node_data.first.second != key)
      continue;
    ++count;
  }

  return count;
}

void GraphImpl::AddNewNode(NodeBase* new_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!node_in_transition_);

  // Add the node to the graph.
  auto it = nodes_.insert(new_node);
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
  RemoveNodeAttachedData(node);    // Data added via the public interface.
  node->RemoveNodeAttachedData();  // Data added via the private interface.
  node->LeaveGraph();
  node_in_transition_ = nullptr;
  node_in_transition_state_ = NodeState::kNotInGraph;

  // Remove the node itself.
  size_t erased = nodes_.erase(node);
  DCHECK_EQ(1u, erased);
}

size_t GraphImpl::NodeDataDescriberCountForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!describer_registry_)
    return 0;
  auto* registry = static_cast<const NodeDataDescriberRegistryImpl*>(
      describer_registry_.get());
  return registry->size();
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
const std::vector<FrameNodeObserver*>& GraphImpl::GetObservers() const {
  return frame_node_observers_;
}

template <>
const std::vector<PageNodeObserver*>& GraphImpl::GetObservers() const {
  return page_node_observers_;
}

template <>
const std::vector<ProcessNodeObserver*>& GraphImpl::GetObservers() const {
  return process_node_observers_;
}

template <>
const std::vector<SystemNodeObserver*>& GraphImpl::GetObservers() const {
  return system_node_observers_;
}

template <>
const std::vector<WorkerNodeObserver*>& GraphImpl::GetObservers() const {
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
  switch (node->type()) {
    case NodeTypeEnum::kFrame: {
      auto* frame_node = FrameNodeImpl::FromNodeBase(node);
      for (auto* observer : frame_node_observers_)
        observer->OnFrameNodeAdded(frame_node);
    } break;
    case NodeTypeEnum::kPage: {
      auto* page_node = PageNodeImpl::FromNodeBase(node);
      for (auto* observer : page_node_observers_)
        observer->OnPageNodeAdded(page_node);
    } break;
    case NodeTypeEnum::kProcess: {
      auto* process_node = ProcessNodeImpl::FromNodeBase(node);
      for (auto* observer : process_node_observers_)
        observer->OnProcessNodeAdded(process_node);
    } break;
    case NodeTypeEnum::kSystem:
      break;
    case NodeTypeEnum::kWorker: {
      auto* worker_node = WorkerNodeImpl::FromNodeBase(node);
      for (auto* observer : worker_node_observers_)
        observer->OnWorkerNodeAdded(worker_node);
    } break;
    case NodeTypeEnum::kInvalidType: {
      NOTREACHED();
    } break;
  }
}

void GraphImpl::DispatchNodeRemovedNotifications(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (node->type()) {
    case NodeTypeEnum::kFrame: {
      auto* frame_node = FrameNodeImpl::FromNodeBase(node);
      for (auto* observer : frame_node_observers_)
        observer->OnBeforeFrameNodeRemoved(frame_node);
    } break;
    case NodeTypeEnum::kPage: {
      auto* page_node = PageNodeImpl::FromNodeBase(node);
      for (auto* observer : page_node_observers_)
        observer->OnBeforePageNodeRemoved(page_node);
    } break;
    case NodeTypeEnum::kProcess: {
      auto* process_node = ProcessNodeImpl::FromNodeBase(node);
      for (auto* observer : process_node_observers_)
        observer->OnBeforeProcessNodeRemoved(process_node);
    } break;
    case NodeTypeEnum::kSystem:
      break;
    case NodeTypeEnum::kWorker: {
      auto* worker_node = WorkerNodeImpl::FromNodeBase(node);
      for (auto* observer : worker_node_observers_)
        observer->OnBeforeWorkerNodeRemoved(worker_node);
    } break;
    case NodeTypeEnum::kInvalidType: {
      NOTREACHED();
    } break;
  }
}

void GraphImpl::RemoveNodeAttachedData(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const Node* public_node = node->ToNode();
  auto lower =
      node_attached_data_map_.lower_bound(std::make_pair(public_node, nullptr));
  auto upper = node_attached_data_map_.lower_bound(
      std::make_pair(public_node + 1, nullptr));
  node_attached_data_map_.erase(lower, upper);
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
  if (process->process_id() != base::kNullProcessId) {
    auto it = processes_by_pid_.find(process->process_id());
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

template <typename NodeType, typename ReturnNodeType>
std::vector<ReturnNodeType> GraphImpl::GetAllNodesOfType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto type = NodeType::Type();
  std::vector<ReturnNodeType> ret;
  for (auto* node : nodes_) {
    if (node->type() == type)
      ret.push_back(NodeType::FromNodeBase(node));
  }
  return ret;
}

void GraphImpl::CreateSystemNode() {
  DCHECK(!system_node_);
  // Create the singleton system node instance. Ownership is taken by the
  // graph.
  system_node_ = std::make_unique<SystemNodeImpl>();
  AddNewNode(system_node_.get());
}

void GraphImpl::ReleaseSystemNode() {
  if (!system_node_.get())
    return;
  RemoveNode(system_node_.get());
  system_node_.reset();
}

}  // namespace performance_manager
