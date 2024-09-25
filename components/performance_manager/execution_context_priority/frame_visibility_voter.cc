// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_visibility_voter.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
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
Vote GetVote(FrameNode::Visibility visibility, bool is_important) {
  base::TaskPriority priority;
  switch (visibility) {
    case FrameNode::Visibility::kUnknown:
      priority = base::TaskPriority::USER_BLOCKING;
      break;
    case FrameNode::Visibility::kVisible: {
      priority = is_important ? base::TaskPriority::USER_BLOCKING
                              : base::TaskPriority::USER_VISIBLE;
      break;
    }
    case FrameNode::Visibility::kNotVisible:
      priority = base::TaskPriority::LOWEST;
      break;
  }
  return Vote(priority, FrameVisibilityVoter::kFrameVisibilityReason);
}

}  // namespace

// static
const char FrameVisibilityVoter::kFrameVisibilityReason[] = "Frame visibility.";

FrameVisibilityVoter::FrameVisibilityVoter(VotingChannel voting_channel)
    : voting_channel_(std::move(voting_channel)) {}

FrameVisibilityVoter::~FrameVisibilityVoter() = default;

void FrameVisibilityVoter::InitializeOnGraph(Graph* graph) {
  graph->AddInitializingFrameNodeObserver(this);
}

void FrameVisibilityVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveInitializingFrameNodeObserver(this);
}

void FrameVisibilityVoter::OnFrameNodeInitializing(
    const FrameNode* frame_node) {
  const Vote vote =
      GetVote(frame_node->GetVisibility(), frame_node->IsImportant());
  voting_channel_.SubmitVote(GetExecutionContext(frame_node), vote);
}

void FrameVisibilityVoter::OnFrameNodeTearingDown(const FrameNode* frame_node) {
  voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
}

void FrameVisibilityVoter::OnFrameVisibilityChanged(
    const FrameNode* frame_node,
    FrameNode::Visibility previous_value) {
  const Vote old_vote = GetVote(previous_value, frame_node->IsImportant());

  const Vote new_vote =
      GetVote(frame_node->GetVisibility(), frame_node->IsImportant());

  // Nothing to change if the new priority is the same as the old one.
  if (new_vote == old_vote) {
    return;
  }

  voting_channel_.ChangeVote(GetExecutionContext(frame_node), new_vote);
}

void FrameVisibilityVoter::OnIsImportantChanged(const FrameNode* frame_node) {
  const Vote old_vote =
      GetVote(frame_node->GetVisibility(), !frame_node->IsImportant());
  const Vote new_vote =
      GetVote(frame_node->GetVisibility(), frame_node->IsImportant());

  // Nothing to change if the new priority is the same as the old one.
  if (new_vote == old_vote) {
    return;
  }

  voting_channel_.ChangeVote(GetExecutionContext(frame_node), new_vote);
}

}  // namespace execution_context_priority
}  // namespace performance_manager
