// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/glic_actuation_priority_voter.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/graph.h"

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
const char GlicActuationPriorityVoter::kGlicActuationReason[] =
    "Glic task actuation.";

GlicActuationPriorityVoter::GlicActuationPriorityVoter() = default;
GlicActuationPriorityVoter::~GlicActuationPriorityVoter() = default;

void GlicActuationPriorityVoter::InitializeOnGraph(
    Graph* graph,
    VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);
  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);
}

void GlicActuationPriorityVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  voting_channel_.Reset();
}

void GlicActuationPriorityVoter::OnGlicActuationStateChanged(
    const PageNode* page_node,
    GlicActuationState previous_state) {
  const GlicActuationState state =
      PageLiveStateDecorator::Data::FromPageNode(page_node)
          ->GetGlicActuationState();

  auto* main_frame_node = page_node->GetPrimaryMainFrameNode();
  if (main_frame_node) {
    UpdateFrameNodeVote(main_frame_node, previous_state, state);
  }
}

void GlicActuationPriorityVoter::OnPageNodeAdded(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)->AddObserver(
      this);
}

void GlicActuationPriorityVoter::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->RemoveObserver(this);
}

void GlicActuationPriorityVoter::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  // Filter out subframes and fenced frame roots (which share the same PageNode)
  // while allowing GuestView main frames (whose embedder is on a different
  // PageNode). Ideally FrameNodeObserver would provide a
  // `pending_parent_or_outer_document` parameter.
  if (pending_parent_or_outer_document_or_embedder &&
      pending_parent_or_outer_document_or_embedder->GetPageNode() ==
          pending_page_node) {
    return;
  }
  const GlicActuationState state =
      PageLiveStateDecorator::Data::FromPageNode(pending_page_node)
          ->GetGlicActuationState();
  if (state != GlicActuationState::kNone && frame_node->IsCurrent()) {
    UpdateFrameNodeVote(frame_node, GlicActuationState::kNone, state);
  }
}

void GlicActuationPriorityVoter::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  if (frame_node->GetParentOrOuterDocument()) {
    return;
  }
  const GlicActuationState state =
      PageLiveStateDecorator::Data::FromPageNode(frame_node->GetPageNode())
          ->GetGlicActuationState();
  if (frame_node->IsCurrent() && state != GlicActuationState::kNone) {
    voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
  }
}

void GlicActuationPriorityVoter::OnCurrentFrameChanged(
    const FrameNode* previous_frame_node,
    const FrameNode* current_frame_node) {
  const FrameNode* frame_node =
      current_frame_node ? current_frame_node : previous_frame_node;
  CHECK(frame_node);
  if (frame_node->GetParentOrOuterDocument()) {
    return;
  }
  GlicActuationState state =
      PageLiveStateDecorator::Data::FromPageNode(frame_node->GetPageNode())
          ->GetGlicActuationState();
  if (state == GlicActuationState::kNone) {
    return;
  }

  if (current_frame_node) {
    UpdateFrameNodeVote(current_frame_node, GlicActuationState::kNone, state);
  }
  if (previous_frame_node) {
    voting_channel_.InvalidateVote(GetExecutionContext(previous_frame_node));
  }
}

void GlicActuationPriorityVoter::UpdateFrameNodeVote(
    const FrameNode* frame_node,
    GlicActuationState previous_state,
    GlicActuationState new_state) {
  DCHECK_NE(previous_state, new_state);
  if (frame_node->GetParentOrOuterDocument()) {
    return;
  }

  if (new_state == GlicActuationState::kNone) {
    voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
    return;
  }

  const base::Process::Priority priority =
      (new_state == GlicActuationState::kActuatingOnVisibleTab)
          ? base::Process::Priority::kUserBlocking
          : base::Process::Priority::kUserVisible;

  const Vote vote(priority, kGlicActuationReason);

  if (previous_state != GlicActuationState::kNone) {
    voting_channel_.ChangeVote(GetExecutionContext(frame_node), vote);
  } else {
    voting_channel_.SubmitVote(GetExecutionContext(frame_node), vote);
  }
}

}  // namespace performance_manager::execution_context_priority
