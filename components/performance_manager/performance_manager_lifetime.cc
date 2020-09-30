// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/performance_manager_lifetime.h"

#include "base/bind.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/performance_manager/decorators/page_load_tracker_decorator.h"
#include "components/performance_manager/execution_context/execution_context_registry_impl.h"
#include "components/performance_manager/graph/frame_node_impl_describer.h"
#include "components/performance_manager/graph/page_node_impl_describer.h"
#include "components/performance_manager/graph/process_node_impl_describer.h"
#include "components/performance_manager/graph/worker_node_impl_describer.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/decorators/tab_properties_decorator.h"
#include "components/performance_manager/public/graph/graph.h"

#if !defined(OS_ANDROID)
#include "components/performance_manager/public/decorators/site_data_recorder.h"
#endif

namespace performance_manager {

namespace {

void DefaultGraphCreatedCallback(
    GraphCreatedCallback external_graph_created_callback,
    GraphImpl* graph) {
  graph->PassToGraph(
      std::make_unique<execution_context::ExecutionContextRegistryImpl>());
  graph->PassToGraph(std::make_unique<FrameNodeImplDescriber>());
  graph->PassToGraph(std::make_unique<PageLiveStateDecorator>());
  graph->PassToGraph(std::make_unique<PageLoadTrackerDecorator>());
  graph->PassToGraph(std::make_unique<PageNodeImplDescriber>());
  graph->PassToGraph(std::make_unique<ProcessNodeImplDescriber>());
  graph->PassToGraph(std::make_unique<TabPropertiesDecorator>());
  graph->PassToGraph(std::make_unique<WorkerNodeImplDescriber>());
#if !defined(OS_ANDROID)
  graph->PassToGraph(std::make_unique<SiteDataRecorder>());
#endif
  std::move(external_graph_created_callback).Run(graph);
}

void NullGraphCreatedCallback(
    GraphCreatedCallback external_graph_created_callback,
    GraphImpl* graph) {
  std::move(external_graph_created_callback).Run(graph);
}

base::OnceCallback<void(GraphImpl*)> AddDecorators(
    Decorators decorators,
    GraphCreatedCallback graph_created_callback) {
  switch (decorators) {
    case Decorators::kNone:
      return base::BindOnce(&NullGraphCreatedCallback,
                            std::move(graph_created_callback));
    case Decorators::kDefault:
      return base::BindOnce(&DefaultGraphCreatedCallback,
                            std::move(graph_created_callback));
  }
  NOTREACHED();
  return {};
}

}  // namespace

PerformanceManagerLifetime::PerformanceManagerLifetime(
    Decorators decorators,
    GraphCreatedCallback graph_created_callback)
    : performance_manager_(PerformanceManagerImpl::Create(
          AddDecorators(decorators, std::move(graph_created_callback)))),
      performance_manager_registry_(
          performance_manager::PerformanceManagerRegistry::Create()) {}

PerformanceManagerLifetime::~PerformanceManagerLifetime() {
  performance_manager_registry_->TearDown();
  performance_manager_registry_.reset();
  performance_manager::DestroyPerformanceManager(
      std::move(performance_manager_));
}

std::unique_ptr<PerformanceManager>
CreatePerformanceManagerWithDefaultDecorators(
    GraphCreatedCallback graph_created_callback) {
  return PerformanceManagerImpl::Create(
      AddDecorators(Decorators::kDefault, std::move(graph_created_callback)));
}

void DestroyPerformanceManager(std::unique_ptr<PerformanceManager> instance) {
  PerformanceManagerImpl::Destroy(std::move(instance));
}

}  // namespace performance_manager
