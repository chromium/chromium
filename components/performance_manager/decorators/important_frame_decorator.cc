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

bool IsImportant(const FrameNode* frame_node) {
  // Always important if the feature is disabled.
  if (!base::FeatureList::IsEnabled(features::kUnimportantFramesPriority)) {
    return true;
  }

  std::optional<ViewportIntersection> viewport_intersection =
      frame_node->GetViewportIntersection();

  // Too early in the frame's lifecycle, don't know yet if it intersects with
  // the viewport. Can't determine the importance. Default to important.
  if (!viewport_intersection.has_value()) {
    return true;
  }

  // A frame is important if it intersects with a large area of the viewport.
  if (viewport_intersection->is_intersecting_large_area()) {
    return true;
  }

  // The frame does not intersect with the viewport.
  if (!viewport_intersection->is_intersecting()) {
    return false;
  }

  // The frame intersects with a non-large area of the viewport. Only important
  // if the user interacted with it.
  return frame_node->HadUserActivation();
}

}  // namespace

ImportantFrameDecorator::ImportantFrameDecorator() = default;
ImportantFrameDecorator::~ImportantFrameDecorator() = default;

void ImportantFrameDecorator::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->HasOnlySystemNode());
  graph->AddInitializingFrameNodeObserver(this);
}

void ImportantFrameDecorator::OnTakenFromGraph(Graph* graph) {
  graph->RemoveInitializingFrameNodeObserver(this);
}

void ImportantFrameDecorator::OnFrameNodeInitializing(
    const FrameNode* frame_node) {
  CHECK(!frame_node->HadUserActivation());
  CHECK(!frame_node->GetViewportIntersection() ||
        frame_node->GetViewportIntersection()->is_intersecting_large_area());
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

}  // namespace performance_manager
