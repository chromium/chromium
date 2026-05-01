// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_LAYERS_VIZ_LAYER_TREE_HOST_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_LAYERS_VIZ_LAYER_TREE_HOST_IMPL_H_

#include "cc/trees/layer_tree_host_impl.h"
#include "components/viz/service/viz_service_export.h"

namespace cc {
class TaskRunnerProvider;
class RenderingStatsInstrumentation;
class TaskGraphRunner;
class MutatorHost;
class RasterDarkModeFilter;
class LayerTreeSettings;
}  // namespace cc

namespace viz {

class VIZ_SERVICE_EXPORT VizLayerTreeHostImpl : public cc::LayerTreeHostImpl {
 public:
  static std::unique_ptr<VizLayerTreeHostImpl> Create(
      const cc::LayerTreeSettings& settings,
      cc::LayerTreeHostImplClient* client,
      cc::TaskRunnerProvider* task_runner_provider,
      cc::RenderingStatsInstrumentation* rendering_stats_instrumentation,
      cc::TaskGraphRunner* task_graph_runner,
      std::unique_ptr<cc::MutatorHost> mutator_host,
      cc::RasterDarkModeFilter* dark_mode_filter,
      int id,
      scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
      cc::LayerTreeHostSchedulingClient* scheduling_client);
  using LayerTreeHostImpl::LayerTreeHostImpl;
  ~VizLayerTreeHostImpl() override;

  void set_current_local_surface_id_from_client(
      const LocalSurfaceId& local_surface_id_from_client) {
    current_local_surface_id_from_client_ = local_surface_id_from_client;
  }

  void set_next_frame_token_from_client(uint32_t frame_token);

  void CreateUIResourceFromImportedResource(cc::UIResourceId uid,
                                            ResourceId resource_id,
                                            const gfx::Size& size,
                                            bool is_opaque);

  void set_send_frame_token_to_embedder(bool send_frame_token_to_embedder) {
    send_frame_token_to_embedder_ = send_frame_token_to_embedder;
  }

  void set_is_handling_interaction_from_client(bool is_handling_interaction) {
    is_handling_interaction_from_client_ = is_handling_interaction;
  }
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_LAYERS_VIZ_LAYER_TREE_HOST_IMPL_H_
