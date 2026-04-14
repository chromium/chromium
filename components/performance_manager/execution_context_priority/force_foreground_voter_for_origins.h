// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FORCE_FOREGROUND_VOTER_FOR_ORIGINS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FORCE_FOREGROUND_VOTER_FOR_ORIGINS_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/voting/voting.h"
#include "components/url_matcher/url_matcher.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace performance_manager::execution_context_priority {

// This voter boosts the priority of all frames and workers that match a given
// list of URL patterns.
class ForceForegroundVoterForOrigins
    : public PriorityVoter,
      public FrameNodeObserver,
      public WorkerNodeObserver,
      public GraphRegisteredImpl<ForceForegroundVoterForOrigins> {
 public:
  static const char kForceForegroundReason[];

  ForceForegroundVoterForOrigins();
  ~ForceForegroundVoterForOrigins() override;

  ForceForegroundVoterForOrigins(const ForceForegroundVoterForOrigins&) =
      delete;
  ForceForegroundVoterForOrigins& operator=(
      const ForceForegroundVoterForOrigins&) = delete;

  // Sets the URL patterns for a given browser context. Any frame or worker
  // associated with this browser context that matches one of these patterns
  // will be foregrounded.
  void SetPatternsForProfile(const std::string& browser_context_id,
                             const base::ListValue& patterns);

  // Clear the patterns for a given browser context.
  void ClearPatternsForProfile(const std::string& browser_context_id);

  // PriorityVoter:
  void InitializeOnGraph(Graph* graph, VotingChannel voting_channel) override;
  void TearDownOnGraph(Graph* graph) override;

  // FrameNodeObserver:
  void OnBeforeFrameNodeAdded(
      const FrameNode* frame_node,
      const FrameNode* pending_parent_frame_node,
      const PageNode* pending_page_node,
      const ProcessNode* pending_process_node,
      const FrameNode* pending_parent_or_outer_document_or_embedder) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnURLChanged(const FrameNode* frame_node,
                    const GURL& previous_value) override;

  // WorkerNodeObserver:
  void OnBeforeWorkerNodeAdded(
      const WorkerNode* worker_node,
      const ProcessNode* pending_process_node) override;
  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) override;
  void OnFinalResponseURLDetermined(const WorkerNode* worker_node) override;

  VoterId voter_id() const { return voting_channel_.voter_id(); }

 private:
  void RequestForeground(const FrameNode* frame_node);
  void RequestForeground(const WorkerNode* worker_node);
  void ReleaseForeground(const FrameNode* frame_node);
  void ReleaseForeground(const WorkerNode* worker_node);

  void RequestForeground(
      const execution_context::ExecutionContext* execution_context);
  void ReleaseForeground(
      const execution_context::ExecutionContext* execution_context);

  bool ShouldBoost(const std::string& browser_context_id,
                   const GURL& url) const;

  raw_ptr<Graph> graph_ = nullptr;
  VotingChannel voting_channel_;

  // Maps each browser context ID to its associated URLMatcher.
  absl::flat_hash_map<std::string, std::unique_ptr<url_matcher::URLMatcher>>
      profiles_force_foreground_patterns_;

  // Keeps track of foregrounded contexts.
  base::flat_set<const execution_context::ExecutionContext*>
      foregrounded_contexts_;
};
}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FORCE_FOREGROUND_VOTER_FOR_ORIGINS_H_
