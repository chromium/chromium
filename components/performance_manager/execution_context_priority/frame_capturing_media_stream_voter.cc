// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_capturing_media_stream_voter.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContext::From(frame_node);
}

// Returns a vote with the appropriate priority depending on if the frame is
// capturing media.
Vote GetVote(bool is_capturing_media_stream) {
  base::TaskPriority priority = is_capturing_media_stream
                                    ? base::TaskPriority::USER_BLOCKING
                                    : base::TaskPriority::LOWEST;
  return Vote(priority,
              FrameCapturingMediaStreamVoter::kFrameCapturingMediaStreamReason);
}

}  // namespace

// static
const char FrameCapturingMediaStreamVoter::kFrameCapturingMediaStreamReason[] =
    "Frame capturing media stream.";

FrameCapturingMediaStreamVoter::FrameCapturingMediaStreamVoter(
    VotingChannel voting_channel)
    : voting_channel_(std::move(voting_channel)) {}

FrameCapturingMediaStreamVoter::~FrameCapturingMediaStreamVoter() = default;

void FrameCapturingMediaStreamVoter::InitializeOnGraph(Graph* graph) {
  graph->AddInitializingFrameNodeObserver(this);
}

void FrameCapturingMediaStreamVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveInitializingFrameNodeObserver(this);
}

void FrameCapturingMediaStreamVoter::OnFrameNodeInitializing(
    const FrameNode* frame_node) {
  const Vote vote = GetVote(frame_node->IsCapturingMediaStream());
  voting_channel_.SubmitVote(GetExecutionContext(frame_node), vote);
}

void FrameCapturingMediaStreamVoter::OnFrameNodeTearingDown(
    const FrameNode* frame_node) {
  voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
}

void FrameCapturingMediaStreamVoter::OnIsCapturingMediaStreamChanged(
    const FrameNode* frame_node) {
  const Vote new_vote = GetVote(frame_node->IsCapturingMediaStream());
  voting_channel_.ChangeVote(GetExecutionContext(frame_node), new_vote);
}

}  // namespace performance_manager::execution_context_priority
