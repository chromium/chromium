// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_CAPTURING_MEDIA_STREAM_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_CAPTURING_MEDIA_STREAM_VOTER_H_

#include "components/performance_manager/execution_context_priority/voter_base.h"
#include "components/performance_manager/graph/initializing_frame_node_observer.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"

namespace performance_manager::execution_context_priority {

// This voter casts a TaskPriority::USER_BLOCKING vote to all frames that are
// capturing a media stream (audio or video), and a TaskPriority::LOWEST vote
// otherwise.
// Note: Uses `InitializingFrameNodeObserver` because it can affect the initial
// priority of a frame.
class FrameCapturingMediaStreamVoter : public VoterBase,
                                       public InitializingFrameNodeObserver {
 public:
  static const char kFrameCapturingMediaStreamReason[];

  explicit FrameCapturingMediaStreamVoter(VotingChannel voting_channel);
  ~FrameCapturingMediaStreamVoter() override;

  FrameCapturingMediaStreamVoter(const FrameCapturingMediaStreamVoter&) =
      delete;
  FrameCapturingMediaStreamVoter& operator=(
      const FrameCapturingMediaStreamVoter&) = delete;

  // VoterBase:
  void InitializeOnGraph(Graph* graph) override;
  void TearDownOnGraph(Graph* graph) override;

  // InitializingFrameNodeObserver:
  void OnFrameNodeInitializing(const FrameNode* frame_node) override;
  void OnFrameNodeTearingDown(const FrameNode* frame_node) override;
  void OnIsCapturingMediaStreamChanged(const FrameNode* frame_node) override;

  VoterId voter_id() const { return voting_channel_.voter_id(); }

 private:
  VotingChannel voting_channel_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_CAPTURING_MEDIA_STREAM_VOTER_H_
