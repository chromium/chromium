// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_capturing_video_stream_voter.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context.h"

namespace performance_manager::execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContext::From(frame_node);
}

// Returns a vote with the appropriate priority depending on if the frame is
// capturing video.
Vote GetVote(bool is_capturing_video_stream) {
  base::TaskPriority priority = is_capturing_video_stream
                                    ? base::TaskPriority::USER_VISIBLE
                                    : base::TaskPriority::LOWEST;
  return Vote(priority,
              FrameCapturingVideoStreamVoter::kFrameCapturingVideoStreamReason);
}

}  // namespace

// static
const char FrameCapturingVideoStreamVoter::kFrameCapturingVideoStreamReason[] =
    "Frame capturing video stream.";

FrameCapturingVideoStreamVoter::FrameCapturingVideoStreamVoter() = default;

FrameCapturingVideoStreamVoter::~FrameCapturingVideoStreamVoter() = default;

void FrameCapturingVideoStreamVoter::SetVotingChannel(
    VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);
}

void FrameCapturingVideoStreamVoter::OnFrameNodeInitializing(
    const FrameNode* frame_node) {
  const Vote vote = GetVote(frame_node->IsCapturingVideoStream());
  voting_channel_.SubmitVote(GetExecutionContext(frame_node), vote);
}

void FrameCapturingVideoStreamVoter::OnFrameNodeTearingDown(
    const FrameNode* frame_node) {
  voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
}

void FrameCapturingVideoStreamVoter::OnIsCapturingVideoStreamChanged(
    const FrameNode* frame_node) {
  const Vote new_vote = GetVote(frame_node->IsCapturingVideoStream());
  voting_channel_.ChangeVote(GetExecutionContext(frame_node), new_vote);
}

}  // namespace performance_manager::execution_context_priority
