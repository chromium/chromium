// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_FRAME_VISIBILITY_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_FRAME_VISIBILITY_DECORATOR_H_

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager {

// A decorator that observes changes to the visibility of all pages, and the
// viewport intersection of all frames, and decorates each frame with their
// visibility.
//
// Currently, all frame nodes are added with an empty viewport intersection.
// Shorty after creation, a OnViewportIntersectionChanged() notification is
// expected.
class FrameVisibilityDecorator : public GraphOwnedDefaultImpl,
                                 public FrameNode::ObserverDefaultImpl,
                                 public PageNode::ObserverDefaultImpl {
 public:
  FrameVisibilityDecorator();
  ~FrameVisibilityDecorator() override;

  FrameVisibilityDecorator(const FrameVisibilityDecorator&) = delete;
  FrameVisibilityDecorator& operator=(const FrameVisibilityDecorator&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver:
  void OnIsVisibleChanged(const PageNode* page_node) override;

  // FrameNodeObserver:
  void OnViewportIntersectionChanged(const FrameNode* frame_node) override;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_FRAME_VISIBILITY_DECORATOR_H_
