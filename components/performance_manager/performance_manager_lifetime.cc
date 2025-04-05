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
#include "components/performance_manager/execution_context_priority/frame_audible_voter.h"
#include "components/performance_manager/execution_context_priority/frame_capturing_media_stream_voter.h"
#include "components/performance_manager/execution_context_priority/frame_visibility_voter.h"
#include "components/performance_manager/execution_context_priority/inherit_client_priority_voter.h"
#include "components/performance_manager/execution_context_priority/loading_page_voter.h"
#include "components/performance_manager/graph/frame_node_impl_describer.h"
#include "components/performance_manager/graph/page_node_impl_describer.h"
#include "components/performance_manager/graph/process_node_impl_describer.h"
#include "components/performance_manager/graph/worker_node_impl_describer.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"
#if BUILDFLAG(IS_MAC)
#include "components/performance_manager/execution_context_priority/inherit_parent_priority_voter.h"
#endif  // BUILDFLAG(IS_MAC)

namespace performance_manager {

namespace {

GraphCreatedCallback* GetAdditionalGraphCreatedCallback() {
  static base::NoDestructor<GraphCreatedCallback>
      additional_graph_created_callback;
  return additional_graph_created_callback.get();
}

// Adds the default set of execution context voters.
void AddVoters(GraphImpl* graph) {
  if (auto* priority_voting_system =
          graph->GetRegisteredObjectAs<
              execution_context_priority::PriorityVotingSystem>()) {
    // When a frame is visible, casts either a USER_BLOCKING or USER_VISIBLE
    // vote, depending on if the frame is important.
    priority_voting_system
        ->AddPriorityVoter<execution_context_priority::FrameVisibilityVoter>();

    // Casts a USER_BLOCKING vote when a frame is audible.
    priority_voting_system
        ->AddPriorityVoter<execution_context_priority::FrameAudibleVoter>();

    // Casts a USER_BLOCKING vote when a frame is capturing a media stream.
    priority_voting_system->AddPriorityVoter<
        execution_context_priority::FrameCapturingMediaStreamVoter>();

    // Casts a vote for each child worker with the client's priority.
    priority_voting_system->AddPriorityVoter<
        execution_context_priority::InheritClientPriorityVoter>();

    // Casts a USER_VISIBLE vote for all frames in a loading page.
    if (base::FeatureList::IsEnabled(features::kPMLoadingPageVoter)) {
      priority_voting_system
          ->AddPriorityVoter<execution_context_priority::LoadingPageVoter>();
    }

#if BUILDFLAG(IS_MAC)
    // Casts a vote for each child frame with the parent's priority.
    if (features::kInheritParentPriority.Get()) {
      priority_voting_system->AddPriorityVoter<
          execution_context_priority::InheritParentPriorityVoter>();
    }
#endif
  }
}

std::optional<GraphFeatures>* GetGraphFeaturesOverride() {
  static std::optional<GraphFeatures> graph_features_override;
  return &graph_features_override;
}

void OnGraphCreated(const GraphFeatures& graph_features,
                    GraphCreatedCallback external_graph_created_callback) {
  GraphImpl* graph = PerformanceManagerImpl::GetGraphImpl();

  auto graph_features_override = *GetGraphFeaturesOverride();
  const GraphFeatures& configured_features =
      graph_features_override ? *graph_features_override : graph_features;

  // Install required features on the graph.
  configured_features.ConfigureGraph(graph);

  AddVoters(graph);

  // Run graph created callbacks.
  std::move(external_graph_created_callback).Run(graph);
  if (*GetAdditionalGraphCreatedCallback())
    std::move(*GetAdditionalGraphCreatedCallback()).Run(graph);
}

}  // namespace

PerformanceManagerLifetime::PerformanceManagerLifetime(
    const GraphFeatures& graph_features,
    GraphCreatedCallback graph_created_callback) {
  performance_manager_ = PerformanceManagerImpl::Create();
  OnGraphCreated(graph_features, std::move(graph_created_callback));
  performance_manager_registry_ =
      performance_manager::PerformanceManagerRegistry::Create();
}

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
