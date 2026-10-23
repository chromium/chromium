// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/priority/loading_page_voter.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/process/process.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"

namespace performance_manager::priority {

namespace {

// Walks up embedder frame relationships to locate the outermost browser tab
// PageNode (e.g., out of embedded GuestViews or portals).
const PageNode* GetRootPageNode(const PageNode* page_node) {
  while (page_node && page_node->GetEmbedderFrameNode()) {
    page_node = page_node->GetEmbedderFrameNode()->GetPageNode();
  }
  return page_node;
}

// Returns true if `page_node` is the active tab directly.
bool IsActiveTab(const PageNode* page_node) {
  const auto* live_state =
      page_node ? PageLiveStateDecorator::Data::FromPageNode(page_node)
                : nullptr;
  return live_state && live_state->IsActiveTab();
}

// Returns true if `page_node` (or its outermost embedder root page) is the
// active tab.
bool IsInActiveTab(const PageNode* page_node) {
  return IsActiveTab(GetRootPageNode(page_node));
}

// Invokes `function` on `page_node` and all embedded subpages recursively.
// Embedded pages track their own IsLoading() state independently, but share
// the IsActiveTab() status of the outermost root tab.
template <typename Function>
void ForEachPageInSubtree(const PageNode* page_node, Function&& function) {
  function(page_node);
  GraphOperations::VisitFrameTreePreOrder(
      page_node, [&](const FrameNode* frame_node) {
        for (const PageNode* embedded_page :
             frame_node->GetEmbeddedPageNodes()) {
          ForEachPageInSubtree(embedded_page, function);
        }
        return true;
      });
}

// Pages loading in an active tab always get kUserBlocking.
Vote GetActiveTabVote() {
  return Vote(base::Process::Priority::kUserBlocking,
              LoadingPageVoter::kPageIsLoadingReason);
}

// Background loading pages get kUserVisible when voted for.
Vote GetBackgroundVote() {
  return Vote(base::Process::Priority::kUserVisible,
              LoadingPageVoter::kPageIsLoadingReason);
}

}  // namespace

// static
const char LoadingPageVoter::kPageIsLoadingReason[] = "Page is loading.";

LoadingPageVoter::LoadingPageVoter(bool restrict_background_votes)
    : restrict_background_votes_(restrict_background_votes) {}

LoadingPageVoter::~LoadingPageVoter() = default;

void LoadingPageVoter::InitializeOnGraph(Graph* graph,
                                         VotingChannel voting_channel) {
  CHECK(graph->HasOnlySystemNode());
  graph_ = graph;
  voting_channel_ = std::move(voting_channel);

  graph->AddPageNodeObserver(this);
}

void LoadingPageVoter::TearDownOnGraph(Graph* graph) {
  CHECK_EQ(graph, graph_);
  graph->RemovePageNodeObserver(this);

  active_tab_load_timers_.clear();
  voting_channel_.Reset();
  graph_ = nullptr;
}

void LoadingPageVoter::OnPageNodeAdded(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)->AddObserver(
      this);
  // A new page isn't loading yet (see OnLoadingStateChanged()).
  CHECK(!IsLoading(page_node->GetLoadingState()));
}

void LoadingPageVoter::OnBeforePageNodeRemoved(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->RemoveObserver(this);
  if (IsLoading(page_node->GetLoadingState())) {
    voting_channel_.SetVote(page_node, std::nullopt);
    if (IsInActiveTab(page_node)) {
      OnActiveTabLoadStopped(page_node);
    }
  }
  CHECK(!active_tab_load_timers_.contains(page_node));
}

void LoadingPageVoter::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  const bool is_loading = IsLoading(page_node->GetLoadingState());
  if (IsLoading(previous_state) == is_loading) {
    return;
  }

  const bool is_in_active_tab = IsInActiveTab(page_node);
  if (!is_loading) {
    voting_channel_.SetVote(page_node, std::nullopt);
    if (is_in_active_tab) {
      OnActiveTabLoadStopped(page_node);
    }
    return;
  }

  if (is_in_active_tab) {
    voting_channel_.SetVote(page_node, GetActiveTabVote());
    OnActiveTabLoadStarted(page_node);
  } else if (ShouldVoteForBackgroundPage(page_node)) {
    voting_channel_.SetVote(page_node, GetBackgroundVote());
  }
}

void LoadingPageVoter::OnIsUserOrBrowserInitiatedLoadChanged(
    const PageNode* page_node) {
  // Only background loading pages are affected.
  if (!IsLoading(page_node->GetLoadingState()) || IsInActiveTab(page_node)) {
    return;
  }
  // Happens when a navigation starts after the page started loading
  // (DidStartLoading() precedes DidStartNavigation()), or supersedes an
  // in-flight one.
  if (ShouldVoteForBackgroundPage(page_node)) {
    voting_channel_.SetVote(page_node, GetBackgroundVote());
  } else {
    voting_channel_.SetVote(page_node, std::nullopt);
  }
}

void LoadingPageVoter::OnEmbedderFrameNodeChanged(
    const PageNode* page_node,
    const FrameNode* previous_embedder) {
  const bool was_in_active_tab =
      previous_embedder ? IsInActiveTab(previous_embedder->GetPageNode())
                        : IsActiveTab(page_node);
  const bool is_in_active_tab = IsInActiveTab(page_node);
  if (was_in_active_tab != is_in_active_tab) {
    OnSubtreeActiveTabChanged(page_node, is_in_active_tab);
  }
}

void LoadingPageVoter::OnIsActiveTabChanged(const PageNode* page_node) {
  if (page_node->GetEmbedderFrameNode()) {
    return;
  }
  // Note: When switching between two loading tabs, the previous active tab is
  // deactivated before the new one is activated, which briefly votes for
  // background loading pages.
  OnSubtreeActiveTabChanged(page_node, IsActiveTab(page_node));
}

bool LoadingPageVoter::IsLoading(PageNode::LoadingState loading_state) const {
  switch (loading_state) {
    case PageNode::LoadingState::kLoading:
      return true;
    case PageNode::LoadingState::kLoadingTimedOut:
      // The navigation hasn't committed yet, but the load is still in
      // progress. Consider it loading in `restrict_background_votes_` mode.
      return restrict_background_votes_;
    case PageNode::LoadingState::kLoadedBusy:
      // The page finished loading but is still busy (CPU or network) for up
      // to a minute, which may be unrelated to the load. Consider it loading
      // when not in `restrict_background_votes_` mode.
      return !restrict_background_votes_;
    case PageNode::LoadingState::kLoadingNotStarted:
    case PageNode::LoadingState::kLoadedIdle:
      return false;
  }
}

bool LoadingPageVoter::ShouldVoteForBackgroundPage(
    const PageNode* page_node) const {
  CHECK(IsLoading(page_node->GetLoadingState()));
  CHECK(!IsInActiveTab(page_node));

  // All background loading pages get a vote when not in
  // `restrict_background_votes_` mode.
  if (!restrict_background_votes_) {
    return true;
  }

  // Don't vote for background pages while they compete with a load in an
  // active tab.
  if (!active_tab_load_timers_.empty()) {
    return false;
  }

  // Only vote for loads that the user or the browser asked for. Use the
  // loading page's own initiator, since embedded pages load independently of
  // their root page.
  if (!page_node->IsUserOrBrowserInitiatedLoad()) {
    return false;
  }

  return true;
}

void LoadingPageVoter::OnSubtreeActiveTabChanged(
    const PageNode* root_or_subpage,
    bool is_in_active_tab) {
  std::vector<const PageNode*> loading_pages;
  ForEachPageInSubtree(root_or_subpage, [&](const PageNode* page_node) {
    if (IsLoading(page_node->GetLoadingState())) {
      loading_pages.push_back(page_node);
    }
  });

  if (is_in_active_tab) {
    for (const PageNode* page_node : loading_pages) {
      voting_channel_.SetVote(page_node, GetActiveTabVote());
      OnActiveTabLoadStarted(page_node);
    }
  } else {
    for (const PageNode* page_node : loading_pages) {
      OnActiveTabLoadStopped(page_node);
    }
    for (const PageNode* page_node : loading_pages) {
      if (ShouldVoteForBackgroundPage(page_node)) {
        voting_channel_.SetVote(page_node, GetBackgroundVote());
      } else {
        voting_channel_.SetVote(page_node, std::nullopt);
      }
    }
  }
}

void LoadingPageVoter::OnActiveTabLoadStarted(const PageNode* page_node) {
  if (!restrict_background_votes_) {
    return;
  }
  auto [it, inserted] = active_tab_load_timers_.try_emplace(page_node);
  CHECK(inserted);
  // Unretained is safe because `this` owns the timer, and the timer is
  // destroyed when `page_node` is removed from `active_tab_load_timers_`.
  it->second = std::make_unique<base::OneShotTimer>();
  it->second->Start(
      FROM_HERE, kActiveTabLoadTimeout,
      base::BindOnce(&LoadingPageVoter::OnActiveTabLoadTimeout,
                     base::Unretained(this), base::Unretained(page_node)));
  // If this is the first active loading page, clear votes on background pages.
  if (active_tab_load_timers_.size() == 1u) {
    ClearVotesForBackgroundPages();
  }
}

void LoadingPageVoter::OnActiveTabLoadStopped(const PageNode* page_node) {
  if (!restrict_background_votes_) {
    return;
  }
  // `page_node` may already be absent if its timer expired in
  // OnActiveTabLoadTimeout().
  if (active_tab_load_timers_.erase(page_node) &&
      active_tab_load_timers_.empty()) {
    SetVotesForBackgroundPages();
  }
}

void LoadingPageVoter::OnActiveTabLoadTimeout(const PageNode* page_node) {
  // Destroys the timer running this callback, which is allowed.
  size_t removed = active_tab_load_timers_.erase(page_node);
  CHECK(removed);
  if (active_tab_load_timers_.empty()) {
    SetVotesForBackgroundPages();
  }
}

void LoadingPageVoter::SetVotesForBackgroundPages() {
  for (const PageNode* page_node : graph_->GetAllPageNodes()) {
    if (IsLoading(page_node->GetLoadingState()) && !IsInActiveTab(page_node) &&
        ShouldVoteForBackgroundPage(page_node)) {
      voting_channel_.SetVote(page_node, GetBackgroundVote());
    }
  }
}

void LoadingPageVoter::ClearVotesForBackgroundPages() {
  for (const PageNode* page_node : graph_->GetAllPageNodes()) {
    if (IsLoading(page_node->GetLoadingState()) && !IsInActiveTab(page_node)) {
      voting_channel_.SetVote(page_node, std::nullopt);
    }
  }
}

}  // namespace performance_manager::priority
