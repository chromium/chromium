// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/root_vote_observer.h"

#include <variant>

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace performance_manager {

namespace execution_context_priority {

namespace {

// Sets the priority of a node.
template <typename NodeImplType>
void SetPriorityAndReasonOnNode(NodeImplType* node_impl,
                                const PriorityAndReason& priority_and_reason) {
  // No property changes while the node is leaving graph.
  if (node_impl->GetNodeState() == NodeState::kLeavingGraph) {
    return;
  }
  node_impl->SetPriorityAndReason(priority_and_reason);
}

void SetPriorityAndReason(VoteContext vote_context,
                          const PriorityAndReason& priority_and_reason) {
  std::visit(
      absl::Overload{
          [&](const FrameNode* frame_node) {
            SetPriorityAndReasonOnNode(FrameNodeImpl::FromNode(frame_node),
                                       priority_and_reason);
          },
          [&](const WorkerNode* worker_node) {
            SetPriorityAndReasonOnNode(WorkerNodeImpl::FromNode(worker_node),
                                       priority_and_reason);
          },
      },
      vote_context);
}

}  // namespace

RootVoteObserver::RootVoteObserver() = default;

RootVoteObserver::~RootVoteObserver() = default;

VotingChannel RootVoteObserver::GetVotingChannel() {
  DCHECK_EQ(0u, voting_channel_factory_.voting_channels_issued());
  auto channel = voting_channel_factory_.BuildVotingChannel();
  voter_id_ = channel.voter_id();
  return channel;
}

void RootVoteObserver::OnVoteSet(VoterId voter_id,
                                 VoteContext vote_context,
                                 const std::optional<Vote>& vote) {
  DCHECK_EQ(voter_id_, voter_id);
  if (vote.has_value()) {
    SetPriorityAndReason(vote_context,
                         PriorityAndReason(vote->value(), vote->reason()));
  } else {
    SetPriorityAndReason(
        vote_context, PriorityAndReason(base::Process::Priority::kMinValue,
                                        FrameNodeImpl::kDefaultPriorityReason));
  }
}

}  // namespace execution_context_priority
}  // namespace performance_manager
