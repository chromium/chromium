// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_capturing_media_stream_voter.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

namespace {

// Returns a vote with the appropriate priority depending on if the frame is
// capturing media.
Vote GetVote(bool is_capturing_media_stream) {
  base::Process::Priority priority =
      is_capturing_media_stream ? base::Process::Priority::kUserBlocking
                                : base::Process::Priority::kMinValue;
  return Vote(priority,
              FrameCapturingMediaStreamVoter::kFrameCapturingMediaStreamReason);
}

}  // namespace

// static
const char FrameCapturingMediaStreamVoter::kFrameCapturingMediaStreamReason[] =
    "Frame capturing media stream.";

FrameCapturingMediaStreamVoter::FrameCapturingMediaStreamVoter() = default;

FrameCapturingMediaStreamVoter::~FrameCapturingMediaStreamVoter() = default;

void FrameCapturingMediaStreamVoter::InitializeOnGraph(
    Graph* graph,
    VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);

  graph->AddFrameNodeObserver(this);
}

void FrameCapturingMediaStreamVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);

  voting_channel_.Reset();
}

void FrameCapturingMediaStreamVoter::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  SetVoteForFrame(frame_node);
}

void FrameCapturingMediaStreamVoter::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  voting_channel_.SetVote(frame_node, std::nullopt);
}

void FrameCapturingMediaStreamVoter::OnIsCapturingMediaStreamChanged(
    const FrameNode* frame_node) {
  SetVoteForFrame(frame_node);
}

void FrameCapturingMediaStreamVoter::SetVoteForFrame(
    const FrameNode* frame_node) {
  const Vote vote = GetVote(frame_node->IsCapturingMediaStream());
  voting_channel_.SetVote(frame_node, vote);
}

}  // namespace performance_manager::execution_context_priority
