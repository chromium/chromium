// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_LAYERS_VIZ_LAYER_TREE_HOST_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_LAYERS_VIZ_LAYER_TREE_HOST_IMPL_H_

#include "cc/trees/layer_tree_host_impl.h"

namespace cc {
class TaskRunnerProvider;
class RenderingStatsInstrumentation;
class TaskGraphRunner;
class MutatorHost;
class RasterDarkModeFilter;
class LayerTreeSettings;
}  // namespace cc

namespace viz {

class VizLayerTreeHostImpl : public cc::LayerTreeHostImpl {
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
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_LAYERS_VIZ_LAYER_TREE_HOST_IMPL_H_
