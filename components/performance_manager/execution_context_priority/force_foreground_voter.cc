// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/force_foreground_voter.h"

#include <utility>

#include "base/not_fatal_until.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

// static
const char ForceForegroundVoter::kForceForegroundReason[] =
    "Force foreground for all.";

ForceForegroundVoter::ForceForegroundVoter() = default;
ForceForegroundVoter::~ForceForegroundVoter() = default;

void ForceForegroundVoter::InitializeOnGraph(Graph* graph,
                                             VotingChannel voting_channel) {
  CHECK(graph->HasOnlySystemNode());
  voting_channel_ = std::move(voting_channel);
  graph->AddFrameNodeObserver(this);
  graph->AddWorkerNodeObserver(this);
}

void ForceForegroundVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemoveWorkerNodeObserver(this);
  voting_channel_.Reset();
}

void ForceForegroundVoter::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  voting_channel_.SetVote(
      frame_node,
      Vote(base::Process::Priority::kUserBlocking, kForceForegroundReason));
}

void ForceForegroundVoter::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  voting_channel_.SetVote(frame_node, std::nullopt);
}

void ForceForegroundVoter::OnBeforeWorkerNodeAdded(
    const WorkerNode* worker_node,
    const ProcessNode* pending_process_node) {
  voting_channel_.SetVote(
      worker_node,
      Vote(base::Process::Priority::kUserBlocking, kForceForegroundReason));
}

void ForceForegroundVoter::OnBeforeWorkerNodeRemoved(
    const WorkerNode* worker_node) {
  voting_channel_.SetVote(worker_node, std::nullopt);
}

}  // namespace performance_manager::execution_context_priority
