// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_audible_voter.h"

#include <utility>

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

// Returns a vote with the appropriate priority depending on if the frame is
// audible.
Vote GetVote(bool is_audible) {
  base::TaskPriority priority = is_audible ? base::TaskPriority::USER_BLOCKING
                                           : base::TaskPriority::LOWEST;
  return Vote(priority, FrameAudibleVoter::kFrameAudibleReason);
}

}  // namespace

// static
const char FrameAudibleVoter::kFrameAudibleReason[] = "Frame audible.";

FrameAudibleVoter::FrameAudibleVoter(VotingChannel voting_channel)
    : voting_channel_(std::move(voting_channel)) {}

FrameAudibleVoter::~FrameAudibleVoter() = default;

void FrameAudibleVoter::InitializeOnGraph(Graph* graph) {
  graph->AddInitializingFrameNodeObserver(this);
}

void FrameAudibleVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveInitializingFrameNodeObserver(this);
}

void FrameAudibleVoter::OnFrameNodeInitializing(const FrameNode* frame_node) {
  const Vote vote = GetVote(frame_node->IsAudible());
  voting_channel_.SubmitVote(GetExecutionContext(frame_node), vote);
}

void FrameAudibleVoter::OnFrameNodeTearingDown(const FrameNode* frame_node) {
  voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
}

void FrameAudibleVoter::OnIsAudibleChanged(const FrameNode* frame_node) {
  const Vote new_vote = GetVote(frame_node->IsAudible());
  voting_channel_.ChangeVote(GetExecutionContext(frame_node), new_vote);
}

}  // namespace execution_context_priority
}  // namespace performance_manager
