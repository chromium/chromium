// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_visibility_voter.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "url/gurl.h"

namespace performance_manager {
namespace execution_context_priority {

namespace {

// Returns a vote with the appropriate priority depending on the frame's
// |visibility|.
Vote GetVote(FrameNode::Visibility visibility, bool is_important) {
  base::Process::Priority priority;
  switch (visibility) {
    case FrameNode::Visibility::kUnknown:
      priority = base::Process::Priority::kUserBlocking;
      break;
    case FrameNode::Visibility::kVisible: {
      priority = is_important ? base::Process::Priority::kUserBlocking
                              : base::Process::Priority::kUserVisible;
      break;
    }
    case FrameNode::Visibility::kNotVisible:
      priority = base::Process::Priority::kMinValue;
      break;
  }
  return Vote(priority, FrameVisibilityVoter::kFrameVisibilityReason);
}

}  // namespace

// static
const char FrameVisibilityVoter::kFrameVisibilityReason[] = "Frame visibility.";

FrameVisibilityVoter::FrameVisibilityVoter(bool ignore_main_frame_visibility)
    : ignore_main_frame_visibility_(ignore_main_frame_visibility) {}

FrameVisibilityVoter::~FrameVisibilityVoter() = default;

bool FrameVisibilityVoter::ShouldVoteForFrame(
    const FrameNode* frame_node) const {
  return !(frame_node->IsMainFrame() && ignore_main_frame_visibility_);
}

void FrameVisibilityVoter::InitializeOnGraph(Graph* graph,
                                             VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);

  graph->AddFrameNodeObserver(this);
}

void FrameVisibilityVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);

  voting_channel_.Reset();
}

void FrameVisibilityVoter::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  SetVoteForFrame(frame_node);
}

void FrameVisibilityVoter::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  if (!ShouldVoteForFrame(frame_node)) {
    return;
  }

  voting_channel_.SetVote(frame_node, std::nullopt);
}

void FrameVisibilityVoter::OnFrameVisibilityChanged(
    const FrameNode* frame_node,
    FrameNode::Visibility previous_value) {
  SetVoteForFrame(frame_node);
}

void FrameVisibilityVoter::OnIsImportantChanged(const FrameNode* frame_node) {
  SetVoteForFrame(frame_node);
}

void FrameVisibilityVoter::SetVoteForFrame(const FrameNode* frame_node) {
  if (!ShouldVoteForFrame(frame_node)) {
    return;
  }

  const Vote vote =
      GetVote(frame_node->GetVisibility(), frame_node->IsImportant());
  voting_channel_.SetVote(frame_node, vote);
}

}  // namespace execution_context_priority
}  // namespace performance_manager
