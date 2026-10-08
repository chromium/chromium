// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/closing_page_voter.h"

#include <optional>
#include <utility>

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
    CHECK_EQ(num_removed, 1U);
  }

  voting_channel_.SetVote(
      page_node, is_closing ? std::make_optional<Vote>(
                                  base::Process::Priority::kUserBlocking,
                                  kPageIsClosingReason)
                            : std::nullopt);
}

void ClosingPageVoter::InitializeOnGraph(Graph* graph,
                                         VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);
  graph->AddPageNodeObserver(this);
}

void ClosingPageVoter::TearDownOnGraph(Graph* graph) {
  graph->RemovePageNodeObserver(this);
  voting_channel_.Reset();
}

void ClosingPageVoter::OnBeforePageNodeRemoved(const PageNode* page_node) {
  // Assume that the page has no more frames.
  CHECK(page_node->GetMainFrameNodes().empty());

  // Stop tracking the closing state for the page on removal.
  if (closing_pages_.erase(page_node)) {
    voting_channel_.SetVote(page_node, std::nullopt);
  }
}

}  // namespace performance_manager::execution_context_priority
