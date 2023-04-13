// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/layers/layer_context_impl.h"

#include <utility>

#include "cc/trees/commit_state.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"

namespace viz {

LayerContextImpl::LayerContextImpl(mojom::PendingLayerContext& context)
    : receiver_(this, std::move(context.receiver)),
      client_(std::move(context.client)) {}

LayerContextImpl::~LayerContextImpl() = default;

void LayerContextImpl::SetTargetLocalSurfaceId(const LocalSurfaceId& id) {
  context_.SetTargetLocalSurfaceId(id);
}

void LayerContextImpl::SetVisible(bool visible) {
  context_.SetVisible(visible);
}

void LayerContextImpl::Commit(mojom::LayerTreeUpdatePtr update) {
  cc::CommitState state;
  state.device_viewport_rect = update->device_viewport;
  state.device_scale_factor = update->device_scale_factor;
  state.local_surface_id_from_parent = update->local_surface_id_from_parent;
  context_.Commit(state);
}

}  // namespace viz
