// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_VISIBILITY_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_VISIBILITY_VOTER_H_

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/frame_node.h"

namespace performance_manager {
namespace execution_context_priority {

// This voter tracks frame nodes and casts a vote for each of them, whose value
// depends on their visibility. A visible frame will receive a
// TaskPriority::USER_BLOCKING vote, while a non-visible frame will receive a
// TaskPriority::LOWEST vote.
// If the kUnimportantFrame feature is enabled, a lesser
// TaskPriority::USER_VISIBLE vote is cast for frames that are deemed
// unimportant.
// Note: This FrameNodeObserver can affect the initial priority of a frame and
// thus uses `OnBeforeFrameNodeAdded`.
class FrameVisibilityVoter : public PriorityVoter, public FrameNodeObserver {
 public:
  static const char kFrameVisibilityReason[];

  FrameVisibilityVoter();
  ~FrameVisibilityVoter() override;

  FrameVisibilityVoter(const FrameVisibilityVoter&) = delete;
  FrameVisibilityVoter& operator=(const FrameVisibilityVoter&) = delete;

  // PriorityVoter:
  void InitializeOnGraph(Graph* graph, VotingChannel voting_channel) override;
  void TearDownOnGraph(Graph* graph) override;

  // FrameNodeObserver:
  void OnBeforeFrameNodeAdded(
      const FrameNode* frame_node,
      const FrameNode* pending_parent_frame_node,
      const PageNode* pending_page_node,
      const ProcessNode* pending_process_node,
      const FrameNode* pending_parent_or_outer_document_or_embedder) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnFrameVisibilityChanged(const FrameNode* frame_node,
                                FrameNode::Visibility previous_value) override;
  void OnIsImportantChanged(const FrameNode* frame_node) override;

  VoterId voter_id() const { return voting_channel_.voter_id(); }

 private:
  VotingChannel voting_channel_;
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_VISIBILITY_VOTER_H_
