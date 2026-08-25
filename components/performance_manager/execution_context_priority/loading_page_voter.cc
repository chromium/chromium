// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/loading_page_voter.h"

#include <utility>

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

namespace {

// Returns true if `loading_state` represent an actively loading state.
bool IsLoading(PageNode::LoadingState loading_state) {
  return loading_state == PageNode::LoadingState::kLoading ||
         loading_state == PageNode::LoadingState::kLoadedBusy;
}

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

LoadingPageVoter::LoadingPageVoter() = default;

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
    OnPageNodeStartedLoading(page_node);
  }
}

void LoadingPageVoter::OnBeforePageNodeRemoved(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->RemoveObserver(this);
}

void LoadingPageVoter::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  const bool was_loading = IsLoading(previous_state);
  const bool is_loading = IsLoading(page_node->GetLoadingState());

  if (was_loading && !is_loading) {
    OnPageNodeStoppedLoading(page_node);
  }
  if (!was_loading && is_loading) {
    OnPageNodeStartedLoading(page_node);
  }
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
      frame_node, Vote(GetPriority(IsRootPageActiveTab(pending_page_node)),
                       kPageIsLoadingReason));
}

void LoadingPageVoter::OnBeforeFrameNodeRemoved(const FrameNode* frame_node) {
  const PageNode* page_node = frame_node->GetPageNode();
  if (!IsLoading(page_node->GetLoadingState())) {
    return;
  }
  voting_channel_.SetVote(frame_node, std::nullopt);
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

base::Process::Priority LoadingPageVoter::GetPriority(
    bool is_root_page_active_tab) const {
  return is_root_page_active_tab ? base::Process::Priority::kUserBlocking
                                 : base::Process::Priority::kUserVisible;
}

void LoadingPageVoter::OnPageNodeStartedLoading(const PageNode* page_node) {
  const Vote vote(GetPriority(IsRootPageActiveTab(page_node)),
                  kPageIsLoadingReason);
  for (const FrameNode* main_frame_node : page_node->GetMainFrameNodes()) {
    SetVoteForSubtree(main_frame_node, vote);
  }
}

void LoadingPageVoter::OnPageNodeStoppedLoading(const PageNode* page_node) {
  for (const FrameNode* main_frame_node : page_node->GetMainFrameNodes()) {
    SetVoteForSubtree(main_frame_node, std::nullopt);
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
    voting_channel_.SetVote(
        frame_node,
        Vote(GetPriority(is_root_page_active_tab), kPageIsLoadingReason));
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
