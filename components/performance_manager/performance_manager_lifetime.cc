// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/performance_manager_lifetime.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "components/performance_manager/decorators/frame_visibility_decorator.h"
#include "components/performance_manager/decorators/page_load_tracker_decorator.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/execution_context/execution_context_registry_impl.h"
#include "components/performance_manager/graph/frame_node_impl_describer.h"
#include "components/performance_manager/graph/page_node_impl_describer.h"
#include "components/performance_manager/graph/process_node_impl_describer.h"
#include "components/performance_manager/graph/worker_node_impl_describer.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"

namespace performance_manager {

namespace {

GraphCreatedCallback* GetAdditionalGraphCreatedCallback() {
  static base::NoDestructor<GraphCreatedCallback>
      additional_graph_created_callback;
  return additional_graph_created_callback.get();
}

std::optional<GraphFeatures>* GetGraphFeaturesOverride() {
  static std::optional<GraphFeatures> graph_features_override;
  return &graph_features_override;
}

void OnGraphCreated(const GraphFeatures& graph_features,
                    GraphCreatedCallback external_graph_created_callback,
                    GraphImpl* graph) {
  auto graph_features_override = *GetGraphFeaturesOverride();
  const GraphFeatures& configured_features =
      graph_features_override ? *graph_features_override : graph_features;

  // Install required features on the graph.
  configured_features.ConfigureGraph(graph);

  // Run graph created callbacks.
  std::move(external_graph_created_callback).Run(graph);
  if (*GetAdditionalGraphCreatedCallback())
    std::move(*GetAdditionalGraphCreatedCallback()).Run(graph);
}

}  // namespace

PerformanceManagerLifetime::PerformanceManagerLifetime(
    const GraphFeatures& graph_features,
    GraphCreatedCallback graph_created_callback)
    : performance_manager_(PerformanceManagerImpl::Create(
          base::BindOnce(&OnGraphCreated,
                         graph_features,
                         std::move(graph_created_callback)))),
      performance_manager_registry_(
          performance_manager::PerformanceManagerRegistry::Create()) {}

PerformanceManagerLifetime::~PerformanceManagerLifetime() {
  // There may still be worker hosts, WebContents and RenderProcessHosts with
  // attached user data, retaining WorkerNodes, PageNodes, FrameNodes and
  // ProcessNodes. Tear down the registry to release these nodes.
  performance_manager_registry_->TearDown();
  performance_manager_registry_.reset();
  performance_manager::DestroyPerformanceManager(
      std::move(performance_manager_));
}

// static
void PerformanceManagerLifetime::SetAdditionalGraphCreatedCallbackForTesting(
    GraphCreatedCallback graph_created_callback) {
  *GetAdditionalGraphCreatedCallback() = std::move(graph_created_callback);
}

// static
void PerformanceManagerLifetime::SetGraphFeaturesOverrideForTesting(
    const GraphFeatures& graph_features_override) {
  *GetGraphFeaturesOverride() = graph_features_override;
}

void DestroyPerformanceManager(std::unique_ptr<PerformanceManager> instance) {
  PerformanceManagerImpl::Destroy(std::move(instance));
}

}  // namespace performance_manager
