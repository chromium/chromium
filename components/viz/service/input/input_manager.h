// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_INPUT_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_INPUT_MANAGER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "components/input/render_input_router.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/input/utils.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/frame_sinks/frame_sink_observer.h"
#include "components/viz/service/input/render_input_router_delegate_impl.h"
#include "gpu/ipc/common/surface_handle.h"

namespace input {
class TouchEmulator;
}

namespace viz {

struct FrameSinkMetadata {
  explicit FrameSinkMetadata(
      uint32_t grouping_id,
      std::unique_ptr<RenderInputRouterDelegateImpl> delegate);

  FrameSinkMetadata(const FrameSinkMetadata&) = delete;
  FrameSinkMetadata& operator=(const FrameSinkMetadata&) = delete;

  FrameSinkMetadata(FrameSinkMetadata&& other);
  FrameSinkMetadata& operator=(FrameSinkMetadata&& other);

  ~FrameSinkMetadata();

  uint32_t grouping_id;
  std::unique_ptr<RenderInputRouterDelegateImpl> rir_delegate;
};

class VIZ_SERVICE_EXPORT InputManager
    : public FrameSinkObserver,
      public input::RenderWidgetHostInputEventRouter::Delegate {
 public:
  explicit InputManager(FrameSinkManagerImpl* frame_sink_manager);

  InputManager(const InputManager&) = delete;
  InputManager& operator=(const InputManager&) = delete;

  ~InputManager() override;

  void OnCreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      bool is_root,
      input::mojom::RenderInputRouterConfigPtr render_input_router_config,
      bool create_input_receiver,
      gpu::SurfaceHandle surface_handle);

  // FrameSinkObserver overrides.
  void OnDestroyedCompositorFrameSink(
      const FrameSinkId& frame_sink_id) override;

  // RenderWidgetHostInputEventRouter::Delegate implementation.
  input::TouchEmulator* GetTouchEmulator(bool create_if_necessary) override;

 private:
#if BUILDFLAG(IS_ANDROID)
  void CreateAndroidInputReceiver(const FrameSinkId& frame_sink_id,
                                  const gpu::SurfaceHandle& surface_handle);
#endif  // BUILDFLAG(IS_ANDROID)

  friend class MockInputManager;

  // Keeps track of InputEventRouter corresponding to FrameSinkIds using a
  // CompositorFrameSink grouping_id sent from the browser, allowing mirroring
  // 1:1 relationship in browser between WebContentsImpl and
  // RenderWidgetHostInputEventRouter to Viz.
  base::flat_map</*grouping_id=*/uint32_t,
                 scoped_refptr<input::RenderWidgetHostInputEventRouter>>
      rwhier_map_;

  // Keeps track of metadata related to FrameSinkIds which are 1:1 to
  // RenderInputRouters.
  base::flat_map<FrameSinkId, FrameSinkMetadata> frame_sink_metadata_map_;

  // RenderInputRouter is created only for non-root layer tree frame sinks, i.e.
  // the layer tree frame sinks requested by renderers.
  base::flat_map<FrameSinkId, std::unique_ptr<input::RenderInputRouter>>
      rir_map_;

  raw_ptr<FrameSinkManagerImpl> frame_sink_manager_;

  base::WeakPtrFactory<InputManager> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_INPUT_MANAGER_H_
