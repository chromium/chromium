// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/graph_features.h"

#include <memory>

#include "build/build_config.h"
#include "components/performance_manager/decorators/frame_visibility_decorator.h"
#include "components/performance_manager/decorators/freezing_vote_decorator.h"
#include "components/performance_manager/decorators/page_load_tracker_decorator.h"
#include "components/performance_manager/decorators/process_hosted_content_types_aggregator.h"
#include "components/performance_manager/execution_context/execution_context_registry_impl.h"
#include "components/performance_manager/execution_context_priority/execution_context_priority_decorator.h"
#include "components/performance_manager/graph/frame_node_impl_describer.h"
#include "components/performance_manager/graph/page_node_impl_describer.h"
#include "components/performance_manager/graph/process_node_impl_describer.h"
#include "components/performance_manager/graph/worker_node_impl_describer.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/metrics/metrics_collector.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"
#include "components/performance_manager/v8_memory/web_memory_stress_tester.h"

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
  if (flags_.execution_context_registry)
    Install<execution_context::ExecutionContextRegistryImpl>(graph);
  if (flags_.frame_node_impl_describer)
    Install<FrameNodeImplDescriber>(graph);
  if (flags_.frame_visibility_decorator)
    Install<FrameVisibilityDecorator>(graph);
  if (flags_.metrics_collector)
    Install<MetricsCollector>(graph);
  if (flags_.freezing_vote_decorator)
    Install<FreezingVoteDecorator>(graph);
  if (flags_.page_load_tracker_decorator)
    Install<PageLoadTrackerDecorator>(graph);
  if (flags_.page_node_impl_describer)
    Install<PageNodeImplDescriber>(graph);
  if (flags_.process_hosted_content_types_aggregator)
    Install<ProcessHostedContentTypesAggregator>(graph);
  if (flags_.process_node_impl_describer)
    Install<ProcessNodeImplDescriber>(graph);
  if (flags_.worker_node_impl_describer)
    Install<WorkerNodeImplDescriber>(graph);

#if !BUILDFLAG(IS_ANDROID)
  if (flags_.site_data_recorder)
    Install<SiteDataRecorder>(graph);
#endif

  // These classes have a dependency on ExecutionContextRegistry, so must be
  // installed after it.
  if (flags_.execution_context_priority_decorator) {
    Install<execution_context_priority::ExecutionContextPriorityDecorator>(
        graph);
  }
  if (flags_.v8_context_tracker) {
    Install<v8_memory::V8ContextTracker>(graph);
    if (v8_memory::WebMeasureMemoryStressTester::FeatureIsEnabled())
      Install<v8_memory::WebMeasureMemoryStressTester>(graph);
  }
}

}  // namespace performance_manager
