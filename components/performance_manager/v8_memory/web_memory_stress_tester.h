// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_STRESS_TESTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_STRESS_TESTER_H_

#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager {

namespace v8_memory {

// Observer that calls WebMeasureMemory randomly after a page load, and
// discards the results, to check for crashes. This is controlled by an
// experiment that will only be enabled for some Canary users.
class WebMeasureMemoryStressTester : public PageNode::ObserverDefaultImpl,
                                     public GraphOwned {
 public:
  static bool FeatureIsEnabled();

  WebMeasureMemoryStressTester() = default;
  ~WebMeasureMemoryStressTester() override = default;

  WebMeasureMemoryStressTester(const WebMeasureMemoryStressTester& other) =
      delete;
  WebMeasureMemoryStressTester& operator=(
      const WebMeasureMemoryStressTester& other) = delete;

  // PageNode::ObserverDefaultImpl
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override;

  // GraphOwned
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;
};

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_STRESS_TESTER_H_
