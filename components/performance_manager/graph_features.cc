// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/graph_features.h"

#include <memory>

#include "build/build_config.h"
#include "components/performance_manager/decorators/frame_visibility_decorator.h"
#include "components/performance_manager/decorators/important_frame_decorator.h"
#include "components/performance_manager/decorators/page_aggregator.h"
#include "components/performance_manager/decorators/page_load_tracker_decorator.h"
#include "components/performance_manager/decorators/process_hosted_content_types_aggregator.h"
#include "components/performance_manager/decorators/process_priority_aggregator.h"
#include "components/performance_manager/execution_context_priority/execution_context_priority_decorator.h"
#include "components/performance_manager/freezing/frozen_frame_aggregator.h"
#include "components/performance_manager/graph/frame_node_impl_describer.h"
#include "components/performance_manager/graph/page_node_impl_describer.h"
#include "components/performance_manager/graph/process_node_impl_describer.h"
#include "components/performance_manager/graph/worker_node_impl_describer.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/metrics/metrics_collector.h"
#include "components/performance_manager/resource_attribution/query_scheduler.h"
#include "components/performance_manager/scenarios/loading_scenario_observer.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/performance_manager/public/decorators/site_data_recorder.h"
#endif

namespace performance_manager {

namespace {

template <typename ObjectType>
void Install(Graph* graph) {
  graph->PassToGraph(std::make_unique<ObjectType>());
}

}  // namespace

void GraphFeatures::ConfigureGraph(Graph* graph) const {
  if (flags_.frame_visibility_decorator) {
    Install<FrameVisibilityDecorator>(graph);
  }
  if (flags_.important_frame_decorator) {
    Install<ImportantFrameDecorator>(graph);
  }
  if (flags_.metrics_collector) {
    Install<MetricsCollector>(graph);
  }
  if (flags_.node_impl_describers) {
    Install<FrameNodeImplDescriber>(graph);
    Install<PageNodeImplDescriber>(graph);
    Install<ProcessNodeImplDescriber>(graph);
    Install<WorkerNodeImplDescriber>(graph);
  }
  if (flags_.page_load_tracker_decorator) {
    Install<PageLoadTrackerDecorator>(graph);
  }
  if (flags_.priority_tracking) {
    // The ExecutionContextPriorityDecorator depends on FrameVisibilityDecorator
    // and ImportantFrameDecorator and so must be installed after.
    Install<execution_context_priority::ExecutionContextPriorityDecorator>(
        graph);
    Install<ProcessPriorityAggregator>(graph);
  }
  if (flags_.process_hosted_content_types_aggregator) {
    Install<ProcessHostedContentTypesAggregator>(graph);
  }
  if (flags_.page_aggregator) {
    Install<PageAggregator>(graph);
  }
  if (flags_.frozen_frame_aggregator) {
    Install<FrozenFrameAggregator>(graph);
  }
  if (flags_.loading_scenario) {
    Install<LoadingScenarioObserver>(graph);
  }
  if (flags_.resource_attribution_scheduler) {
    Install<resource_attribution::internal::QueryScheduler>(graph);
  }

#if !BUILDFLAG(IS_ANDROID)
  if (flags_.site_data_recorder) {
    Install<SiteDataRecorder>(graph);
  }
#endif

  if (flags_.tab_page_decorator) {
    Install<TabPageDecorator>(graph);
  }

  if (flags_.v8_context_tracker) {
    Install<v8_memory::V8ContextTracker>(graph);
  }
}

}  // namespace performance_manager
