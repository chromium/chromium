// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_CLOSING_PAGE_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_CLOSING_PAGE_VOTER_H_

#include "base/containers/flat_map.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/voting/voting.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace performance_manager::execution_context {
class ExecutionContext;
}

namespace performance_manager::execution_context_priority {

// This voter boosts the priority of a page that is closing.
class ClosingPageVoter : public PriorityVoter, public PageNodeObserver {
 public:
  static const char kPageIsClosingReason[];

  ClosingPageVoter();
  ~ClosingPageVoter() override;

  ClosingPageVoter(const ClosingPageVoter&) = delete;
  ClosingPageVoter& operator=(const ClosingPageVoter&) = delete;

  // PriorityVoter implementation:
  void InitializeOnGraph(Graph* graph, VotingChannel voting_channel) override;
  void TearDownOnGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnBeforePageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsClosingChanged(const PageNode* page_node) override;

  VoterId voter_id() const { return voting_channel_.voter_id(); }

 private:
  VotingChannel voting_channel_;

  // Stores the active votes submitted by this voter.
  absl::flat_hash_map<const PageNode*,
                      const execution_context::ExecutionContext*>
      active_votes_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_CLOSING_PAGE_VOTER_H_
