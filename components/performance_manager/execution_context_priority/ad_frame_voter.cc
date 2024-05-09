// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/ad_frame_voter.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context_registry.h"
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

}  // namespace

// static
const char AdFrameVoter::kAdFrameReason[] = "Ad frame.";

AdFrameVoter::AdFrameVoter(VotingChannel voting_channel)
    : voting_channel_(std::move(voting_channel)) {}

AdFrameVoter::~AdFrameVoter() = default;

void AdFrameVoter::InitializeOnGraph(Graph* graph) {
  graph->AddInitializingFrameNodeObserver(this);
}

void AdFrameVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveInitializingFrameNodeObserver(this);
}

void AdFrameVoter::OnFrameNodeInitializing(const FrameNode* frame_node) {
  if (!frame_node->IsAdFrame())
    return;

  const Vote vote(base::TaskPriority::LOWEST, kAdFrameReason);
  voting_channel_.SubmitVote(GetExecutionContext(frame_node), vote);
}

void AdFrameVoter::OnFrameNodeTearingDown(const FrameNode* frame_node) {
  if (!frame_node->IsAdFrame())
    return;

  voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
}

void AdFrameVoter::OnIsAdFrameChanged(const FrameNode* frame_node) {
  if (frame_node->IsAdFrame()) {
    const Vote vote(base::TaskPriority::LOWEST, kAdFrameReason);
    voting_channel_.SubmitVote(GetExecutionContext(frame_node), vote);
  } else {
    voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
  }
}

}  // namespace execution_context_priority
}  // namespace performance_manager
