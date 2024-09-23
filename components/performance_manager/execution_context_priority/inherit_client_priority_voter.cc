// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/inherit_client_priority_voter.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/not_fatal_until.h"
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

std::optional<Vote> GetVoteFromClient(const FrameNode* client_frame_node) {
  const base::TaskPriority client_priority =
      client_frame_node->GetPriorityAndReason().priority();

  if (client_priority == base::TaskPriority::BEST_EFFORT) {
    return std::nullopt;
  }

  return Vote(client_priority,
              InheritClientPriorityVoter::kPriorityInheritedReason);
}

std::optional<Vote> GetVoteFromClient(const WorkerNode* client_worker_node) {
  const base::TaskPriority client_priority =
      client_worker_node->GetPriorityAndReason().priority();

  // Don't cast a vote with the default priority as it wouldn't have any effect
  // anyways, and this prevent unnecessary work in the aggregators.
  if (client_priority == base::TaskPriority::BEST_EFFORT) {
    return std::nullopt;
  }

  return Vote(client_priority,
              InheritClientPriorityVoter::kPriorityInheritedReason);
}

}  // namespace

// InheritClientPriorityVoter ------------------------------------------

// static
const char InheritClientPriorityVoter::kPriorityInheritedReason[] =
    "Priority inherited from client(s).";

InheritClientPriorityVoter::InheritClientPriorityVoter(
    VotingChannel voting_channel) {
  DCHECK(voting_channel.IsValid());
  voter_id_ = voting_channel.voter_id();
  max_vote_aggregator_.SetUpstreamVotingChannel(std::move(voting_channel));
}

InheritClientPriorityVoter::~InheritClientPriorityVoter() = default;

void InheritClientPriorityVoter::InitializeOnGraph(Graph* graph) {
  graph->AddFrameNodeObserver(this);
  graph->AddWorkerNodeObserver(this);
}

void InheritClientPriorityVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemoveWorkerNodeObserver(this);
}

void InheritClientPriorityVoter::OnFrameNodeAdded(const FrameNode* frame_node) {
  const auto [_, inserted] = voting_channels_.emplace(
      GetExecutionContext(frame_node), max_vote_aggregator_.GetVotingChannel());
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
  if (frame_node->GetPriorityAndReason().priority() ==
      previous_value.priority()) {
    // The priority is the same, meaning only the reason changed. Ignore.
    return;
  }

  // The priority of a frame changed. All its children must inherit the new
  // priority.

  auto it = voting_channels_.find(GetExecutionContext(frame_node));
  CHECK(it != voting_channels_.end(), base::NotFatalUntil::M130);
  auto& voting_channel = it->second;

  const std::optional<Vote> inherited_vote = GetVoteFromClient(frame_node);
  for (const WorkerNode* child_worker_node :
       frame_node->GetChildWorkerNodes()) {
    const ExecutionContext* child_execution_context =
        GetExecutionContext(child_worker_node);
    voting_channel.ChangeVote(child_execution_context, inherited_vote);
  }
}

void InheritClientPriorityVoter::OnWorkerNodeAdded(
    const WorkerNode* worker_node) {
  const auto [_, inserted] =
      voting_channels_.emplace(GetExecutionContext(worker_node),
                               max_vote_aggregator_.GetVotingChannel());
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
  CHECK(it != voting_channels_.end(), base::NotFatalUntil::M130);
  auto& voting_channel = it->second;

  const std::optional<Vote> vote = GetVoteFromClient(client_frame_node);
  voting_channel.SubmitVote(GetExecutionContext(worker_node), vote);
}

void InheritClientPriorityVoter::OnBeforeClientFrameRemoved(
    const WorkerNode* worker_node,
    const FrameNode* client_frame_node) {
  // |worker_node| is no longer the child of |client_frame_node|. The inherited
  // vote must be invalidated.

  // Get the voting channel for the client.
  auto it = voting_channels_.find(GetExecutionContext(client_frame_node));
  CHECK(it != voting_channels_.end(), base::NotFatalUntil::M130);
  auto& voting_channel = it->second;

  voting_channel.InvalidateVote(GetExecutionContext(worker_node));
}

void InheritClientPriorityVoter::OnClientWorkerAdded(
    const WorkerNode* worker_node,
    const WorkerNode* client_worker_node) {
  // |worker_node| is now the child of |client_worker_node|. It must inherit its
  // priority.

  // Get the voting channel for the client.
  auto it = voting_channels_.find(GetExecutionContext(client_worker_node));
  CHECK(it != voting_channels_.end(), base::NotFatalUntil::M130);
  auto& voting_channel = it->second;

  const std::optional<Vote> inherited_vote =
      GetVoteFromClient(client_worker_node);
  voting_channel.SubmitVote(GetExecutionContext(worker_node), inherited_vote);
}

void InheritClientPriorityVoter::OnBeforeClientWorkerRemoved(
    const WorkerNode* worker_node,
    const WorkerNode* client_worker_node) {
  // |worker_node| is no longer the child of |client_worker_node|. The inherited
  // vote must be invalidated.

  // Get the voting channel for the client.
  auto it = voting_channels_.find(GetExecutionContext(client_worker_node));
  CHECK(it != voting_channels_.end(), base::NotFatalUntil::M130);
  auto& voting_channel = it->second;

  voting_channel.InvalidateVote(GetExecutionContext(worker_node));
}

void InheritClientPriorityVoter::OnPriorityAndReasonChanged(
    const WorkerNode* worker_node,
    const PriorityAndReason& previous_value) {
  if (worker_node->GetPriorityAndReason().priority() ==
      previous_value.priority()) {
    // The priority is the same, meaning only the reason changed. Ignore.
    return;
  }

  // The priority of a worker changed. All its children must inherit the new
  // priority.

  auto it = voting_channels_.find(GetExecutionContext(worker_node));
  CHECK(it != voting_channels_.end(), base::NotFatalUntil::M130);
  auto& voting_channel = it->second;

  const std::optional<Vote> inherited_vote = GetVoteFromClient(worker_node);
  for (const WorkerNode* child_worker_node : worker_node->GetChildWorkers()) {
    const ExecutionContext* child_execution_context =
        GetExecutionContext(child_worker_node);
    voting_channel.ChangeVote(child_execution_context, inherited_vote);
  }
}

}  // namespace execution_context_priority
}  // namespace performance_manager
