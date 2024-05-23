// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_CHILD_FRAME_BOOSTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_CHILD_FRAME_BOOSTER_H_

#include <map>

#include "components/performance_manager/execution_context_priority/boosting_vote_aggregator.h"
#include "components/performance_manager/execution_context_priority/voter_base.h"
#include "components/performance_manager/graph/initializing_frame_node_observer.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/graph/frame_node.h"

namespace performance_manager::execution_context_priority {

class BoostingVoteAggregator;

// This class is used to boost the priority of a child frame when its parent's
// priority is higher.
//
// This is needed to correctly support the use case of using a non-visible
// cross-origin frame to sandbox some of the work that a web application wants
// to do (See https://crbug.com/336161235 for example).
//
// This is only done for non-ad frames to reduce the amount of unnecessary
// boosting.
class ChildFrameBooster : public VoterBase,
                          public InitializingFrameNodeObserver {
 public:
  static const char kChildFrameBoostReason[];

  explicit ChildFrameBooster(BoostingVoteAggregator* boosting_vote_aggregator);
  ~ChildFrameBooster() override;

  ChildFrameBooster(const ChildFrameBooster&) = delete;
  ChildFrameBooster& operator=(const ChildFrameBooster&) = delete;

  // VoterBase:
  void InitializeOnGraph(Graph* graph) override;
  void TearDownOnGraph(Graph* graph) override;

  // FrameNodeObserver:
  void OnFrameNodeInitializing(const FrameNode* frame_node) override;
  void OnFrameNodeTearingDown(const FrameNode* frame_node) override;
  void OnIsAdFrameChanged(const FrameNode* frame_node) override;

 private:
  // Creates a BoostingVote, which will ensure that `frame_node` always has a
  // priority higher or equal than its parent.
  void CreateBoostingVote(const FrameNode* frame_node);

  // Deletes an existing BoostingVote for `frame_node`.
  void DeleteBoostingVote(const FrameNode* frame_node);

  // The aggregator that takes care of casting the right vote to ensure the
  // child frame's priority is boosted.
  raw_ptr<BoostingVoteAggregator> boosting_vote_aggregator_;

  // Each frame can potentially be boosted to match it's parent priority by
  // creating one BoostingVote.
  std::map<const FrameNode*, BoostingVote> boosting_votes_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_CHILD_FRAME_BOOSTER_H_
