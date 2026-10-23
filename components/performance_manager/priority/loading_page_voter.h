// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PRIORITY_LOADING_PAGE_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PRIORITY_LOADING_PAGE_VOTER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/priority/priority.h"
#include "components/performance_manager/public/priority/priority_voting_system.h"

namespace performance_manager {

class FrameNode;
class Graph;

namespace priority {

// This voter casts a Process::Priority::kUserBlocking vote on a loading page if
// it is the active tab, or a Process::Priority::kUserVisible vote if it is not
// the active tab. The vote is cast on the PageNode, and thus applies to all of
// its frames. This makes loading in the active tab fast while preventing
// background loads from freezing.
//
// With `restrict_background_votes` (features::kPMLoadingPageVoterV2), a vote is
// only cast for a background loading page if:
// - The load was user- or browser-initiated (see
//   PageNode::IsUserOrBrowserInitiatedLoad()), to avoid voting for unsolicited
//   background navigations.
// - No load is in progress in an active tab, so that they don't compete with
//   the loads the user is waiting for. Since a load may never finish, a page
//   stops preventing votes for background pages after kActiveTabLoadTimeout
//   (the page keeps its kUserBlocking vote).
class LoadingPageVoter : public PriorityVoter,
                         public PageNodeObserver,
                         public PageLiveStateObserver {
 public:
  static const char kPageIsLoadingReason[];

  // Maximum duration for which a page loading in an active tab prevents votes
  // for background pages, in case that load never finishes. Only used with
  // `restrict_background_votes`.
  static constexpr base::TimeDelta kActiveTabLoadTimeout = base::Seconds(30);

  // If `restrict_background_votes` is false, the voter uses its original
  // behavior (features::kPMLoadingPageVoter): a vote is cast for every
  // background loading page regardless of its initiator and of active tab
  // loads, and kLoadedBusy (instead of kLoadingTimedOut) is considered loading.
  // Kept only for diagnostics, to be removed when
  // features::kPMLoadingPageVoterV2 launches.
  explicit LoadingPageVoter(bool restrict_background_votes);
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
  void OnIsUserOrBrowserInitiatedLoadChanged(
      const PageNode* page_node) override;
  void OnEmbedderFrameNodeChanged(const PageNode* page_node,
                                  const FrameNode* previous_embedder) override;

  // PageLiveStateObserver:
  void OnIsActiveTabChanged(const PageNode* page_node) override;

  VoterId voter_id() const { return voting_channel_.voter_id(); }

 private:
  // Returns true if `loading_state` represents an actively loading state.
  bool IsLoading(PageNode::LoadingState loading_state) const;

  // Returns true if this voter should cast a vote for a loading background
  // `page_node`.
  bool ShouldVoteForBackgroundPage(const PageNode* page_node) const;

  // Invoked when `page_node` enters or leaves an active tab (either because a
  // root tab's IsActiveTab() changed, or an embedded page's embedder changed).
  // Updates votes and timers for `page_node` and any pages embedded within it,
  // since embedded pages inherit their embedder's active-tab status.
  void OnSubtreeActiveTabChanged(const PageNode* page_node,
                                 bool is_in_active_tab);

  // Invoked when `page_node` starts or stops loading in an active tab (either
  // because its loading state changed or because it entered/left an active
  // tab). Manages the kActiveTabLoadTimeout timer and clears/sets votes for
  // background pages as needed.
  void OnActiveTabLoadStarted(const PageNode* page_node);
  void OnActiveTabLoadStopped(const PageNode* page_node);

  // Invoked when `page_node` has been loading in an active tab for
  // kActiveTabLoadTimeout, so that it stops preventing votes for background
  // pages.
  void OnActiveTabLoadTimeout(const PageNode* page_node);

  // Sets or clears votes on background loading pages when
  // `active_tab_load_timers_` becomes empty or non-empty.
  void SetVotesForBackgroundPages();
  void ClearVotesForBackgroundPages();

  // If true, votes are only cast for background loading pages when the load was
  // requested and doesn't compete with active tab loads
  // (features::kPMLoadingPageVoterV2).
  const bool restrict_background_votes_;

  raw_ptr<const Graph> graph_ = nullptr;

  // Running kActiveTabLoadTimeout timers for pages loading in an active tab
  // that currently prevent votes for background pages. Each entry is removed
  // when its timer fires, when the page stops loading, when it leaves the
  // active tab, or when the page is removed. Only populated when
  // `restrict_background_votes_` is true.
  base::flat_map<raw_ptr<const PageNode>, std::unique_ptr<base::OneShotTimer>>
      active_tab_load_timers_;

  VotingChannel voting_channel_;
};

}  // namespace priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PRIORITY_LOADING_PAGE_VOTER_H_
