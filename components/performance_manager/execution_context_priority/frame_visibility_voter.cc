// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_visibility_voter.h"

#include <utility>
#include <vector>

#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/page_node.h"
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
const char FrameVisibilityVoter::kSpeculativeFrameReason[] =
    "Speculative frame replacing visible frame.";

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

  speculative_frames_.clear();
  voting_channel_.Reset();
}

void FrameVisibilityVoter::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  if (!frame_node->IsActive()) {
    speculative_frames_.insert(frame_node);
  }
  SetVoteForFrame(frame_node, pending_page_node);
}

void FrameVisibilityVoter::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  const bool was_active = frame_node->IsActive();
  if (was_active) {
    DCHECK(!speculative_frames_.contains(frame_node));
  } else {
    speculative_frames_.erase(frame_node);
  }

  if (!ShouldVoteForFrame(frame_node)) {
    return;
  }

  voting_channel_.SetVote(frame_node, std::nullopt);

  // If an active frame is removed, any speculative frame replacing it no longer
  // has an active frame to inherit from.
  if (was_active) {
    for (const FrameNode* speculative_frame :
         GetSpeculativeFramesForActiveFrame(frame_node)) {
      SetVoteForSpeculativeFrame(speculative_frame, /*active_frame=*/nullptr);
    }
  }
}

void FrameVisibilityVoter::OnIsActiveChanged(const FrameNode* frame_node) {
  if (frame_node->IsActive()) {
    speculative_frames_.erase(frame_node);
  }
  SetVoteForFrame(frame_node);
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
  SetVoteForFrame(frame_node, frame_node->GetPageNode());
}

void FrameVisibilityVoter::SetVoteForFrame(const FrameNode* frame_node,
                                           const PageNode* page_node) {
  if (!ShouldVoteForFrame(frame_node)) {
    return;
  }

  // Active frame: vote directly based on its visibility and importance.
  if (frame_node->IsActive()) {
    const Vote vote =
        GetVote(frame_node->GetVisibility(), frame_node->IsImportant());
    voting_channel_.SetVote(frame_node, vote);
    UpdateVotesForSpeculativeFrames(frame_node);
    return;
  }

  // If this inactive frame is not tracked as a speculative frame (e.g., cached
  // or old frame), it should receive minimum priority.
  if (!speculative_frames_.contains(frame_node)) {
    voting_channel_.SetVote(frame_node, Vote(base::Process::Priority::kMinValue,
                                             kFrameVisibilityReason));
    return;
  }

  // Speculative frame: check if it is replacing an active frame.
  const FrameNode* active_frame =
      page_node ? GraphOperations::GetActiveFrameForFrameTreeNodeId(
                      page_node, frame_node->GetFrameTreeNodeId())
                : nullptr;
  SetVoteForSpeculativeFrame(frame_node, active_frame);
}

void FrameVisibilityVoter::SetVoteForSpeculativeFrame(
    const FrameNode* speculative_frame,
    const FrameNode* active_frame) {
  if (!ShouldVoteForFrame(speculative_frame)) {
    return;
  }

  if (active_frame) {
    const Vote active_vote =
        GetVote(active_frame->GetVisibility(), active_frame->IsImportant());
    if (active_vote.value() > base::Process::Priority::kMinValue) {
      voting_channel_.SetVote(speculative_frame, Vote(active_vote.value(),
                                                      kSpeculativeFrameReason));
      return;
    }
  }

  // Inactive frame not replacing a visible frame (or prerendered/background).
  voting_channel_.SetVote(
      speculative_frame,
      Vote(base::Process::Priority::kMinValue, kFrameVisibilityReason));
}

void FrameVisibilityVoter::UpdateVotesForSpeculativeFrames(
    const FrameNode* active_frame) {
  for (const FrameNode* speculative_frame :
       GetSpeculativeFramesForActiveFrame(active_frame)) {
    SetVoteForSpeculativeFrame(speculative_frame, active_frame);
  }
}

std::vector<const FrameNode*>
FrameVisibilityVoter::GetSpeculativeFramesForActiveFrame(
    const FrameNode* active_frame) const {
  std::vector<const FrameNode*> matching_speculative_frames;
  for (const FrameNode* speculative_frame : speculative_frames_) {
    if (speculative_frame->GetFrameTreeNodeId() ==
        active_frame->GetFrameTreeNodeId()) {
      matching_speculative_frames.push_back(speculative_frame);
    }
  }
  return matching_speculative_frames;
}

}  // namespace execution_context_priority
}  // namespace performance_manager
