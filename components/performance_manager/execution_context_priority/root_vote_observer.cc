// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/root_vote_observer.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context.h"

namespace performance_manager {

namespace execution_context_priority {

namespace {

// Sets the priority of an execution context.
void SetPriorityAndReason(
    const execution_context::ExecutionContext* execution_context,
    const PriorityAndReason& priority_and_reason) {
  switch (execution_context->GetType()) {
    case execution_context::ExecutionContextType::kFrameNode:
      FrameNodeImpl::FromNode(execution_context->GetFrameNode())
          ->SetPriorityAndReason(priority_and_reason);
      break;
    case execution_context::ExecutionContextType::kWorkerNode:
      WorkerNodeImpl::FromNode(execution_context->GetWorkerNode())
          ->SetPriorityAndReason(priority_and_reason);
      break;
  }
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

void RootVoteObserver::OnVoteSubmitted(
    VoterId voter_id,
    const ExecutionContext* execution_context,
    const Vote& vote) {
  DCHECK_EQ(voter_id_, voter_id);
  SetPriorityAndReason(execution_context,
                       PriorityAndReason(vote.value(), vote.reason()));
}

void RootVoteObserver::OnVoteChanged(VoterId voter_id,
                                     const ExecutionContext* execution_context,
                                     const Vote& new_vote) {
  DCHECK_EQ(voter_id_, voter_id);
  SetPriorityAndReason(execution_context,
                       PriorityAndReason(new_vote.value(), new_vote.reason()));
}

void RootVoteObserver::OnVoteInvalidated(
    VoterId voter_id,
    const ExecutionContext* execution_context) {
  DCHECK_EQ(voter_id_, voter_id);
  SetPriorityAndReason(
      execution_context,
      PriorityAndReason(base::TaskPriority::LOWEST,
                        FrameNodeImpl::kDefaultPriorityReason));
}

}  // namespace execution_context_priority
}  // namespace performance_manager
