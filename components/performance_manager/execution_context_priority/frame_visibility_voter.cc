// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_visibility_voter.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "url/gurl.h"

namespace performance_manager {
namespace execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContextRegistry::GetFromGraph(
             frame_node->GetGraph())
      ->GetExecutionContextForFrameNode(frame_node);
}

// Returns a vote with the appropriate priority depending on the frame's
// |visibility|.
Vote GetVote(FrameNode::Visibility visibility) {
  base::TaskPriority priority;
  switch (visibility) {
    case FrameNode::Visibility::kUnknown:
      priority = base::TaskPriority::USER_VISIBLE;
      break;
    case FrameNode::Visibility::kVisible:
      priority = base::TaskPriority::USER_VISIBLE;
      break;
    case FrameNode::Visibility::kNotVisible:
      priority = base::TaskPriority::LOWEST;
      break;
  }
  return Vote(priority, FrameVisibilityVoter::kFrameVisibilityReason);
}

}  // namespace

// static
const char FrameVisibilityVoter::kFrameVisibilityReason[] = "Frame visibility.";

FrameVisibilityVoter::FrameVisibilityVoter() = default;

FrameVisibilityVoter::~FrameVisibilityVoter() = default;

void FrameVisibilityVoter::SetVotingChannel(VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);
}

void FrameVisibilityVoter::OnFrameNodeInitializing(
    const FrameNode* frame_node) {
  const Vote vote = GetVote(frame_node->GetVisibility());
  voting_channel_.SubmitVote(GetExecutionContext(frame_node), vote);
}

void FrameVisibilityVoter::OnFrameNodeTearingDown(const FrameNode* frame_node) {
  voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
}

void FrameVisibilityVoter::OnFrameVisibilityChanged(
    const FrameNode* frame_node,
    FrameNode::Visibility previous_value) {
  const Vote new_vote = GetVote(frame_node->GetVisibility());

  // Nothing to change if the new priority is the same as the old one.
  if (new_vote == GetVote(previous_value))
    return;

  voting_channel_.ChangeVote(GetExecutionContext(frame_node), new_vote);
}

}  // namespace execution_context_priority
}  // namespace performance_manager
