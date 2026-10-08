// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_PAGE_TO_FRAME_VOTE_EXPANDER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_PAGE_TO_FRAME_VOTE_EXPANDER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/max_vote_aggregator.h"
#include "components/performance_manager/public/graph/frame_node.h"

namespace performance_manager {

class Graph;
class PageNode;

namespace execution_context_priority {

// Expands the top vote of each page into a vote on every frame of that page,
// cast back into the MaxVoteAggregator, so that the final vote for a frame is
// the highest of its own votes and its page's vote.
//
// A page vote applies to every frame of the PageNode, including inactive ones.
// Page votes are meant for state that belongs to the page rather than to a
// specific document (e.g. the page is loading or closing), and that state
// carries over across navigations. Notably, this means the speculative frame of
// a cross-process navigation inherits the page vote, which ensures that its
// process is not slowed down before the navigation commits.
//
// Note: Frames in the BackForwardCache or prerendering are currently part of
// the same PageNode and thus inherit page votes too. These are expected to
// become separate pages in the future (MPArch).
//
// Page votes do not propagate to embedded pages (e.g. GuestViews).
//
// Page votes can't change while a frame of the page is being removed (this is
// CHECKed).
class PageToFrameVoteExpander : public MaxVoteAggregator::Observer,
                                public FrameNodeObserver {
 public:
  // `max_vote_aggregator` must outlive this. The top votes of pages are
  // observed from it, and the votes on frames are cast back into it.
  explicit PageToFrameVoteExpander(MaxVoteAggregator* max_vote_aggregator);
  ~PageToFrameVoteExpander() override;

  PageToFrameVoteExpander(const PageToFrameVoteExpander&) = delete;
  PageToFrameVoteExpander& operator=(const PageToFrameVoteExpander&) = delete;

  void InitializeOnGraph(Graph* graph);
  void TearDownOnGraph(Graph* graph);

  // MaxVoteAggregator::Observer: Casts the top vote of a page on every frame of
  // that page.
  void OnPageTopVoteChanged(const PageNode* page_node,
                            const std::optional<Vote>& vote) override;

  // FrameNodeObserver:
  void OnBeforeFrameNodeAdded(
      const FrameNode* frame_node,
      const FrameNode* pending_parent_frame_node,
      const PageNode* pending_page_node,
      const ProcessNode* pending_process_node,
      const FrameNode* pending_parent_or_outer_document_or_embedder) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;

 private:
  const raw_ptr<MaxVoteAggregator> max_vote_aggregator_;

  // Casts page votes on the frames of the page.
  VotingChannel voting_channel_;
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_PAGE_TO_FRAME_VOTE_EXPANDER_H_
