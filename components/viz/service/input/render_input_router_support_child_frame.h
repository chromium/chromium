// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_SUPPORT_CHILD_FRAME_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_SUPPORT_CHILD_FRAME_H_

#include <memory>
#include <string>

#include "components/input/child_frame_input_helper.h"
#include "components/input/render_input_router.h"
#include "components/viz/service/input/render_input_router_support_base.h"

namespace viz {

class VIZ_SERVICE_EXPORT RenderInputRouterSupportChildFrame
    : public RenderInputRouterSupportBase,
      public input::ChildFrameInputHelper::Delegate {
 public:
  explicit RenderInputRouterSupportChildFrame(
      input::RenderInputRouter* rir,
      RenderInputRouterSupportBase::Delegate* delegate,
      const FrameSinkId& frame_sink_id);

  ~RenderInputRouterSupportChildFrame() override;

  RenderInputRouterSupportChildFrame(
      const RenderInputRouterSupportChildFrame&) = delete;
  RenderInputRouterSupportChildFrame& operator=(
      const RenderInputRouterSupportChildFrame&) = delete;

  // RenderWidgetHostViewInput implementation.
  const LocalSurfaceId& GetLocalSurfaceId() const override;
  // Empty implementation since SelectionController is not handled on Viz.
  void DidStopFlinging() override {}
  RenderInputRouterSupportBase* GetRootView() override;
  FrameSinkId GetRootFrameSinkId() override;
  SurfaceId GetCurrentSurfaceId() const override;
  void NotifyHitTestRegionUpdated(
      const AggregatedHitTestRegion& region) override;
  bool ScreenRectIsUnstableFor(const blink::WebInputEvent& event) override;
  bool ScreenRectIsUnstableForIOv2For(
      const blink::WebInputEvent& event) override;
  void PreProcessTouchEvent(const blink::WebTouchEvent& event) override {}
  void PreProcessMouseEvent(const blink::WebMouseEvent& event) override;
  gfx::PointF TransformRootPointToViewCoordSpace(
      const gfx::PointF& point) override;
  gfx::PointF TransformPointToRootCoordSpaceF(
      const gfx::PointF& point) override;
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      input::RenderWidgetHostViewInput* target_view,
      gfx::PointF* transformed_point) override;
  void TransformPointToRootSurface(gfx::PointF* point) override;
  blink::mojom::InputEventResultState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;
  bool IsPointerLocked() override;
  void StopFlingingIfNecessary(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result) override;

  // RenderWidgetHostViewInput, ChildFrameInputHelper::Delegate implementation.
  RenderInputRouterSupportBase* GetParentViewInput() override;

  // ChildFrameInputHelper::Delegate implementation.
  RenderInputRouterSupportBase* GetRootViewInput() override;

 private:
  void ForwardTouchpadZoomEventIfNecessary(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result) override;

  std::unique_ptr<input::ChildFrameInputHelper> input_helper_;

  base::WeakPtrFactory<RenderInputRouterSupportChildFrame> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_SUPPORT_CHILD_FRAME_H_
