// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/page_aggregator.h"

#include "components/performance_manager/decorators/page_aggregator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"

namespace performance_manager {
namespace testing {

void CreatePageAggregatorAndPassItToGraph() {
  Graph* graph = PerformanceManager::GetGraph();
  graph->PassToGraph(std::make_unique<PageAggregator>());
}

}  // namespace testing
}  // namespace performance_manager
