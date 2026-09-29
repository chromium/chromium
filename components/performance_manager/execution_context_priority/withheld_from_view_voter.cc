// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/withheld_from_view_voter.h"

#include <optional>
#include <utility>

#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"

namespace performance_manager::execution_context_priority {

// static
const char WithheldFromViewVoter::kWithheldFromViewReason[] =
    "Page is withheld from view.";

WithheldFromViewVoter::WithheldFromViewVoter(bool ignore_main_frame_visibility)
    : ignore_main_frame_visibility_(ignore_main_frame_visibility) {}

WithheldFromViewVoter::~WithheldFromViewVoter() = default;

void WithheldFromViewVoter::InitializeOnGraph(Graph* graph,
                                              VotingChannel voting_channel) {
  // Pages that already exist would never have their PageLiveState observer
  // registered by OnPageNodeAdded(), so this voter must be installed before
  // any page exists.
  CHECK(graph->HasOnlySystemNode());

  voting_channel_ = std::move(voting_channel);
  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);
}

void WithheldFromViewVoter::TearDownOnGraph(Graph* graph) {
  // Mirrors the registration done in OnPageNodeAdded(). Normally the graph is
  // empty by now, but don't depend on teardown ordering.
  for (const PageNode* page_node : graph->GetAllPageNodes()) {
    PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
        ->RemoveObserver(this);
  }

  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  voting_channel_.Reset();
}

// static
bool WithheldFromViewVoter::IsPageWithheldFromView(const PageNode* page_node) {
  const auto* live_state =
      PageLiveStateDecorator::Data::FromPageNode(page_node);
  return live_state && live_state->IsWithheldFromView();
}

bool WithheldFromViewVoter::ShouldVoteForFrame(
    const FrameNode* frame_node) const {
  return !(frame_node->IsMainFrame() && ignore_main_frame_visibility_);
}

void WithheldFromViewVoter::OnIsWithheldFromViewChanged(
    const PageNode* page_node) {
  const bool is_withheld = IsPageWithheldFromView(page_node);

  // The whole page is withheld, so every frame in it is waiting to be shown,
  // not just the main frame. A cross-process subframe must load too before the
  // embedder can display the page.
  for (const FrameNode* frame_node :
       GraphOperations::GetFrameNodes(page_node)) {
    if (!ShouldVoteForFrame(frame_node)) {
      continue;
    }
    voting_channel_.SetVote(
        frame_node, is_withheld ? std::make_optional<Vote>(
                                      base::Process::Priority::kUserBlocking,
                                      kWithheldFromViewReason)
                                : std::nullopt);
  }
}

void WithheldFromViewVoter::OnPageNodeAdded(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)->AddObserver(
      this);
}

void WithheldFromViewVoter::OnBeforePageNodeRemoved(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->RemoveObserver(this);
}

void WithheldFromViewVoter::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  if (!ShouldVoteForFrame(frame_node)) {
    return;
  }
  if (!IsPageWithheldFromView(pending_page_node)) {
    return;
  }
  voting_channel_.SetVote(
      frame_node,
      Vote(base::Process::Priority::kUserBlocking, kWithheldFromViewReason));
}

void WithheldFromViewVoter::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  if (!ShouldVoteForFrame(frame_node)) {
    return;
  }
  voting_channel_.SetVote(frame_node, std::nullopt);
}

}  // namespace performance_manager::execution_context_priority
