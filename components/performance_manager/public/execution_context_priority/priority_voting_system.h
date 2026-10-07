// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_PRIORITY_VOTING_SYSTEM_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_PRIORITY_VOTING_SYSTEM_H_

#include <memory>
#include <utility>
#include <vector>

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/max_vote_aggregator.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/worker_node.h"

namespace performance_manager {

class Graph;

namespace execution_context_priority {

class PrioritySetter;

// Base interface for creating a voter class that can submit a vote to influence
// the priority of a graph node.
//
// Use `PriorityVotingSystem::AddPriorityVoter()` to register your voter.
class PriorityVoter {
 public:
  virtual ~PriorityVoter() = default;

  // Initializes the voter on the graph. Used to register as observers, and to
  // initialize the VotingChannel.
  virtual void InitializeOnGraph(Graph* graph,
                                 VotingChannel voting_channel) = 0;

  // Tears down the voter on the graph. Used to unregister as observers.
  virtual void TearDownOnGraph(Graph* graph) = 0;
};

// This class owns the voters that are responsible for deciding the priority of
// graph nodes, and the MaxVoteAggregator that aggregates their votes. The top
// vote of a node in the aggregator is its final priority. Each component is
// built from the aggregator, observes the final priority of nodes in it, and/or
// casts votes into it:
//   - The PrioritySetter sets the priority of frames and workers.
//
// It also verifies that no votes are leaked.
//
// See README.md for an overview.
class PriorityVotingSystem
    : public GraphOwnedAndRegistered<PriorityVotingSystem>,
      private FrameNodeObserver,
      private WorkerNodeObserver {
 public:
  PriorityVotingSystem();
  ~PriorityVotingSystem() override;

  // Adds a new PriorityVoter to the graph.
  template <class T, class... Args>
  void AddPriorityVoter(Args&&... args) {
    AddPriorityVoter(std::make_unique<T>(std::forward<Args>(args)...));
  }

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 private:
  void AddPriorityVoter(std::unique_ptr<PriorityVoter> priority_voter);

  // FrameNodeObserver:
  void OnFrameNodeRemoved(
      const FrameNode* frame_node,
      const FrameNode* previous_parent_frame_node,
      const PageNode* previous_page_node,
      const ProcessNode* previous_process_node,
      const FrameNode* previous_parent_or_outer_document_or_embedder) override;

  // WorkerNodeObserver:
  void OnWorkerNodeRemoved(const WorkerNode* worker_node,
                           const ProcessNode* previous_process_node) override;

  // Aggregates the votes from the voters. Declared first, since everything else
  // observes it or holds voting channels issued by it.
  MaxVoteAggregator max_vote_aggregator_;

  // Observes `max_vote_aggregator_`.
  std::unique_ptr<PrioritySetter> priority_setter_;

  std::vector<std::unique_ptr<PriorityVoter>> priority_voters_;
};

}  // namespace execution_context_priority

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_PRIORITY_VOTING_SYSTEM_H_
