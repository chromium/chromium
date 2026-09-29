// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_WITHHELD_FROM_VIEW_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_WITHHELD_FROM_VIEW_VOTER_H_

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/voting/voting.h"

namespace performance_manager::execution_context_priority {

// Casts a kUserBlocking vote for every frame of a page that an embedder is
// withholding from view until it is ready to be shown (see
// PageLiveStateDecorator::MarkWithheldFromView). Without this vote, the hidden
// page would be treated as a background page and deprioritized while loading,
// delaying when it can be made visible.
class WithheldFromViewVoter : public PriorityVoter,
                              public PageLiveStateObserver,
                              public PageNodeObserver,
                              public FrameNodeObserver {
 public:
  static const char kWithheldFromViewReason[];

  // If `ignore_main_frame_visibility` is true, this voter will not cast votes
  // for main frames, matching FrameVisibilityVoter.
  explicit WithheldFromViewVoter(bool ignore_main_frame_visibility);
  ~WithheldFromViewVoter() override;

  WithheldFromViewVoter(const WithheldFromViewVoter&) = delete;
  WithheldFromViewVoter& operator=(const WithheldFromViewVoter&) = delete;

  // PriorityVoter:
  void InitializeOnGraph(Graph* graph, VotingChannel voting_channel) override;
  void TearDownOnGraph(Graph* graph) override;

  // PageLiveStateObserver:
  void OnIsWithheldFromViewChanged(const PageNode* page_node) override;

  // PageNodeObserver:
  void OnPageNodeAdded(const PageNode* page_node) override;
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
  // Returns whether `page_node`'s embedder is currently withholding it.
  static bool IsPageWithheldFromView(const PageNode* page_node);

  // Returns whether this voter casts votes for `frame_node` at all.
  bool ShouldVoteForFrame(const FrameNode* frame_node) const;

  const bool ignore_main_frame_visibility_;

  VotingChannel voting_channel_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_WITHHELD_FROM_VIEW_VOTER_H_
