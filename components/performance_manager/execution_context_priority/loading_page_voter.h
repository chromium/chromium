// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_LOADING_PAGE_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_LOADING_PAGE_VOTER_H_

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager::execution_context_priority {

// This voter casts a TaskPriority::USER_BLOCKING vote to all frames that are
// part of a loading page. This makes switching to a loading tab faster.
// Note: This FrameNodeObserver can affect the initial priority of a frame and
// thus uses `OnBeforeFrameNodeAdded`.
class LoadingPageVoter : public PriorityVoter,
                         public PageNodeObserver,
                         public FrameNodeObserver {
 public:
  static const char kPageIsLoadingReason[];

  explicit LoadingPageVoter();
  ~LoadingPageVoter() override;

  LoadingPageVoter(const LoadingPageVoter&) = delete;
  LoadingPageVoter& operator=(const LoadingPageVoter&) = delete;

  // PriorityVoter:
  void InitializeOnGraph(Graph* graph, VotingChannel voting_channel) override;
  void TearDownOnGraph(Graph* graph) override;

  // PageNodeObserver:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override;

  // FrameNodeObserver:
  void OnBeforeFrameNodeAdded(
      const FrameNode* frame_node,
      const FrameNode* pending_parent_frame_node,
      const PageNode* pending_page_node,
      const ProcessNode* pending_process_node,
      const FrameNode* pending_parent_or_outer_document_or_embedder) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;

  VoterId voter_id() const { return voting_channel_.voter_id(); }

 private:
  // Called when a page node starts/stops loading, which will submit/invalidate
  // a vote for every frame in that page, respectively.
  void OnPageNodeStartedLoading(const PageNode* page_node);
  void OnPageNodeStoppedLoading(const PageNode* page_node);

  // Submits/Invalidates a vote for `frame_node` and its subtree.
  void SubmitVoteForSubtree(const FrameNode* frame_node);
  void InvalidateVoteForSubtree(const FrameNode* frame_node);

  VotingChannel voting_channel_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_LOADING_PAGE_VOTER_H_
