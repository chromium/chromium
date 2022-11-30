// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/frame_visibility_decorator.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"

namespace performance_manager {

namespace {

FrameNode::Visibility GetFrameNodeVisibility(FrameNodeImpl* frame_node,
                                             bool is_page_visible) {
  // All frames of a page are not visible if the page is not visible.
  if (!is_page_visible)
    return FrameNode::Visibility::kNotVisible;

  // A main frame is always visible if the page is visible.
  if (frame_node->IsMainFrame())
    return FrameNode::Visibility::kVisible;

  // No viewport intersection. Can't determine the visibility.
  if (!frame_node->viewport_intersection().has_value())
    return FrameNode::Visibility::kUnknown;

  // A non-empty viewport intersection denotes a visible frame.
  if (!frame_node->viewport_intersection()->IsEmpty())
    return FrameNode::Visibility::kVisible;

  // Empty viewport intersection. The frame is thus not visible.
  return FrameNode::Visibility::kNotVisible;
}

// Update a frame node's visibility following a change in the page visibility.
void UpdateFrameVisibility(FrameNodeImpl* frame_node, bool is_page_visible) {
  FrameNode::Visibility visibility =
      GetFrameNodeVisibility(frame_node, is_page_visible);

  frame_node->SetVisibility(visibility);

  for (FrameNodeImpl* child_frame_node : frame_node->child_frame_nodes())
    UpdateFrameVisibility(child_frame_node, is_page_visible);
}

}  // namespace

FrameVisibilityDecorator::FrameVisibilityDecorator() = default;

FrameVisibilityDecorator::~FrameVisibilityDecorator() = default;

void FrameVisibilityDecorator::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->HasOnlySystemNode());
  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);
}

void FrameVisibilityDecorator::OnTakenFromGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
}

void FrameVisibilityDecorator::OnIsVisibleChanged(const PageNode* page_node) {
  PageNodeImpl* page_node_impl = PageNodeImpl::FromNode(page_node);

  // A page can sometimes have no main frame.
  FrameNodeImpl* main_frame_node = page_node_impl->GetMainFrameNodeImpl();
  if (!main_frame_node)
    return;

  UpdateFrameVisibility(main_frame_node, page_node_impl->is_visible());
}

void FrameVisibilityDecorator::OnViewportIntersectionChanged(
    const FrameNode* frame_node) {
  DCHECK(!frame_node->IsMainFrame());
  DCHECK(frame_node->GetViewportIntersection().has_value());

  FrameNodeImpl* frame_node_impl = FrameNodeImpl::FromNode(frame_node);
  FrameNode::Visibility visibility = GetFrameNodeVisibility(
      frame_node_impl, frame_node_impl->page_node()->is_visible());

  // Compare to the old value.
  if (visibility == frame_node_impl->visibility())
    return;

  frame_node_impl->SetVisibility(visibility);
}

}  // namespace performance_manager
