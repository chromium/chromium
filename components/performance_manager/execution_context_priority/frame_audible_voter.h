// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_AUDIBLE_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_AUDIBLE_VOTER_H_

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/graph/frame_node.h"

namespace performance_manager {
namespace execution_context_priority {

// This voter casts a TaskPriority::USER_VISIBLE vote to all audible frames, and
// a TaskPriority::LOWEST vote to non-audible frames.
class FrameAudibleVoter : public FrameNode::ObserverDefaultImpl {
 public:
  static const char kFrameAudibleReason[];

  FrameAudibleVoter();
  ~FrameAudibleVoter() override;

  FrameAudibleVoter(const FrameAudibleVoter&) = delete;
  FrameAudibleVoter& operator=(const FrameAudibleVoter&) = delete;

  // Sets the voting channel where the votes will be cast.
  void SetVotingChannel(VotingChannel voting_channel);

  // FrameNodeObserver:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnIsAudibleChanged(const FrameNode* frame_node) override;

 private:
  VotingChannel voting_channel_;
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_AUDIBLE_VOTER_H_
