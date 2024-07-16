// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/boosting_vote_aggregator.h"

#include <algorithm>
#include <deque>
#include <tuple>
#include <utility>

#include "base/not_fatal_until.h"
#include "components/performance_manager/public/execution_context/execution_context.h"

// This voter allows expressing "priority boosts" which are used to resolve
// priority inversions. These priority inversions are a result of resource
// contention. For example, consider execution contexts ec0 and ec1, where ec0
// holds a WebLock and ec1 is waiting to acquire that WebLock. If the priority
// of ec0 is below that of execution context ec1 then its priority needs to be
// boosted in order to prevent a priority inversion.
//
// We represent this resource contention relationship with a directed edge from
// ec1 -> ec0, which expresses the fact that ec1 is waiting on a resource held
// by ec0. We instrument APIs that provide shared access to resources (WebLocks,
// IndexedDB, etc) and they serve as factories of edges, expressing their "wait
// lists" to the BoostingVoteAggregator. This graph will in general be quite
// sparse and consist of directed acyclic components (one per shared resource),
// but in theory it may be fully connected and cyclic. (A cycle in the graph
// effectively indicates web content that has expressed a dead lock, so while
// possible it is unlikely.)
//
// Nodes in the graph represent execution contexts, which themselves have
// baseline priorities that are calculated based on a variety of factors
// (visibility, type of content, ...). These priorities flow along edges if the
// next node has a lower priority, "boosting" the priority of the destination
// node in order to resolve priority inversions. Since the graph is finite and
// the flow is in one direction only this process is guaranteed to converge for
// any given graph and set of baseline priorities. We call this graph the
// "priority flow graph".
//
// We break this down into a separate graph problem per priority level. We
// define a virtual "source" node, and create directed edges from that source
// node to each node that has received an explicit vote. Finding the set of all
// nodes that are supported at that priority level then becomes one of
// reachability from a single source node, which is trivially O(|edges|) using
// a depth-first search.
//
// We wish to maintain this graph in the face of edge additions and removals
// (both direct priority votes and priority boosts are expressed as edges in
// this representation). To make this simpler we actually maintain a
// reachability spanning tree. That is, every reachable node has exactly one
// "active" (part of the spanning tree) incoming edge. The tree can be
// maintained as follows:
//
// Adding an edge:
//
// - Add the edge to the graph and mark it as inactive.
// - If the destination node is already active, we're done.
// - If the source node is inactive (and so is the destination node at this
//   point), then there is no possible path to the node, thus we're done.
// - At this point the source node is active, and the destination is inactive.
//   We mark the edge and the destination node as active, and recursively
//   investigate outbound edges to see if other nodes are now reachable. Since
//   edges are only ever marked active if they are from an active node to an
//   inactive node, this can occur once per node, thus each active node has
//   exactly one incoming edge, and the entire set of active edges and nodes
//   forms a directed tree.
//
// Removing an edge:
//
// - If the edge is inactive, simply remove it and return.
// - If the edge is active, then recurse on the subtree hanging off the
//   destination node, marking all of the associated edges and nodes as
//   inactive, while simultaneously maintaining a set of the nodes that have
//   been marked as inactive.
//   - For each node that was just marked inactive, traverse inactive back-edges
//     and find an active ancestor node if possible. Collect these active
//     ancestors as part of a search front.
//   - Launch a search from the search front, extending the tree of active nodes
//     by traversing inactive edges and marking them and the destination node as
//     active.
// - The 2-pass approach of first marking the subtree as inactive and *then*
//   extending the tree is necessary. If these 2 steps were mingled, then when
//   traversing the subtree and inspecting a node it would be possible to
//   identify an inactive edge from an ancestor that is actually a descendant
//   of the current node. The first pass serves to effectively split the tree
//   into ancestors and descendants so that future edge activations can maintain
//   the tree relationship rather than creating simple cycles.

namespace performance_manager {
namespace execution_context_priority {

namespace {

// Converts a non-default priority level to a zero-based index.
constexpr size_t PriorityToIndex(base::TaskPriority priority) {
  DCHECK_NE(base::TaskPriority::LOWEST, priority);
  return static_cast<size_t>(static_cast<int>(priority) -
                             static_cast<int>(base::TaskPriority::LOWEST)) -
         1;
}

// Converts a priority level to a bit in an |active_layers| variable.
constexpr uint32_t PriorityToBit(base::TaskPriority priority) {
  return 1 << PriorityToIndex(priority);
}

static constexpr uint32_t kFirstLayerBit = 1;
static constexpr uint32_t kLastLayerBit =
    PriorityToBit(base::TaskPriority::HIGHEST);
static_assert(kFirstLayerBit < kLastLayerBit, "expect more than 1 layer");

static const ExecutionContext* kMaxExecutionContext =
    reinterpret_cast<const ExecutionContext*>(static_cast<uintptr_t>(0) - 1);

}  // namespace

BoostingVote::BoostingVote(BoostingVoteAggregator* aggregator,
                           const ExecutionContext* input_execution_context,
                           const ExecutionContext* output_execution_context,
                           const char* reason)
    : aggregator_(aggregator),
      input_execution_context_(input_execution_context),
      output_execution_context_(output_execution_context),
      reason_(reason) {
  aggregator_->SubmitBoostingVote(this);
}

BoostingVote::BoostingVote(BoostingVote&& rhs) {
  *this = std::move(rhs);
}

BoostingVote& BoostingVote::operator=(BoostingVote&& rhs) {
  // Reset the current BoostingVote first, as it is about to be overwritten.
  Reset();

  // Take the aggregator.
  aggregator_ = std::exchange(rhs.aggregator_, nullptr);
  input_execution_context_ =
      std::exchange(rhs.input_execution_context_, nullptr);
  output_execution_context_ =
      std::exchange(rhs.output_execution_context_, nullptr);
  reason_ = std::exchange(rhs.reason_, nullptr);

  return *this;
}

BoostingVote::~BoostingVote() {
  Reset();
}

void BoostingVote::Reset() {
  if (aggregator_) {
    aggregator_->CancelBoostingVote(this);
    aggregator_ = nullptr;
  }
}

bool BoostingVoteAggregator::ActiveLayers::IsActive(uint32_t layer_bit) const {
  return active_layers_ & layer_bit;
}

void BoostingVoteAggregator::ActiveLayers::SetActive(uint32_t layer_bit) {
  active_layers_ |= layer_bit;
}

void BoostingVoteAggregator::ActiveLayers::SetInactive(uint32_t layer_bit) {
  active_layers_ &= ~layer_bit;
}

BoostingVoteAggregator::NodeData::NodeData() = default;
BoostingVoteAggregator::NodeData::NodeData(NodeData&& rhs) = default;
BoostingVoteAggregator::NodeData::~NodeData() = default;

base::TaskPriority BoostingVoteAggregator::NodeData::GetEffectivePriorityLevel()
    const {
  if (IsActive(PriorityToBit(base::TaskPriority::HIGHEST)))
    return base::TaskPriority::HIGHEST;
  if (IsActive(PriorityToBit(base::TaskPriority::USER_VISIBLE)))
    return base::TaskPriority::USER_VISIBLE;
  return base::TaskPriority::LOWEST;
}

void BoostingVoteAggregator::NodeData::IncrementEdgeCount() {
  ++edge_count_;
}

void BoostingVoteAggregator::NodeData::DecrementEdgeCount() {
  DCHECK_LT(0u, edge_count_);
  --edge_count_;
}

BoostingVoteAggregator::EdgeData::EdgeData() = default;

BoostingVoteAggregator::EdgeData::EdgeData(EdgeData&&) = default;

BoostingVoteAggregator::EdgeData& BoostingVoteAggregator::EdgeData::operator=(
    EdgeData&&) = default;

BoostingVoteAggregator::EdgeData::~EdgeData() = default;

void BoostingVoteAggregator::EdgeData::AddReason(const char* reason) {
  reasons_.push_back(reason);
}

bool BoostingVoteAggregator::EdgeData::RemoveReason(const char* reason) {
  // Special case: there's only one reason.
  if (reasons_.size() == 1) {
    DCHECK_EQ(reason, reasons_[0]);
    reasons_.clear();
    return true;
  }

  bool active_reason_changed = false;

  // Look for the reason to be erased anywhere but in the first position. This
  // is so that we can avoid causing an active reason change when possible.
  auto it = std::find(reasons_.begin() + 1, reasons_.end(), reason);

  // If the reason being deleted is the sole instance of the active reason (we
  // didn't find another instance in our search above), then there's no way to
  // avoid propagating a reason change.
  if (it == reasons_.end()) {
    DCHECK_EQ(reason, reasons_[0]);
    it = reasons_.begin();
    active_reason_changed = true;
  }

  // Replace the element being erased with the last one in the array. If it was
  // already the last one in the array this is a nop.
  *it = reasons_.back();
  reasons_.pop_back();

  return active_reason_changed;
}

const char* BoostingVoteAggregator::EdgeData::GetActiveReason() const {
  DCHECK(!reasons_.empty());
  return reasons_[0];
}

BoostingVoteAggregator::BoostingVoteAggregator() = default;

BoostingVoteAggregator::~BoostingVoteAggregator() {
  DCHECK(forward_edges_.empty());
  DCHECK(reverse_edges_.empty());
}

VotingChannel BoostingVoteAggregator::GetVotingChannel() {
  DCHECK(nodes_.empty());
  DCHECK(!input_voter_id_);
  DCHECK_GT(1u, voting_channel_factory_.voting_channels_issued());
  auto channel = voting_channel_factory_.BuildVotingChannel();
  input_voter_id_ = channel.voter_id();
  return channel;
}

void BoostingVoteAggregator::SetUpstreamVotingChannel(VotingChannel channel) {
  channel_ = std::move(channel);
}

bool BoostingVoteAggregator::IsSetup() const {
  return input_voter_id_ && channel_.IsValid();
}

void BoostingVoteAggregator::SubmitBoostingVote(
    const BoostingVote* boosting_vote) {
  bool is_new_edge = false;

  // Ensure an entry exists in the edge map.
  EdgeData* edge_data = nullptr;
  ForwardEdge fwd_edge(boosting_vote);
  auto it = forward_edges_.lower_bound(fwd_edge);
  if (it == forward_edges_.end() || it->first != fwd_edge) {
    is_new_edge = true;
    edge_data =
        &forward_edges_.insert(it, std::make_pair(fwd_edge, EdgeData()))
             ->second;
    reverse_edges_.insert(
        std::make_pair(ReverseEdge(boosting_vote), edge_data));
  } else {
    edge_data = &it->second;
  }

  // Update the list of reasons.
  edge_data->AddReason(boosting_vote->reason());

  // If this is not a new edge, then the priority flow graph result doesn't
  // change.
  if (!is_new_edge)
    return;

  auto src_node_data_it =
      FindOrCreateNodeData(boosting_vote->input_execution_context());
  auto dst_node_data_it =
      FindOrCreateNodeData(boosting_vote->output_execution_context());
  src_node_data_it->second.IncrementEdgeCount();
  dst_node_data_it->second.IncrementEdgeCount();

  // Update the reachability spanning tree for each priority layer.
  NodeDataPtrSet changes;
  for (uint32_t layer_bit = kFirstLayerBit; layer_bit <= kLastLayerBit;
       layer_bit <<= 1) {
    // If the source node is inactive, then there's no way this edge can be part
    // of the spanning tree.
    if (!src_node_data_it->second.IsActive(layer_bit))
      continue;

    // If the destination node is active, then there's no way this edge can be
    // part of the spanning tree.
    if (dst_node_data_it->second.IsActive(layer_bit))
      continue;

    // At this point we've added an edge from an active source node to an
    // inactive destination node. The spanning tree needs to be extended.
    NodeDataPtrSet active_search_front;
    active_search_front.insert(&(*dst_node_data_it));
    changes.insert(&(*dst_node_data_it));
    edge_data->SetActive(layer_bit);
    dst_node_data_it->second.SetActive(layer_bit);
    MarkNodesActiveFromSearchFront(layer_bit, &active_search_front, &changes);
  }

  UpstreamChanges(changes);
}

void BoostingVoteAggregator::CancelBoostingVote(
    const BoostingVote* boosting_vote) {
  // Find the edge and remove the reason from it; it's possible that this reason
  // was the 'active' reason associated with the edge, in which case upstream
  // votes may need to be changed.
  auto fwd_edge_it = forward_edges_.find(ForwardEdge(boosting_vote));
  bool active_reason_changed =
      fwd_edge_it->second.RemoveReason(boosting_vote->reason());

  auto src_node_data_it =
      FindNodeData(boosting_vote->input_execution_context());
  auto dst_node_data_it =
      FindNodeData(boosting_vote->output_execution_context());

  // If the edge's active reason changed, and the edge is active in any layer,
  // then the destination node might need to have its vote updated.
  NodeDataPtrSet changes;
  if (active_reason_changed && fwd_edge_it->second.IsActiveInAnyLayer())
    changes.insert(&(*dst_node_data_it));

  // If the reasons are now empty, then this is the last occurrence of the
  // edge. Remove it entirely.
  bool edge_removed = (fwd_edge_it->second.GetReasonCount() == 0);
  if (edge_removed) {
    // Remember the active layer information associated with this edge, as its
    // needed to decide if the reachability spanning tree needs to be updated.
    ActiveLayers old_edge_layers = fwd_edge_it->second;

    // Delete the edge from both the forward and reverse map, updating edge
    // counts.
    forward_edges_.erase(fwd_edge_it);
    auto rev_edge_it = reverse_edges_.find(ReverseEdge(boosting_vote));
    reverse_edges_.erase(rev_edge_it);
    src_node_data_it->second.DecrementEdgeCount();
    dst_node_data_it->second.DecrementEdgeCount();

    // Repair the reachability spanning tree for each layer.
    for (uint32_t layer_bit = kFirstLayerBit; layer_bit <= kLastLayerBit;
         layer_bit <<= 1) {
      // If the edge was inactive in this layer, then the layer won't change.
      if (!old_edge_layers.IsActive(layer_bit))
        continue;

      // The edge was active in this layer, so the reachability spanning tree
      // needs repairing.
      ReprocessSubtree(layer_bit, &(*dst_node_data_it), &changes);
    }
  }

  UpstreamChanges(changes);

  // Since an edge was removed the nodes might also be eligible for removal.
  // This is done after changes are upstreamed so that votes have a chance to
  // be canceled first.
  if (edge_removed) {
    MaybeRemoveNode(src_node_data_it);
    MaybeRemoveNode(dst_node_data_it);
  }
}

void BoostingVoteAggregator::OnVoteSubmitted(
    VoterId voter_id,
    const ExecutionContext* execution_context,
    const Vote& vote) {
  DCHECK(IsSetup());
  DCHECK_EQ(input_voter_id_, voter_id);

  // Store the vote.
  auto node_data_it = FindOrCreateNodeData(execution_context);
  node_data_it->second.SetIncomingVote(vote);

  NodeDataPtrSet changes;

  // Update the reachability tree for the new vote if necessary.
  if (vote.value() != base::TaskPriority::LOWEST) {
    uint32_t layer_bit = PriorityToBit(vote.value());
    OnVoteAdded(layer_bit, &(*node_data_it), &changes);
  }

  UpstreamChanges(changes);
}

void BoostingVoteAggregator::OnVoteChanged(
    VoterId voter_id,
    const ExecutionContext* execution_context,
    const Vote& new_vote) {
  auto node_data_it = FindNodeData(execution_context);
  auto* node_data = &(*node_data_it);

  // Remember the old and new priorities before committing any changes.
  base::TaskPriority old_priority = node_data->second.incoming_vote().value();
  base::TaskPriority new_priority = new_vote.value();

  // Update the vote in place.
  node_data->second.UpdateIncomingVote(new_vote);

  NodeDataPtrSet changes;
  changes.insert(node_data);  // This node is changing regardless.

  // Update the reachability tree for the old vote if necessary.
  if (old_priority != base::TaskPriority::LOWEST) {
    uint32_t layer_bit = PriorityToBit(old_priority);
    OnVoteRemoved(layer_bit, node_data, &changes);
  }

  // Update the reachability tree for the new vote if necessary.
  if (new_priority != base::TaskPriority::LOWEST) {
    uint32_t layer_bit = PriorityToBit(new_priority);
    OnVoteAdded(layer_bit, node_data, &changes);
  }

  UpstreamChanges(changes);
}

void BoostingVoteAggregator::OnVoteInvalidated(
    VoterId voter_id,
    const ExecutionContext* execution_context) {
  auto node_data_it = FindNodeData(execution_context);

  // Remember the old priority before committing any changes.
  base::TaskPriority old_priority =
      node_data_it->second.incoming_vote().value();

  // Update the vote.
  node_data_it->second.RemoveIncomingVote();

  NodeDataPtrSet changes;

  // Update the reachability tree for the old vote if necessary.
  if (old_priority != base::TaskPriority::LOWEST) {
    uint32_t layer_bit = PriorityToBit(old_priority);
    OnVoteRemoved(layer_bit, &(*node_data_it), &changes);
  }

  UpstreamChanges(changes);

  // The node may be eligible for removal after its incoming vote has been
  // canceled.
  MaybeRemoveNode(node_data_it);

  return;
}

template <typename Function>
void BoostingVoteAggregator::ForEachIncomingEdge(const ExecutionContext* node,
                                                 Function&& function) {
  auto edges_begin = reverse_edges_.lower_bound(ReverseEdge(nullptr, node));
  auto edges_end =
      reverse_edges_.upper_bound(ReverseEdge(kMaxExecutionContext, node));
  for (auto edge_it = edges_begin; edge_it != edges_end; ++edge_it) {
    if (!function(edge_it))
      return;
  }
}

template <typename Function>
void BoostingVoteAggregator::ForEachOutgoingEdge(const ExecutionContext* node,
                                                 Function&& function) {
  auto edges_begin = forward_edges_.lower_bound(ForwardEdge(node, nullptr));
  auto edges_end =
      forward_edges_.upper_bound(ForwardEdge(node, kMaxExecutionContext));
  for (auto edge_it = edges_begin; edge_it != edges_end; ++edge_it) {
    if (!function(edge_it))
      return;
  }
}

BoostingVoteAggregator::NodeDataMap::iterator
BoostingVoteAggregator::FindOrCreateNodeData(const ExecutionContext* node) {
  auto it = nodes_.lower_bound(node);
  if (it->first == node)
    return it;
  it = nodes_.insert(it, std::make_pair(node, NodeData()));
  return it;
}

BoostingVoteAggregator::NodeDataMap::iterator
BoostingVoteAggregator::FindNodeData(const ExecutionContext* node) {
  auto it = nodes_.find(node);
  CHECK(it != nodes_.end(), base::NotFatalUntil::M130);
  return it;
}

const char* BoostingVoteAggregator::GetVoteReason(
    const NodeDataMap::value_type* node) {
  const NodeData& node_data = node->second;
  auto priority = node_data.GetEffectivePriorityLevel();
  uint32_t layer_bit = PriorityToBit(priority);

  // No reason is needed for the lowest priority.
  if (priority == base::TaskPriority::LOWEST)
    return nullptr;

  // If a vote has been expressed for this node at the given priority level then
  // preferentially use that reason.
  if (node_data.HasIncomingVote()) {
    const Vote& incoming_vote = node_data.incoming_vote();
    if (priority == incoming_vote.value()) {
      // This node will not have an active incoming edge in this case.
      DCHECK(GetActiveInboundEdge(layer_bit, node) == reverse_edges_.end());
      return incoming_vote.reason();
    }
  }

  // Otherwise, this node has inherited its priority. Find the active incoming
  // edge and use the active reason for that edge.
  auto edge_it = GetActiveInboundEdge(layer_bit, node);
  CHECK(edge_it != reverse_edges_.end(), base::NotFatalUntil::M130);
  DCHECK(edge_it->second->GetReasonCount());
  return edge_it->second->GetActiveReason();
}

void BoostingVoteAggregator::UpstreamVoteIfNeeded(
    NodeDataMap::value_type* node) {
  const ExecutionContext* execution_context = node->first;
  NodeData* node_data = &node->second;
  auto priority = node_data->GetEffectivePriorityLevel();

  // We specifically don't upstream lowest priority votes, as that is the
  // default priority level of every execution context in the absence of any
  // specific higher votes.
  if (priority == base::TaskPriority::LOWEST) {
    if (node_data->HasOutgoingVote()) {
      node_data->CancelOutgoingVote();
      channel_.InvalidateVote(execution_context);
    }
    return;
  }

  const Vote vote(priority, GetVoteReason(node));

  // Update the vote if the node already has one. If it changed, the new vote
  // must be upstreamed.
  if (node_data->HasOutgoingVote()) {
    if (node_data->UpdateOutgoingVote(vote))
      channel_.ChangeVote(execution_context, vote);
    return;
  }

  // Create an outgoing vote.
  node_data->SetOutgoingVote(vote);
  channel_.SubmitVote(execution_context, vote);
}

void BoostingVoteAggregator::UpstreamChanges(const NodeDataPtrSet& changes) {
  for (auto* node : changes)
    UpstreamVoteIfNeeded(node);
}

void BoostingVoteAggregator::MaybeRemoveNode(
    NodeDataMap::iterator node_data_it) {
  const NodeData& node = node_data_it->second;
  if (!node.HasEdges() && !node.HasIncomingVote())
    nodes_.erase(node_data_it);
}

void BoostingVoteAggregator::MarkSubtreeInactive(uint32_t layer_bit,
                                                 NodeDataMap::value_type* node,
                                                 NodeDataPtrSet* deactivated) {
  std::deque<NodeDataMap::value_type*> to_visit;
  to_visit.push_back(node);
  while (!to_visit.empty()) {
    auto* current_node = to_visit.front();
    to_visit.pop_front();
    DCHECK(current_node->second.IsActive(layer_bit));
    current_node->second.SetInactive(layer_bit);
    deactivated->insert(current_node);

    ForEachOutgoingEdge(
        current_node->first, [&](ForwardEdges::iterator edge_it) {
          // Skip over inactive edges.
          if (!edge_it->second.IsActive(layer_bit))
            return true;

          // Mark the edge inactive.
          edge_it->second.SetInactive(layer_bit);

          // Schedule the node to be visited.
          auto dst_node_data_it = FindNodeData(edge_it->first.dst());
          to_visit.push_back(&(*dst_node_data_it));

          return true;
        });
  }
}

BoostingVoteAggregator::ReverseEdges::iterator
BoostingVoteAggregator::GetActiveInboundEdge(
    uint32_t layer_bit,
    const NodeDataMap::value_type* node) {
  ReverseEdges::iterator active_edge = reverse_edges_.end();
  ForEachIncomingEdge(node->first, [&](ReverseEdges::iterator edge_it) -> bool {
    if (!edge_it->second->IsActive(layer_bit))
      return true;

    active_edge = edge_it;
    return false;
  });

  return active_edge;
}

BoostingVoteAggregator::NodeDataMap::value_type*
BoostingVoteAggregator::GetNearestActiveAncestor(
    uint32_t layer_bit,
    const NodeDataMap::value_type* deactivated_node) {
  DCHECK(!deactivated_node->second.IsActive(layer_bit));

  NodeDataMap::value_type* active_ancestor = nullptr;
  ForEachIncomingEdge(deactivated_node->first,
                      [&](ReverseEdges::iterator edge_it) -> bool {
                        // None of the edges should be active, by definition.
                        DCHECK(!edge_it->second->IsActive(layer_bit));
                        const ExecutionContext* src_node = edge_it->first.src();
                        auto src_node_it = FindNodeData(src_node);

                        if (!src_node_it->second.IsActive(layer_bit))
                          return true;

                        // Return the first active immediate ancestor we
                        // encounter.
                        active_ancestor = &(*src_node_it);
                        return false;
                      });

  return active_ancestor;
}

void BoostingVoteAggregator::GetNearestActiveAncestors(
    uint32_t layer_bit,
    const NodeDataPtrSet& deactivated,
    NodeDataPtrSet* active_ancestors) {
  for (const auto* node : deactivated) {
    auto* ancestor = GetNearestActiveAncestor(layer_bit, node);
    if (ancestor)
      active_ancestors->insert(ancestor);
  }
}

void BoostingVoteAggregator::MarkNodesActiveFromSearchFront(
    uint32_t layer_bit,
    NodeDataPtrSet* active_search_front,
    NodeDataPtrSet* activated) {
  while (!active_search_front->empty()) {
    auto* current_node = *active_search_front->begin();
    active_search_front->erase(active_search_front->begin());
    DCHECK(current_node->second.IsActive(layer_bit));

    ForEachOutgoingEdge(
        current_node->first, [&](ForwardEdges::iterator edge_it) -> bool {
          // Ignore active edges.
          if (edge_it->second.IsActive(layer_bit))
            return true;

          // Ignore active destination nodes.
          const ExecutionContext* dst_node = edge_it->first.dst();
          auto dst_node_it = FindNodeData(dst_node);
          if (dst_node_it->second.IsActive(layer_bit))
            return true;

          // Mark the edge and the node as active, and add the
          // node both to the search front and to the set of
          // nodes that became active as a part of this search.
          edge_it->second.SetActive(layer_bit);
          dst_node_it->second.SetActive(layer_bit);
          active_search_front->insert(&(*dst_node_it));
          activated->insert(&(*dst_node_it));
          return true;
        });
  }
}

void BoostingVoteAggregator::ReprocessSubtree(uint32_t layer_bit,
                                              NodeDataMap::value_type* node,
                                              NodeDataPtrSet* changes) {
  MarkSubtreeInactive(layer_bit, node, changes);

  NodeDataPtrSet active_ancestors;
  GetNearestActiveAncestors(layer_bit, *changes, &active_ancestors);

  MarkNodesActiveFromSearchFront(layer_bit, &active_ancestors, changes);
}

void BoostingVoteAggregator::OnVoteAdded(uint32_t layer_bit,
                                         NodeDataMap::value_type* node,
                                         NodeDataPtrSet* changes) {
  changes->insert(node);

  // If the node is already active then it must be active due to another
  // inbound edge. In this case the reachability spanning tree doesn't need
  // updating, but we do deactivate the incoming edge so as to always favor the
  // explicit vote edge (and its reason). This is guaranteed not to introduce
  // cycles as there is no inbound edge to the imaginary "root" node from which
  // all votes flow (ie, we're simply rehoming a subtree to hang directly off
  // the root).
  if (node->second.IsActive(layer_bit)) {
    auto edge_it = GetActiveInboundEdge(layer_bit, node);
    edge_it->second->SetInactive(layer_bit);
    return;
  }

  // Otherwise this node is now supported by a direct vote when it wasn't
  // previously supported by anything. Extend the reachability spanning tree.
  NodeDataPtrSet active_search_front;
  active_search_front.insert(node);
  node->second.SetActive(layer_bit);
  MarkNodesActiveFromSearchFront(layer_bit, &active_search_front, changes);
}

void BoostingVoteAggregator::OnVoteRemoved(uint32_t layer_bit,
                                           NodeDataMap::value_type* node,
                                           NodeDataPtrSet* changes) {
  // The node should *not* have an active inbound edge, as we always prefer a
  // direct vote over an inferred priority boost.
  DCHECK(GetActiveInboundEdge(layer_bit, node) == reverse_edges_.end());

  // Reprocess the subtree hanging off this node.
  ReprocessSubtree(layer_bit, node, changes);
}

}  // namespace execution_context_priority
}  // namespace performance_manager
