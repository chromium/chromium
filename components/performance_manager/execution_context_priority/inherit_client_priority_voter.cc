// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/inherit_client_priority_voter.h"

#include <utility>

#include "base/auto_reset.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager {
namespace execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContextRegistry::GetFromGraph(
             frame_node->GetGraph())
      ->GetExecutionContextForFrameNode(frame_node);
}

const execution_context::ExecutionContext* GetExecutionContext(
    const WorkerNode* worker_node) {
  return execution_context::ExecutionContextRegistry::GetFromGraph(
             worker_node->GetGraph())
      ->GetExecutionContextForWorkerNode(worker_node);
}

}  // namespace

// InheritClientPriorityVoter ------------------------------------------

// static
const char InheritClientPriorityVoter::kPriorityInheritedReason[] =
    "Priority inherited.";

InheritClientPriorityVoter::InheritClientPriorityVoter() = default;

InheritClientPriorityVoter::~InheritClientPriorityVoter() = default;

void InheritClientPriorityVoter::SetVotingChannel(
    VotingChannel voting_channel) {
  DCHECK(voting_channel.IsValid());
  max_vote_aggregator_.SetUpstreamVotingChannel(std::move(voting_channel));
}

void InheritClientPriorityVoter::OnFrameNodeAdded(const FrameNode* frame_node) {
  bool inserted = voting_channels_
                      .emplace(GetExecutionContext(frame_node),
                               max_vote_aggregator_.GetVotingChannel())
                      .second;
  DCHECK(inserted);
  DCHECK(frame_node->GetChildWorkerNodes().empty());
}

void InheritClientPriorityVoter::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  DCHECK(frame_node->GetChildWorkerNodes().empty());
  size_t removed = voting_channels_.erase(GetExecutionContext(frame_node));
  DCHECK_EQ(removed, 1u);
}

void InheritClientPriorityVoter::OnPriorityAndReasonChanged(
    const FrameNode* frame_node,
    const PriorityAndReason& previous_value) {
  // The priority of a frame changed. All its children must inherit the new
  // priority.

  auto it = voting_channels_.find(GetExecutionContext(frame_node));
  DCHECK(it != voting_channels_.end());

  auto& voting_channel = it->second;

  const Vote inherited_vote(frame_node->GetPriorityAndReason().priority(),
                            kPriorityInheritedReason);
  for (const WorkerNode* child_worker_node :
       frame_node->GetChildWorkerNodes()) {
    const ExecutionContext* child_execution_context =
        GetExecutionContext(child_worker_node);
    voting_channel.ChangeVote(child_execution_context, inherited_vote);
  }
}

void InheritClientPriorityVoter::OnWorkerNodeAdded(
    const WorkerNode* worker_node) {
  bool inserted = voting_channels_
                      .emplace(GetExecutionContext(worker_node),
                               max_vote_aggregator_.GetVotingChannel())
                      .second;
  DCHECK(inserted);
  DCHECK(worker_node->GetChildWorkers().empty());
}

void InheritClientPriorityVoter::OnBeforeWorkerNodeRemoved(
    const WorkerNode* worker_node) {
  DCHECK(worker_node->GetChildWorkers().empty());
  size_t removed = voting_channels_.erase(GetExecutionContext(worker_node));
  DCHECK_EQ(removed, 1u);
}

void InheritClientPriorityVoter::OnClientFrameAdded(
    const WorkerNode* worker_node,
    const FrameNode* client_frame_node) {
  // |worker_node| is now the child of |client_frame_node|. It must inherit its
  // priority.

  // Get the voting channel for the client.
  auto it = voting_channels_.find(GetExecutionContext(client_frame_node));
  DCHECK(it != voting_channels_.end());
  auto* voting_channel = &it->second;

  const Vote inherited_vote(
      client_frame_node->GetPriorityAndReason().priority(),
      kPriorityInheritedReason);
  voting_channel->SubmitVote(GetExecutionContext(worker_node), inherited_vote);
}

void InheritClientPriorityVoter::OnBeforeClientFrameRemoved(
    const WorkerNode* worker_node,
    const FrameNode* client_frame_node) {
  // |worker_node| is no longer the child of |client_frame_node|. The inherited
  // vote must be invalidated.

  // Get the voting channel for the client.
  auto it = voting_channels_.find(GetExecutionContext(client_frame_node));
  DCHECK(it != voting_channels_.end());
  auto* voting_channel = &it->second;

  voting_channel->InvalidateVote(GetExecutionContext(worker_node));
}

void InheritClientPriorityVoter::OnClientWorkerAdded(
    const WorkerNode* worker_node,
    const WorkerNode* client_worker_node) {
  // |worker_node| is now the child of |client_worker_node|. It must inherit its
  // priority.

  // Get the voting channel for the client.
  auto it = voting_channels_.find(GetExecutionContext(client_worker_node));
  DCHECK(it != voting_channels_.end());
  auto* voting_channel = &it->second;

  const Vote inherited_vote(
      client_worker_node->GetPriorityAndReason().priority(),
      kPriorityInheritedReason);
  voting_channel->SubmitVote(GetExecutionContext(worker_node), inherited_vote);
}

void InheritClientPriorityVoter::OnBeforeClientWorkerRemoved(
    const WorkerNode* worker_node,
    const WorkerNode* client_worker_node) {
  // |worker_node| is no longer the child of |client_worker_node|. The inherited
  // vote must be invalidated.

  // Get the voting channel for the client.
  auto it = voting_channels_.find(GetExecutionContext(client_worker_node));
  DCHECK(it != voting_channels_.end());
  auto* voting_channel = &it->second;

  voting_channel->InvalidateVote(GetExecutionContext(worker_node));
}

void InheritClientPriorityVoter::OnPriorityAndReasonChanged(
    const WorkerNode* worker_node,
    const PriorityAndReason& previous_value) {
  // The priority of a worker changed. All its children must inherit the new
  // priority.

  auto it = voting_channels_.find(GetExecutionContext(worker_node));

  // Unknown |worker_node|. Just ignore it until we get notified of its
  // existence via OnWorkerNodeAdded().
  if (it == voting_channels_.end())
    return;

  auto& voting_channel = it->second;

  const Vote inherited_vote(worker_node->GetPriorityAndReason().priority(),
                            kPriorityInheritedReason);
  for (const WorkerNode* child_worker_node : worker_node->GetChildWorkers()) {
    const ExecutionContext* child_execution_context =
        GetExecutionContext(child_worker_node);
    voting_channel.ChangeVote(child_execution_context, inherited_vote);
  }
}

}  // namespace execution_context_priority
}  // namespace performance_manager
