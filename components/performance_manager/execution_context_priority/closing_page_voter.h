// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_CLOSING_PAGE_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_CLOSING_PAGE_VOTER_H_

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/voting/voting.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace performance_manager::execution_context_priority {

// This voter boosts the priority of a page that is closing.
class ClosingPageVoter : public PriorityVoter,
                         public PageNodeObserver,
                         public FrameNodeObserver,
                         public GraphRegisteredImpl<ClosingPageVoter> {
 public:
  static const char kPageIsClosingReason[];

  ClosingPageVoter();
  ~ClosingPageVoter() override;

  ClosingPageVoter(const ClosingPageVoter&) = delete;
  ClosingPageVoter& operator=(const ClosingPageVoter&) = delete;

  // Sets the page's closing state and adjusts priority votes accordingly.
  void SetPageIsClosing(const PageNode* page_node, bool is_closing);

  // PriorityVoter:
  void InitializeOnGraph(Graph* graph, VotingChannel voting_channel) override;
  void TearDownOnGraph(Graph* graph) override;

  // PageNodeObserver:
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;

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
  // Adjust votes for the subtree rooted at `frame_node`. `is_closing` is the
  // new closing state of the parent page.
  void AdjustVotesForSubtree(const FrameNode* frame_node, bool is_closing);

  VotingChannel voting_channel_;

  // A set of pages that are currently closing.
  absl::flat_hash_set<raw_ptr<const PageNode>> closing_pages_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_CLOSING_PAGE_VOTER_H_
