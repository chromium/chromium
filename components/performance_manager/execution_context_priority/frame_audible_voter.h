// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_AUDIBLE_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_AUDIBLE_VOTER_H_

#include "components/performance_manager/execution_context_priority/voter_base.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/graph/frame_node.h"

namespace performance_manager {
namespace execution_context_priority {

// This voter casts a TaskPriority::USER_BLOCKING vote to all audible frames,
// and a TaskPriority::LOWEST vote to non-audible frames.
// Note: This FrameNodeObserver can affect the initial priority of a frame and
// thus uses `OnBeforeFrameNodeAdded`.
class FrameAudibleVoter : public VoterBase, public FrameNodeObserver {
 public:
  static const char kFrameAudibleReason[];

  explicit FrameAudibleVoter(VotingChannel voting_channel);
  ~FrameAudibleVoter() override;

  FrameAudibleVoter(const FrameAudibleVoter&) = delete;
  FrameAudibleVoter& operator=(const FrameAudibleVoter&) = delete;

  // VoterBase:
  void InitializeOnGraph(Graph* graph) override;
  void TearDownOnGraph(Graph* graph) override;

  // FrameNodeObserver:
  void OnBeforeFrameNodeAdded(
      const FrameNode* frame_node,
      const FrameNode* pending_parent_frame_node,
      const PageNode* pending_page_node,
      const ProcessNode* pending_process_node,
      const FrameNode* pending_parent_or_outer_document_or_embedder) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnIsAudibleChanged(const FrameNode* frame_node) override;

  VoterId voter_id() const { return voting_channel_.voter_id(); }

 private:
  VotingChannel voting_channel_;
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FRAME_AUDIBLE_VOTER_H_
