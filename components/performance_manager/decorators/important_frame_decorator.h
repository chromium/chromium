// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_IMPORTANT_FRAME_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_IMPORTANT_FRAME_DECORATOR_H_

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager {

// Responsible for maintaining the IsImportant() property of frame nodes.
class ImportantFrameDecorator : public GraphOwnedDefaultImpl,
                                public FrameNodeObserver {
 public:
  ImportantFrameDecorator();
  ~ImportantFrameDecorator() override;

  ImportantFrameDecorator(const ImportantFrameDecorator&) = delete;
  ImportantFrameDecorator& operator=(const ImportantFrameDecorator&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // FrameNodeObserver:
  void OnBeforeFrameNodeAdded(
      const FrameNode* frame_node,
      const FrameNode* pending_parent_frame_node,
      const PageNode* pending_page_node,
      const ProcessNode* pending_process_node,
      const FrameNode* pending_parent_or_outer_document_or_embedder) override;
  void OnHadUserActivationChanged(const FrameNode* frame_node) override;
  void OnViewportIntersectionChanged(const FrameNode* frame_node) override;
  void OnIsIntersectingLargeAreaChanged(const FrameNode* frame_node) override;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_IMPORTANT_FRAME_DECORATOR_H_
