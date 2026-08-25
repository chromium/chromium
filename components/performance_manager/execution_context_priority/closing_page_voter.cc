// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/closing_page_voter.h"

#include <utility>

#include "base/not_fatal_until.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

// static
const char ClosingPageVoter::kPageIsClosingReason[] = "Page is closing.";

ClosingPageVoter::ClosingPageVoter() = default;
ClosingPageVoter::~ClosingPageVoter() = default;

void ClosingPageVoter::SetPageIsClosing(const PageNode* page_node,
                                        bool is_closing) {
  if (is_closing) {
    auto [it, inserted] = closing_pages_.insert(page_node);
    if (!inserted) {
      // TODO(crbug.com/432275395): Investigate cases where
      // SetPageIsClosing(true) is invoked multiple times.
      return;
    }
  } else {
    size_t num_removed = closing_pages_.erase(page_node);
    CHECK_EQ(num_removed, 1U, base::NotFatalUntil::M145);
  }

  const std::optional<Vote> vote =
      is_closing
          ? std::make_optional<Vote>(base::Process::Priority::kUserBlocking,
                                     kPageIsClosingReason)
          : std::nullopt;
  for (const FrameNode* main_frame_node : page_node->GetMainFrameNodes()) {
    SetVoteForSubtree(main_frame_node, vote);
  }
}

void ClosingPageVoter::InitializeOnGraph(Graph* graph,
                                         VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);
  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);
}

void ClosingPageVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  voting_channel_.Reset();
}

void ClosingPageVoter::OnBeforePageNodeRemoved(const PageNode* page_node) {
  // Assume that the page has no more frames.
  CHECK(page_node->GetMainFrameNodes().empty(), base::NotFatalUntil::M145);

  // Stop tracking the closing state for the page on removal.
  closing_pages_.erase(page_node);
}

void ClosingPageVoter::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  if (closing_pages_.contains(pending_page_node)) {
    voting_channel_.SetVote(
        frame_node,
        Vote(base::Process::Priority::kUserBlocking, kPageIsClosingReason));
  }
}

void ClosingPageVoter::OnBeforeFrameNodeRemoved(const FrameNode* frame_node) {
  if (closing_pages_.contains(frame_node->GetPageNode())) {
    voting_channel_.SetVote(frame_node, std::nullopt);
  }
}

void ClosingPageVoter::SetVoteForSubtree(const FrameNode* frame_node,
                                         const std::optional<Vote>& vote) {
  voting_channel_.SetVote(frame_node, vote);

  // Recurse through subtree.
  for (const FrameNode* child_frame_node : frame_node->GetChildFrameNodes()) {
    SetVoteForSubtree(child_frame_node, vote);
  }
}

}  // namespace performance_manager::execution_context_priority
