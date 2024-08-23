// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_INPUT_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_INPUT_MANAGER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "components/input/render_input_router.h"
#include "components/input/utils.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/frame_sinks/frame_sink_observer.h"

namespace viz {

class VIZ_SERVICE_EXPORT InputManager : public FrameSinkObserver {
 public:
  explicit InputManager(FrameSinkManagerImpl* frame_sink_manager);

  InputManager(const InputManager&) = delete;
  InputManager& operator=(const InputManager&) = delete;

  ~InputManager() override;

  void OnCreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      bool is_root,
      input::mojom::RenderInputRouterConfigPtr render_input_router_config);

  // FrameSinkObserver overrides.
  void OnRegisteredFrameSinkId(const FrameSinkId& frame_sink_id) override {}
  void OnInvalidatedFrameSinkId(const FrameSinkId& frame_sink_id) override {}
  void OnCreatedCompositorFrameSink(const FrameSinkId& frame_sink_id,
                                    bool is_root) override {}
  void OnDestroyedCompositorFrameSink(
      const FrameSinkId& frame_sink_id) override;
  void OnRegisteredFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override {}
  void OnUnregisteredFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override {}
  void OnFrameSinkDidBeginFrame(const FrameSinkId& frame_sink_id,
                                const BeginFrameArgs& args) override {}
  void OnFrameSinkDidFinishFrame(const FrameSinkId& frame_sink_id,
                                 const BeginFrameArgs& args) override {}
  void OnCaptureStarted(const FrameSinkId& frame_sink_id) override {}

 private:
  friend class MockInputManager;

  // RenderInputRouter is created only for non-root layer tree frame sinks, i.e.
  // the layer tree frame sinks requested by renderers.
  base::flat_map<FrameSinkId, std::unique_ptr<input::RenderInputRouter>>
      rir_map_;

  raw_ptr<FrameSinkManagerImpl> frame_sink_manager_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_INPUT_MANAGER_H_
