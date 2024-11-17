// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_IMPORTANT_FRAME_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_IMPORTANT_FRAME_DECORATOR_H_

#include "components/performance_manager/graph/initializing_frame_node_observer.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager {

// Responsible for maintaining the IsImportant() property of frame nodes.
class ImportantFrameDecorator : public GraphOwnedDefaultImpl,
                                public InitializingFrameNodeObserver {
 public:
  ImportantFrameDecorator();
  ~ImportantFrameDecorator() override;

  ImportantFrameDecorator(const ImportantFrameDecorator&) = delete;
  ImportantFrameDecorator& operator=(const ImportantFrameDecorator&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // InitializingFrameNodeObserver:
  void OnFrameNodeInitializing(const FrameNode* frame_node) override;
  void OnHadUserActivationChanged(const FrameNode* frame_node) override;
  void OnViewportIntersectionChanged(const FrameNode* frame_node) override;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_IMPORTANT_FRAME_DECORATOR_H_
