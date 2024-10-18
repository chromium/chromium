// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_SUPPORT_ANDROID_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_SUPPORT_ANDROID_H_

#include <memory>

#include "components/input/android_input_helper.h"
#include "components/input/events_helper.h"
#include "components/viz/service/input/render_input_router_support_base.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"

namespace viz {

class VIZ_SERVICE_EXPORT RenderInputRouterSupportAndroid
    : public RenderInputRouterSupportBase,
      public ui::GestureProviderClient,
      public input::AndroidInputHelper::Delegate {
 public:
  explicit RenderInputRouterSupportAndroid(
      input::RenderInputRouter* rir,
      RenderInputRouterSupportBase::Delegate* delegate,
      const FrameSinkId& frame_sink_id);

  RenderInputRouterSupportAndroid(const RenderInputRouterSupportAndroid&) =
      delete;
  RenderInputRouterSupportAndroid& operator=(
      const RenderInputRouterSupportAndroid&) = delete;

  ~RenderInputRouterSupportAndroid() override;

  bool OnTouchEvent(const ui::MotionEventAndroid& event);
  bool ShouldRouteEvents() const;

  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;
  bool RequiresDoubleTapGestureEvents() const override;

  // RenderWidgetHostViewInput implementation.
  void ProcessAckedTouchEvent(
      const input::TouchEventWithLatencyInfo& touch,
      blink::mojom::InputEventResultState ack_result) override;
  void DidOverscroll(const ui::DidOverscrollParams& params) override;
  FrameSinkId GetRootFrameSinkId() override;
  SurfaceId GetCurrentSurfaceId() const override;
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

  // AndroidInputHelper::Delegate implementation.
  void SendGestureEvent(const blink::WebGestureEvent& event) override;
  ui::FilteredGestureProvider& GetGestureProvider() override;

 private:
  std::unique_ptr<input::AndroidInputHelper> input_helper_;

  // Provides gesture synthesis given a stream of touch events (derived from
  // Android MotionEvent's) and touch event acks.
  ui::FilteredGestureProvider gesture_provider_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_SUPPORT_ANDROID_H_
