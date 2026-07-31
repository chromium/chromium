// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_GLIC_ACTUATION_PRIORITY_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_GLIC_ACTUATION_PRIORITY_VOTER_H_

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/voting/voting.h"

namespace performance_manager::execution_context_priority {

// This voter boosts the priority of main frames in a page that is currently
// performing Glic task actuation to base::Process::Priority::kUserBlocking.
class GlicActuationPriorityVoter
    : public PriorityVoter,
      public PageLiveStateObserver,
      public PageNodeObserver,
      public FrameNodeObserver,
      public GraphRegisteredImpl<GlicActuationPriorityVoter> {
 public:
  static const char kGlicActuationReason[];

  GlicActuationPriorityVoter();
  ~GlicActuationPriorityVoter() override;

  GlicActuationPriorityVoter(const GlicActuationPriorityVoter&) = delete;
  GlicActuationPriorityVoter& operator=(const GlicActuationPriorityVoter&) =
      delete;

  // PriorityVoter:
  void InitializeOnGraph(Graph* graph, VotingChannel voting_channel) override;
  void TearDownOnGraph(Graph* graph) override;

  // PageLiveStateObserver:
  void OnGlicActuationStateChanged(const PageNode* page_node,
                                   GlicActuationState previous_state) override;

  // PageNodeObserver:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;

  // FrameNodeObserver:
  void OnBeforeFrameNodeAdded(
      const FrameNode* frame_node,
      const FrameNode* pending_parent_frame_node,
      const PageNode* pending_page_node,
      const ProcessNode* pending_process_node,
      const FrameNode* pending_parent_or_outer_document_or_embedder) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnCurrentFrameChanged(const FrameNode* previous_frame_node,
                             const FrameNode* current_frame_node) override;

  VoterId voter_id() const { return voting_channel_.voter_id(); }

 private:
  VotingChannel voting_channel_;

  void UpdateFrameNodeVote(const FrameNode* frame_node,
                           GlicActuationState previous_state,
                           GlicActuationState new_state);
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_GLIC_ACTUATION_PRIORITY_VOTER_H_
