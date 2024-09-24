// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/frame_visibility_decorator.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/features.h"

namespace performance_manager {

namespace {

// Returns true if `page_node`'s content is being mirrored.
bool IsBeingMirrored(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsBeingMirrored();
}

FrameNode::Visibility GetFrameNodeVisibility(FrameNodeImpl* frame_node,
                                             bool is_page_user_visible) {
  // All frames of a page are not visible if the page is not visible.
  if (!is_page_user_visible) {
    return FrameNode::Visibility::kNotVisible;
  }

  // Only frame nodes that are current can be visible.
  if (!frame_node->IsCurrent()) {
    return FrameNode::Visibility::kNotVisible;
  }

  // Too early in the frame's lifecycle, don't know yet if it intersects with
  // the viewport. Can't determine the visibility.
  if (!frame_node->GetViewportIntersection().has_value()) {
    return FrameNode::Visibility::kUnknown;
  }

  // The frame intersects with the viewport and is thus visible.
  if (frame_node->GetViewportIntersection()->is_intersecting()) {
    return FrameNode::Visibility::kVisible;
  }

  // Does not intersects with the viewport. The frame is not visible.
  return FrameNode::Visibility::kNotVisible;
}

// Update a frame node's visibility and its children following a change in the
// page visibility.
void UpdateFrameTreeVisibility(FrameNodeImpl* frame_node,
                               bool is_page_user_visible) {
  FrameNode::Visibility visibility =
      GetFrameNodeVisibility(frame_node, is_page_user_visible);

  frame_node->SetVisibility(visibility);

  for (FrameNodeImpl* child_frame_node : frame_node->child_frame_nodes())
    UpdateFrameTreeVisibility(child_frame_node, is_page_user_visible);
}

}  // namespace

FrameVisibilityDecorator::FrameVisibilityDecorator() = default;

FrameVisibilityDecorator::~FrameVisibilityDecorator() = default;

void FrameVisibilityDecorator::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->HasOnlySystemNode());
  graph->AddPageNodeObserver(this);
  graph->AddInitializingFrameNodeObserver(this);
}

void FrameVisibilityDecorator::OnTakenFromGraph(Graph* graph) {
  graph->RemoveInitializingFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
}

void FrameVisibilityDecorator::OnPageNodeAdded(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)->AddObserver(
      this);
}

void FrameVisibilityDecorator::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->RemoveObserver(this);
}

void FrameVisibilityDecorator::OnIsVisibleChanged(const PageNode* page_node) {
  // This notification can't change the user visibility of the page if it is
  // already being mirrored.
  if (IsBeingMirrored(page_node)) {
    return;
  }

  OnPageUserVisibilityChanged(page_node, page_node->IsVisible());
}

void FrameVisibilityDecorator::OnIsBeingMirroredChanged(
    const PageNode* page_node) {
  // If `IsVisible` is already true, this notification can't change the user
  // visibility of the page.
  if (page_node->IsVisible()) {
    return;
  }

  OnPageUserVisibilityChanged(page_node, IsBeingMirrored(page_node));
}

void FrameVisibilityDecorator::OnFrameNodeInitializing(
    const FrameNode* frame_node) {
  FrameNodeImpl* frame_node_impl = FrameNodeImpl::FromNode(frame_node);
  frame_node_impl->SetInitialVisibility(GetFrameNodeVisibility(
      frame_node_impl, IsPageUserVisible(frame_node_impl->page_node())));
}

void FrameVisibilityDecorator::OnCurrentFrameChanged(
    const FrameNode* previous_frame_node,
    const FrameNode* current_frame_node) {
  if (base::FeatureList::IsEnabled(features::kSeamlessRenderFrameSwap)) {
    if (current_frame_node) {
      OnFramePropertyChanged(current_frame_node);
    }
    if (previous_frame_node) {
      OnFramePropertyChanged(previous_frame_node);
    }
  } else {
    if (previous_frame_node) {
      OnFramePropertyChanged(previous_frame_node);
    }
    if (current_frame_node) {
      OnFramePropertyChanged(current_frame_node);
    }
  }
}

void FrameVisibilityDecorator::OnViewportIntersectionChanged(
    const FrameNode* frame_node) {
  CHECK(frame_node->GetParentOrOuterDocumentOrEmbedder());
  CHECK(frame_node->GetViewportIntersection().has_value());
  OnFramePropertyChanged(frame_node);
}

void FrameVisibilityDecorator::OnPageUserVisibilityChanged(
    const PageNode* page_node,
    bool page_is_user_visible) {
  PageNodeImpl* page_node_impl = PageNodeImpl::FromNode(page_node);

  // A page can sometimes have no main frame.
  FrameNodeImpl* main_frame_node = page_node_impl->main_frame_node();
  if (!main_frame_node) {
    return;
  }

  UpdateFrameTreeVisibility(main_frame_node, page_is_user_visible);
}

void FrameVisibilityDecorator::OnFramePropertyChanged(
    const FrameNode* frame_node) {
  FrameNodeImpl* frame_node_impl = FrameNodeImpl::FromNode(frame_node);
  FrameNode::Visibility new_visibility = GetFrameNodeVisibility(
      frame_node_impl, IsPageUserVisible(frame_node_impl->page_node()));

  if (new_visibility == frame_node_impl->GetVisibility()) {
    // No visibility change.
    return;
  }

  frame_node_impl->SetVisibility(new_visibility);
}

// static
bool FrameVisibilityDecorator::IsPageUserVisible(const PageNode* page_node) {
  return page_node->IsVisible() || IsBeingMirrored(page_node);
}

}  // namespace performance_manager
