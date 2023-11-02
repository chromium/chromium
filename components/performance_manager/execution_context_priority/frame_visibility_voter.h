// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_VISIBILITY_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_VISIBILITY_VOTER_H_

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/graph/frame_node.h"

namespace performance_manager {
namespace execution_context_priority {

// This voter tracks frame nodes and casts a vote for each of them, whose value
// depends on their visibility. A visible frame will receive a
// TaskPriority::USER_VISIBLE vote, while a non-visible frame will receive a
// TaskPriority::LOWEST vote.
class FrameVisibilityVoter : public FrameNode::ObserverDefaultImpl {
 public:
  static const char kFrameVisibilityReason[];

  FrameVisibilityVoter();
  ~FrameVisibilityVoter() override;

  FrameVisibilityVoter(const FrameVisibilityVoter&) = delete;
  FrameVisibilityVoter& operator=(const FrameVisibilityVoter&) = delete;

  // Sets the voting channel where the votes will be cast.
  void SetVotingChannel(VotingChannel voting_channel);

  // FrameNodeObserver:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnFrameVisibilityChanged(const FrameNode* frame_node,
                                FrameNode::Visibility previous_value) override;

 private:
  VotingChannel voting_channel_;
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_VISIBILITY_VOTER_H_
