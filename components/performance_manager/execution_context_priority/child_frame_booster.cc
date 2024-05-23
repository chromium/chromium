// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/child_frame_booster.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/graph.h"
#include "url/gurl.h"

namespace performance_manager::execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContextRegistry::GetFromGraph(
             frame_node->GetGraph())
      ->GetExecutionContextForFrameNode(frame_node);
}

}  // namespace

// static
const char ChildFrameBooster::kChildFrameBoostReason[] = "Child frame boost";

ChildFrameBooster::ChildFrameBooster(
    BoostingVoteAggregator* boosting_vote_aggregator)
    : boosting_vote_aggregator_(boosting_vote_aggregator) {}

ChildFrameBooster::~ChildFrameBooster() = default;

void ChildFrameBooster::InitializeOnGraph(Graph* graph) {
  graph->AddInitializingFrameNodeObserver(this);
}

void ChildFrameBooster::TearDownOnGraph(Graph* graph) {
  graph->RemoveInitializingFrameNodeObserver(this);
}

void ChildFrameBooster::OnFrameNodeInitializing(const FrameNode* frame_node) {
  if (frame_node->IsMainFrame()) {
    return;
  }

  // Non-ad frames are boosted.
  if (!frame_node->IsAdFrame()) {
    CreateBoostingVote(frame_node);
  }
}

void ChildFrameBooster::OnFrameNodeTearingDown(const FrameNode* frame_node) {
  if (frame_node->IsMainFrame()) {
    return;
  }

  // Delete the boosting vote for non-ad frames.
  if (!frame_node->IsAdFrame()) {
    DeleteBoostingVote(frame_node);
  }
}

void ChildFrameBooster::OnIsAdFrameChanged(const FrameNode* frame_node) {
  if (frame_node->IsMainFrame()) {
    return;
  }

  if (!frame_node->IsAdFrame()) {
    CreateBoostingVote(frame_node);
  } else {
    DeleteBoostingVote(frame_node);
  }
}

void ChildFrameBooster::CreateBoostingVote(const FrameNode* frame_node) {
  CHECK(frame_node);
  const FrameNode* parent_frame_node = frame_node->GetParentFrameNode();
  CHECK(parent_frame_node);
  boosting_votes_.emplace(
      std::piecewise_construct, std::forward_as_tuple(frame_node),
      std::forward_as_tuple(
          /*aggregator=*/boosting_vote_aggregator_,
          /*input_execution_context=*/GetExecutionContext(parent_frame_node),
          /*output_execution_context=*/GetExecutionContext(frame_node),
          /*reason=*/kChildFrameBoostReason));
}

void ChildFrameBooster::DeleteBoostingVote(const FrameNode* frame_node) {
  CHECK(frame_node);
  size_t removed = boosting_votes_.erase(frame_node);
  CHECK(removed);
}

}  // namespace performance_manager::execution_context_priority
