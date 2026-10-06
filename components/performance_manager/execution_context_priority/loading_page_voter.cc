// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/loading_page_voter.h"

#include <utility>

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

namespace {

// Walks up embedder frame relationships to locate the outermost browser tab
// PageNode (e.g., out of embedded GuestViews or portals).
const PageNode* GetRootPageNode(const PageNode* page_node) {
  while (page_node && page_node->GetEmbedderFrameNode()) {
    page_node = page_node->GetEmbedderFrameNode()->GetPageNode();
  }
  return page_node;
}

}  // namespace

// static
const char LoadingPageVoter::kPageIsLoadingReason[] = "Page is loading.";

LoadingPageVoter::LoadingPageVoter(bool boost_only_requested_background_loads)
    : boost_only_requested_background_loads_(
          boost_only_requested_background_loads) {}

LoadingPageVoter::~LoadingPageVoter() = default;

void LoadingPageVoter::InitializeOnGraph(Graph* graph,
                                         VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);

  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);

  CHECK(graph->HasOnlySystemNode());
}

void LoadingPageVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);

  voting_channel_.Reset();
}

void LoadingPageVoter::OnPageNodeAdded(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)->AddObserver(
      this);
  if (IsLoading(page_node->GetLoadingState())) {
    UpdateVotesForPage(page_node);
  }
}

void LoadingPageVoter::OnBeforePageNodeRemoved(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->RemoveObserver(this);
}

void LoadingPageVoter::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  if (IsLoading(previous_state) != IsLoading(page_node->GetLoadingState())) {
    UpdateVotesForPage(page_node);
  }
}

void LoadingPageVoter::OnIsUserOrBrowserInitiatedLoadChanged(
    const PageNode* page_node) {
  if (!boost_only_requested_background_loads_) {
    // The original behavior doesn't depend on the initiator.
    return;
  }
  // Happens when a navigation starts after the page started loading
  // (DidStartLoading() precedes DidStartNavigation()), or supersedes an
  // in-flight one.
  UpdateVotesForPage(page_node);
}

void LoadingPageVoter::OnEmbedderFrameNodeChanged(
    const PageNode* page_node,
    const FrameNode* previous_embedder) {
  const bool was_active_tab =
      previous_embedder ? IsRootPageActiveTab(previous_embedder->GetPageNode())
                        : IsPageActiveTab(page_node);
  const bool is_active_tab = IsRootPageActiveTab(page_node);
  if (was_active_tab != is_active_tab) {
    ChangeVotesForPageAndSubpages(page_node, is_active_tab);
  }
}

void LoadingPageVoter::OnIsActiveTabChanged(const PageNode* page_node) {
  if (page_node->GetEmbedderFrameNode()) {
    return;
  }
  ChangeVotesForPageAndSubpages(page_node, IsPageActiveTab(page_node));
}

void LoadingPageVoter::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  if (!IsLoading(pending_page_node->GetLoadingState())) {
    return;
  }

  voting_channel_.SetVote(
      frame_node,
      GetVote(pending_page_node, IsRootPageActiveTab(pending_page_node)));
}

void LoadingPageVoter::OnBeforeFrameNodeRemoved(const FrameNode* frame_node) {
  const PageNode* page_node = frame_node->GetPageNode();
  if (!IsLoading(page_node->GetLoadingState())) {
    return;
  }
  voting_channel_.SetVote(frame_node, std::nullopt);
}

bool LoadingPageVoter::IsLoading(PageNode::LoadingState loading_state) const {
  switch (loading_state) {
    case PageNode::LoadingState::kLoading:
    case PageNode::LoadingState::kLoadedBusy:
      return true;
    case PageNode::LoadingState::kLoadingTimedOut:
      // The navigation hasn't committed yet, but the load is still in
      // progress. The original behavior doesn't consider it loading.
      return boost_only_requested_background_loads_;
    case PageNode::LoadingState::kLoadingNotStarted:
    case PageNode::LoadingState::kLoadedIdle:
      return false;
  }
}

bool LoadingPageVoter::IsPageActiveTab(const PageNode* page_node) const {
  const auto* live_state =
      page_node ? PageLiveStateDecorator::Data::FromPageNode(page_node)
                : nullptr;
  return live_state && live_state->IsActiveTab();
}

bool LoadingPageVoter::IsRootPageActiveTab(const PageNode* page_node) const {
  return IsPageActiveTab(GetRootPageNode(page_node));
}

std::optional<Vote> LoadingPageVoter::GetVote(
    const PageNode* page_node,
    bool is_root_page_active_tab) const {
  if (is_root_page_active_tab) {
    return Vote(base::Process::Priority::kUserBlocking, kPageIsLoadingReason);
  }
  // In a background tab, only boost loads that the user or the browser asked
  // for (unless running the original behavior). Use the loading page's own
  // initiator, since embedded pages load independently of their root page.
  if (!boost_only_requested_background_loads_ ||
      page_node->IsUserOrBrowserInitiatedLoad()) {
    return Vote(base::Process::Priority::kUserVisible, kPageIsLoadingReason);
  }
  return std::nullopt;
}

void LoadingPageVoter::UpdateVotesForPage(const PageNode* page_node) {
  const std::optional<Vote> vote =
      IsLoading(page_node->GetLoadingState())
          ? GetVote(page_node, IsRootPageActiveTab(page_node))
          : std::nullopt;
  for (const FrameNode* main_frame_node : page_node->GetMainFrameNodes()) {
    SetVoteForSubtree(main_frame_node, vote);
  }
}

void LoadingPageVoter::ChangeVotesForPageAndSubpages(
    const PageNode* page_node,
    bool is_root_page_active_tab) {
  for (const FrameNode* main_frame : page_node->GetMainFrameNodes()) {
    ChangeVotesForFrameSubtree(main_frame, is_root_page_active_tab);
  }
}

void LoadingPageVoter::SetVoteForSubtree(const FrameNode* frame_node,
                                         const std::optional<Vote>& vote) {
  voting_channel_.SetVote(frame_node, vote);

  // Recurse through subtree.
  for (const FrameNode* child_frame_node : frame_node->GetChildFrameNodes()) {
    SetVoteForSubtree(child_frame_node, vote);
  }
}

void LoadingPageVoter::ChangeVotesForFrameSubtree(
    const FrameNode* frame_node,
    bool is_root_page_active_tab) {
  const PageNode* page_node = frame_node->GetPageNode();
  if (IsLoading(page_node->GetLoadingState())) {
    voting_channel_.SetVote(frame_node,
                            GetVote(page_node, is_root_page_active_tab));
  }

  // Recurse through subtree.
  for (const FrameNode* child_frame_node : frame_node->GetChildFrameNodes()) {
    ChangeVotesForFrameSubtree(child_frame_node, is_root_page_active_tab);
  }

  // Recurse into embedded subpages. Embedded pages track their own IsLoading()
  // state independently (so they submit their own initial votes), but they
  // share the IsActiveTab() status of the outermost root tab.
  for (const PageNode* embedded_page : frame_node->GetEmbeddedPageNodes()) {
    ChangeVotesForPageAndSubpages(embedded_page, is_root_page_active_tab);
  }
}

}  // namespace performance_manager::execution_context_priority
