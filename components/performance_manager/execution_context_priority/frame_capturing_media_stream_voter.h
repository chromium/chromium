// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_CAPTURING_MEDIA_STREAM_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_CAPTURING_MEDIA_STREAM_VOTER_H_

#include "components/performance_manager/graph/initializing_frame_node_observer.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"

namespace performance_manager::execution_context_priority {

// This voter casts a TaskPriority::USER_VISIBLE vote to all frames that are
// capturing a media stream (audio or video), and a TaskPriority::LOWEST vote
// otherwise.
// Note: Uses `InitializingFrameNodeObserver` because it can affect the initial
// priority of a frame.
class FrameCapturingMediaStreamVoter : public InitializingFrameNodeObserver {
 public:
  static const char kFrameCapturingMediaStreamReason[];

  FrameCapturingMediaStreamVoter();
  ~FrameCapturingMediaStreamVoter() override;

  FrameCapturingMediaStreamVoter(const FrameCapturingMediaStreamVoter&) =
      delete;
  FrameCapturingMediaStreamVoter& operator=(
      const FrameCapturingMediaStreamVoter&) = delete;

  // Sets the voting channel where the votes will be cast.
  void SetVotingChannel(VotingChannel voting_channel);

  // InitializingFrameNodeObserver:
  void OnFrameNodeInitializing(const FrameNode* frame_node) override;
  void OnFrameNodeTearingDown(const FrameNode* frame_node) override;
  void OnIsCapturingMediaStreamChanged(const FrameNode* frame_node) override;

 private:
  VotingChannel voting_channel_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_CAPTURING_MEDIA_STREAM_VOTER_H_
