// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_EXECUTION_CONTEXT_PRIORITY_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_EXECUTION_CONTEXT_PRIORITY_DECORATOR_H_

#include <memory>

#include "components/performance_manager/execution_context_priority/ad_frame_voter.h"
#include "components/performance_manager/execution_context_priority/child_frame_booster.h"
#include "components/performance_manager/execution_context_priority/frame_audible_voter.h"
#include "components/performance_manager/execution_context_priority/frame_capturing_media_stream_voter.h"
#include "components/performance_manager/execution_context_priority/frame_visibility_voter.h"
#include "components/performance_manager/execution_context_priority/inherit_client_priority_voter.h"
#include "components/performance_manager/execution_context_priority/loading_page_voter.h"
#include "components/performance_manager/execution_context_priority/max_vote_aggregator.h"
#include "components/performance_manager/execution_context_priority/override_vote_aggregator.h"
#include "components/performance_manager/execution_context_priority/root_vote_observer.h"
#include "components/performance_manager/public/graph/graph.h"

#if BUILDFLAG(IS_MAC)
#include "components/performance_manager/execution_context_priority/boosting_vote_aggregator.h"
#endif

namespace performance_manager::execution_context_priority {

// The ExecutionContextPriorityDecorator's responsibility is to own the voting
// system that assigns the priority of every frame and worker in the graph.
//
// See the README.md for more details on the voting system.
class ExecutionContextPriorityDecorator final : public GraphOwned {
 public:
  ExecutionContextPriorityDecorator();
  ~ExecutionContextPriorityDecorator() override;

  ExecutionContextPriorityDecorator(const ExecutionContextPriorityDecorator&) =
      delete;
  ExecutionContextPriorityDecorator& operator=(
      const ExecutionContextPriorityDecorator&) = delete;

 private:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // Takes in the aggregated votes and applies them to the execution contexts in
  // the graph.
  RootVoteObserver root_vote_observer_;

  // Used to cast a negative vote that overrides the vote from
  // |max_vote_aggregator_|.
  OverrideVoteAggregator override_vote_aggregator_;

  // Can be used to express a relationship between 2 execution contextes where
  // one must always have at least the priority of another one.
  BoostingVoteAggregator boosting_vote_aggregator_;

  // Aggregates all the votes from the voters.
  MaxVoteAggregator max_vote_aggregator_;

  // Note: Voters should be added below this line so that they are destroyed
  //       before the aggregators.

  // Casts a downvote for ad frames.
  AdFrameVoter ad_frame_voter_;

  // Casts a USER_VISIBLE vote when a frame is visible.
  FrameVisibilityVoter frame_visibility_voter_;

  // Casts a USER_VISIBLE vote when a frame is audible.
  FrameAudibleVoter frame_audible_voter_;

  // Casts a USER_VISIBLE vote when a frame is capturing a media stream.
  FrameCapturingMediaStreamVoter frame_capturing_media_stream_voter_;

  // Casts a vote for each child worker with the client's priority.
  InheritClientPriorityVoter inherit_client_priority_voter_;

  // Casts a USER_VISIBLE vote for all frames in a loading page.
  LoadingPageVoter loading_page_voter_;

#if BUILDFLAG(IS_MAC)
  //  Boosts the priority of non-ad child frames.
  std::unique_ptr<ChildFrameBooster> child_frame_booster_;
#endif
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_EXECUTION_CONTEXT_PRIORITY_DECORATOR_H_
