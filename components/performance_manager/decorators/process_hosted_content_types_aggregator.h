// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PROCESS_HOSTED_CONTENT_TYPES_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PROCESS_HOSTED_CONTENT_TYPES_AGGREGATOR_H_

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/worker_node.h"

namespace performance_manager {

// Aggregates the type of content hosted inside a process and populates the
// |hosted_content_types()| property.
class ProcessHostedContentTypesAggregator
    : public GraphOwnedDefaultImpl,
      public PageNode::ObserverDefaultImpl,
      public FrameNode::ObserverDefaultImpl,
      public WorkerNode::ObserverDefaultImpl {
 public:
  ProcessHostedContentTypesAggregator();
  ~ProcessHostedContentTypesAggregator() override;

  ProcessHostedContentTypesAggregator(
      const ProcessHostedContentTypesAggregator&) = delete;
  ProcessHostedContentTypesAggregator& operator=(
      const ProcessHostedContentTypesAggregator&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver:
  void OnTypeChanged(const PageNode* page_node,
                     PageType previous_type) override;

  // FrameNodeObserver:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnIsAdFrameChanged(const FrameNode* frame_node) override;
  void OnURLChanged(const FrameNode* frame_node,
                    const GURL& previous_value) override;

  // WorkerNodeObserver:
  void OnWorkerNodeAdded(const WorkerNode* worker_node) override;

 private:
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PROCESS_HOSTED_CONTENT_TYPES_AGGREGATOR_H_
