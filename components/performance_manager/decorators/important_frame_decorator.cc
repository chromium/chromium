// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/important_frame_decorator.h"

#include <optional>

#include "base/feature_list.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/features.h"

namespace performance_manager {

namespace {

bool IsCrossProcessFrame(const FrameNode* frame_node) {
  const ProcessNode* process_node = frame_node->GetProcessNode();
  // Walks up the frame tree to check if the frame has different process node
  // with any ancestor frame.
  while (const FrameNode* parent_frame_node =
             frame_node->GetParentOrOuterDocumentOrEmbedder()) {
    if (parent_frame_node->GetProcessNode() != process_node) {
      return true;
    }
    frame_node = parent_frame_node;
  }
  return false;
}

bool IsImportant(const FrameNode* frame_node) {
  // Always important if the feature is disabled.
  if (!base::FeatureList::IsEnabled(features::kUnimportantFramesPriority)) {
    return true;
  }

  switch (frame_node->GetViewportIntersection()) {
    case ViewportIntersection::kUnknown:
      // Too early in the frame's lifecycle, don't know yet if it intersects
      // with the viewport. Can't determine the importance. Default to
      // important.
      return true;

    case ViewportIntersection::kNotIntersecting:
      // A frame is not important if it doesn't intersect with the viewport.
      return false;

    case ViewportIntersection::kIntersecting:
      // An intersecting frame is always important if it has same process as its
      // ancestor frames.
      if (!IsCrossProcessFrame(frame_node)) {
        return true;
      }

      // The frame is important if it either intersects with a large area of the
      // viewport or if the user interacted with it.
      return frame_node->IsIntersectingLargeArea() ||
             frame_node->HadUserActivation();
  }
}

}  // namespace

ImportantFrameDecorator::ImportantFrameDecorator() = default;
ImportantFrameDecorator::~ImportantFrameDecorator() = default;

void ImportantFrameDecorator::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->HasOnlySystemNode());
  graph->AddFrameNodeObserver(this);
}

void ImportantFrameDecorator::OnTakenFromGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
}

void ImportantFrameDecorator::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  CHECK(!frame_node->HadUserActivation());
  CHECK(IsImportant(frame_node));
}

void ImportantFrameDecorator::OnHadUserActivationChanged(
    const FrameNode* frame_node) {
  FrameNodeImpl::FromNode(frame_node)->SetIsImportant(IsImportant(frame_node));
}

void ImportantFrameDecorator::OnViewportIntersectionChanged(
    const FrameNode* frame_node) {
  FrameNodeImpl::FromNode(frame_node)->SetIsImportant(IsImportant(frame_node));
}

void ImportantFrameDecorator::OnIsIntersectingLargeAreaChanged(
    const FrameNode* frame_node) {
  FrameNodeImpl::FromNode(frame_node)->SetIsImportant(IsImportant(frame_node));
}

}  // namespace performance_manager
