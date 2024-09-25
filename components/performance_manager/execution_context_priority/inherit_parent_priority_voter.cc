// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/inherit_parent_priority_voter.h"

#include <optional>
#include <utility>

#include "base/task/task_traits.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContextRegistry::GetFromGraph(
             frame_node->GetGraph())
      ->GetExecutionContextForFrameNode(frame_node);
}

// Returns the priority that should be used to cast a vote for `frame_node`,
// which is basically the parent's priority. Returns std::nullopt when no vote
// should be cast.
std::optional<base::TaskPriority> GetVotePriority(const FrameNode* frame_node) {
  // Main frames have no parents to inherit their priority from.
  if (frame_node->IsMainFrame()) {
    return std::nullopt;
  }

  // Ad frames are skipped.
  if (frame_node->IsAdFrame()) {
    return std::nullopt;
  }

  const base::TaskPriority parent_priority =
      frame_node->GetParentFrameNode()->GetPriorityAndReason().priority();

  // Don't cast a vote with the default priority as it wouldn't have any effect
  // anyways, and this prevent unnecessary work in the aggregators.
  if (parent_priority == base::TaskPriority::BEST_EFFORT) {
    return std::nullopt;
  }

  // Only inherit up to the USER_VISIBLE priority level.
  CHECK_GE(parent_priority, base::TaskPriority::USER_VISIBLE);
  return base::TaskPriority::USER_VISIBLE;
}

std::optional<Vote> GetVote(const FrameNode* frame_node) {
  std::optional<base::TaskPriority> vote_priority = GetVotePriority(frame_node);
  if (!vote_priority) {
    return std::nullopt;
  }

  return Vote(*vote_priority,
              InheritParentPriorityVoter::kPriorityInheritedReason);
}

}  // namespace

// static
const char InheritParentPriorityVoter::kPriorityInheritedReason[] =
    "Priority inherited from parent.";

InheritParentPriorityVoter::InheritParentPriorityVoter(
    VotingChannel voting_channel)
    : voting_channel_(std::move(voting_channel)) {}

InheritParentPriorityVoter::~InheritParentPriorityVoter() = default;

void InheritParentPriorityVoter::InitializeOnGraph(Graph* graph) {
  graph->AddInitializingFrameNodeObserver(this);
}

void InheritParentPriorityVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveInitializingFrameNodeObserver(this);
}

void InheritParentPriorityVoter::OnFrameNodeInitializing(
    const FrameNode* frame_node) {
  voting_channel_.SubmitVote(GetExecutionContext(frame_node),
                             GetVote(frame_node));
}

void InheritParentPriorityVoter::OnFrameNodeTearingDown(
    const FrameNode* frame_node) {
  voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
}

void InheritParentPriorityVoter::OnIsAdFrameChanged(
    const FrameNode* frame_node) {
  voting_channel_.ChangeVote(GetExecutionContext(frame_node),
                             GetVote(frame_node));
}

void InheritParentPriorityVoter::OnPriorityAndReasonChanged(
    const FrameNode* frame_node,
    const PriorityAndReason& previous_value) {
  // If only the reason changed, nothing to do.
  if (frame_node->GetPriorityAndReason().priority() ==
      previous_value.priority()) {
    return;
  }

  // Maybe change the vote for every children.
  for (const FrameNode* child_frame_node : frame_node->GetChildFrameNodes()) {
    voting_channel_.ChangeVote(GetExecutionContext(child_frame_node),
                               GetVote(child_frame_node));
  }
}

}  // namespace performance_manager::execution_context_priority
