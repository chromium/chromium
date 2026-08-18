// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_LOADING_PAGE_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_LOADING_PAGE_VOTER_H_

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager::execution_context_priority {

// This voter casts a Process::Priority::kUserBlocking vote to all frames of
// a loading page if it is the active tab, or a Process::Priority::kUserVisible
// vote if it is not the active tab. This makes loading in the active tab
// fast while preventing background loading pages from freezing.
// Note: This FrameNodeObserver can affect the initial priority of a frame and
// thus uses `OnBeforeFrameNodeAdded`.
class LoadingPageVoter : public PriorityVoter,
                         public PageNodeObserver,
                         public PageLiveStateObserver,
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
  void OnEmbedderFrameNodeChanged(const PageNode* page_node,
                                  const FrameNode* previous_embedder) override;

  // PageLiveStateObserver:
  void OnIsActiveTabChanged(const PageNode* page_node) override;

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
  // Returns true if `page_node` is the active tab directly.
  bool IsPageActiveTab(const PageNode* page_node) const;

  // Returns true if the outermost embedder root page is the active tab.
  bool IsRootPageActiveTab(const PageNode* page_node) const;

  // Returns the priority to vote for a loading frame based on whether its root
  // page is the active tab.
  base::Process::Priority GetPriority(bool is_root_page_active_tab) const;

  // Called when a page node starts/stops loading, which will submit/invalidate
  // a vote for every frame in that page, respectively.
  void OnPageNodeStartedLoading(const PageNode* page_node);
  void OnPageNodeStoppedLoading(const PageNode* page_node);

  // Changes votes for `page_node` and its embedded subpages when root page
  // active tab state changes.
  void ChangeVotesForPageAndSubpages(const PageNode* page_node,
                                     bool is_root_page_active_tab);

  // Submits/Invalidates/Changes a vote for `frame_node` and its subtree.
  void SubmitVoteForSubtree(const FrameNode* frame_node);
  void InvalidateVoteForSubtree(const FrameNode* frame_node);
  void ChangeVotesForFrameSubtree(const FrameNode* frame_node,
                                  bool is_root_page_active_tab);

  VotingChannel voting_channel_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_LOADING_PAGE_VOTER_H_
