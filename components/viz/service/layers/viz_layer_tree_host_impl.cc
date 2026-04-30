// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/layers/viz_layer_tree_host_impl.h"

#include "base/trace_event/trace_event.h"
#include "cc/trees/layer_tree_settings.h"

namespace viz {

std::unique_ptr<VizLayerTreeHostImpl> VizLayerTreeHostImpl::Create(
    const cc::LayerTreeSettings& settings,
    cc::LayerTreeHostImplClient* client,
    cc::TaskRunnerProvider* task_runner_provider,
    cc::RenderingStatsInstrumentation* rendering_stats_instrumentation,
    cc::TaskGraphRunner* task_graph_runner,
    std::unique_ptr<cc::MutatorHost> mutator_host,
    cc::RasterDarkModeFilter* dark_mode_filter,
    int id,
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
    cc::LayerTreeHostSchedulingClient* scheduling_client) {
  CHECK(settings.trees_in_viz_in_viz_process);
  return base::WrapUnique(new VizLayerTreeHostImpl(
      settings, client, task_runner_provider, rendering_stats_instrumentation,
      task_graph_runner, std::move(mutator_host), dark_mode_filter, id,
      std::move(image_worker_task_runner), scheduling_client));
}

VizLayerTreeHostImpl::~VizLayerTreeHostImpl() = default;

void VizLayerTreeHostImpl::set_next_frame_token_from_client(
    uint32_t frame_token) {
  next_frame_token_from_client_ = frame_token;
}

void VizLayerTreeHostImpl::CreateUIResourceFromImportedResource(
    cc::UIResourceId uid,
    ResourceId resource_id,
    const gfx::Size& size,
    bool is_opaque) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::CreateUIResourceFromResource");
  DCHECK_GT(uid, 0);

  // Allow for multiple creation requests with the same UIResourceId.  The
  // previous resource is simply deleted.
  ResourceId id = ResourceIdForUIResource(uid);
  if (id) {
    DeleteUIResource(uid);
  }

  if (!has_valid_layer_tree_frame_sink_) {
    EvictAllUIResources();
    return;
  }

  UIResourceData data;
  data.resource_id_for_export = resource_id;
  data.opaque = is_opaque;
  data.size = size;
  ui_resource_map_[uid] = std::move(data);
}

}  // namespace viz
