// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/input_manager.h"

#include <utility>

namespace viz {

InputManager::~InputManager() {
  frame_sink_manager_->RemoveObserver(this);
}

InputManager::InputManager(FrameSinkManagerImpl* frame_sink_manager)
    : frame_sink_manager_(frame_sink_manager) {
  TRACE_EVENT("viz", "InputManager::InputManager");
  DCHECK(frame_sink_manager_);
  frame_sink_manager_->AddObserver(this);
}

void InputManager::OnCreateCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    bool is_root,
    input::mojom::RenderInputRouterConfigPtr render_input_router_config,
    bool create_input_receiver,
    gpu::SurfaceHandle surface_handle) {
  TRACE_EVENT("viz", "InputManager::OnCreateCompositorFrameSink",
              "config_is_null", !render_input_router_config, "frame_sink_id",
              frame_sink_id);
  if (create_input_receiver) {
    CHECK(is_root);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&InputManager::CreateAndroidInputReceiver,
                                  weak_ptr_factory_.GetWeakPtr(), frame_sink_id,
                                  surface_handle));
    return;
  }

  // `render_input_router_config` is non null only when layer tree frame sinks
  // for renderer are being requested.
  if (!render_input_router_config) {
    return;
  }

  DCHECK(render_input_router_config->rir_client.is_valid());
  DCHECK(input::TransferInputToViz() && !is_root);

  auto render_input_router = std::make_unique<input::RenderInputRouter>(
      /* host */ nullptr,
      /* fling_scheduler */ nullptr,
      /* delegate */ nullptr,
      base::SingleThreadTaskRunner::GetCurrentDefault());

  rir_map_.emplace(
      std::make_pair(frame_sink_id, std::move(render_input_router)));
}

void InputManager::OnDestroyedCompositorFrameSink(
    const FrameSinkId& frame_sink_id) {
  TRACE_EVENT("viz", "InputManager::OnDestroyedCompositorFrameSink",
              "frame_sink_id", frame_sink_id);

  rir_map_.erase(frame_sink_id);
}

void InputManager::CreateAndroidInputReceiver(
    const FrameSinkId& frame_sink_id,
    const gpu::SurfaceHandle& surface_handle) {
  // TODO(b/364201006): Request android to create input receiver.
}

}  // namespace viz
