// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_SUPPORT_BASE_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_SUPPORT_BASE_H_

#include "base/observer_list.h"
#include "components/input/render_widget_host_view_input.h"
#include "components/viz/common/hit_test/hit_test_data_provider.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class VIZ_SERVICE_EXPORT RenderInputRouterSupportBase
    : public input::RenderWidgetHostViewInput {
 public:
  RenderInputRouterSupportBase(const RenderInputRouterSupportBase&) = delete;
  RenderInputRouterSupportBase& operator=(const RenderInputRouterSupportBase&) =
      delete;

  ~RenderInputRouterSupportBase() override;

  class Delegate {
   public:
    virtual float GetDeviceScaleFactorForId(
        const FrameSinkId& frame_sink_id) = 0;
  };

  // StylusInterface implementation.
  bool ShouldInitiateStylusWriting() override;
  void NotifyHoverActionStylusWritable(bool stylus_writable) override;

  // RenderWidgetHostViewInput implementation
  base::WeakPtr<input::RenderWidgetHostViewInput> GetInputWeakPtr() override;
  input::RenderInputRouter* GetViewRenderInputRouter() override;
  void ProcessMouseEvent(const blink::WebMouseEvent& event,
                         const ui::LatencyInfo& latency) override;
  void ProcessMouseWheelEvent(const blink::WebMouseWheelEvent& event,
                              const ui::LatencyInfo& latency) override;
  void ProcessTouchEvent(const blink::WebTouchEvent& event,
                         const ui::LatencyInfo& latency) override;
  void ProcessGestureEvent(const blink::WebGestureEvent& event,
                           const ui::LatencyInfo& latency) override;
  RenderInputRouterSupportBase* GetRootView() override;
  const FrameSinkId& GetFrameSinkId() const override;
  void OnAutoscrollStart() override;
  float GetDeviceScaleFactor() const final;
  bool IsPointerLocked() override;

 protected:
  explicit RenderInputRouterSupportBase(input::RenderInputRouter* rir,
                                        Delegate* delegate,
                                        const FrameSinkId& frame_sink_id);
  void UpdateFrameSinkIdRegistration() override;

 private:
  const FrameSinkId frame_sink_id_;

  // |delegate_| is the InputManager and outlives |this|.
  raw_ptr<Delegate> delegate_;
  // |rir_| is owned by InputManager and outlives it's associated
  // RenderInputRouterSupportBase, so it's safe to keep a reference to it.
  const raw_ref<input::RenderInputRouter> rir_;

  base::WeakPtrFactory<RenderInputRouterSupportBase> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_SUPPORT_BASE_H_
