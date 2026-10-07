// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/priority_setter.h"

#include "base/process/process.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/graph/node_state.h"

namespace performance_manager::execution_context_priority {

namespace {

// Sets the priority and reason of a frame or worker from its vote, or to its
// default priority and reason if it has no vote.
template <typename NodeImplType>
void SetPriorityAndReason(NodeImplType* node_impl,
                          const std::optional<Vote>& vote) {
  // No property changes while the node is leaving graph.
  if (node_impl->GetNodeState() == NodeState::kLeavingGraph) {
    return;
  }
  node_impl->SetPriorityAndReason(
      vote.has_value()
          ? PriorityAndReason(vote->value(), vote->reason())
          : PriorityAndReason(base::Process::Priority::kMinValue,
                              NodeImplType::kDefaultPriorityReason));
}

}  // namespace

PrioritySetter::PrioritySetter(MaxVoteAggregator* max_vote_aggregator)
    : max_vote_aggregator_(max_vote_aggregator) {
  max_vote_aggregator_->AddObserver(this);
}

PrioritySetter::~PrioritySetter() {
  max_vote_aggregator_->RemoveObserver(this);
}

void PrioritySetter::OnFrameTopVoteChanged(const FrameNode* frame_node,
                                           const std::optional<Vote>& vote) {
  SetPriorityAndReason(FrameNodeImpl::FromNode(frame_node), vote);
}

void PrioritySetter::OnWorkerTopVoteChanged(const WorkerNode* worker_node,
                                            const std::optional<Vote>& vote) {
  SetPriorityAndReason(WorkerNodeImpl::FromNode(worker_node), vote);
}

}  // namespace performance_manager::execution_context_priority
