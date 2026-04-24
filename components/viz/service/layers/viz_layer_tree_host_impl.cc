// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/layers/viz_layer_tree_host_impl.h"

#include "cc/trees/layer_tree_settings.h"

namespace viz {

std::unique_ptr<cc::LayerTreeHostImpl> VizLayerTreeHostImpl::Create(
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

}  // namespace viz
