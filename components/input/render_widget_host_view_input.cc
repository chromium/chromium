// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/render_widget_host_view_input.h"

#include "base/notreached.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "ui/gfx/geometry/dip_util.h"

namespace input {

RenderWidgetHostViewInput::RenderWidgetHostViewInput() = default;
RenderWidgetHostViewInput::~RenderWidgetHostViewInput() = default;

void RenderWidgetHostViewInput::NotifyObserversAboutShutdown() {
  // Note: RenderWidgetHostInputEventRouter is an observer, and uses the
  // following notification to remove this view from its surface owners map.
  for (auto& observer : observers_) {
    observer.OnRenderWidgetHostViewInputDestroyed(this);
  }
  // All observers are required to disconnect after they are notified.
  CHECK(observers_.empty());
}

void RenderWidgetHostViewInput::ProcessAckedTouchEvent(
    const input::TouchEventWithLatencyInfo& touch,
    blink::mojom::InputEventResultState ack_result) {
  DUMP_WILL_BE_NOTREACHED();
}

viz::FrameSinkId RenderWidgetHostViewInput::GetRootFrameSinkId() {
  return viz::FrameSinkId();
}

bool RenderWidgetHostViewInput::ScreenRectIsUnstableFor(
    const blink::WebInputEvent& event) {
  return false;
}

bool RenderWidgetHostViewInput::ScreenRectIsUnstableForIOv2For(
    const blink::WebInputEvent& event) {
  return false;
}

gfx::PointF RenderWidgetHostViewInput::TransformRootPointToViewCoordSpace(
    const gfx::PointF& point) {
  return point;
}

bool RenderWidgetHostViewInput::TransformPointToLocalCoordSpace(
    const gfx::PointF& point,
    const viz::FrameSinkId& original_frame_sink_id,
    gfx::PointF* transformed_point) {
  viz::FrameSinkId target_frame_sink_id = GetFrameSinkId();
  if (!original_frame_sink_id.is_valid() || !target_frame_sink_id.is_valid()) {
    return false;
  }
  if (original_frame_sink_id == target_frame_sink_id) {
    return true;
  }
  if (!GetViewRenderInputRouter() || !GetViewRenderInputRouter()->delegate()) {
    return false;
  }
  auto* router = GetViewRenderInputRouter()->delegate()->GetInputEventRouter();
  if (!router) {
    return false;
  }
  *transformed_point = point;
  return TransformPointToTargetCoordSpace(
      router->FindViewFromFrameSinkId(original_frame_sink_id),
      router->FindViewFromFrameSinkId(target_frame_sink_id), point,
      transformed_point);
}

bool RenderWidgetHostViewInput::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    input::RenderWidgetHostViewInput* target_view,
    gfx::PointF* transformed_point) {
  NOTREACHED_IN_MIGRATION();
  return true;
}

bool RenderWidgetHostViewInput::GetTransformToViewCoordSpace(
    input::RenderWidgetHostViewInput* target_view,
    gfx::Transform* transform) {
  CHECK(transform);
  if (target_view == this) {
    transform->MakeIdentity();
    return true;
  }

  viz::FrameSinkId root_frame_sink_id = GetRootFrameSinkId();
  if (!root_frame_sink_id.is_valid()) {
    return false;
  }

  const auto& display_hit_test_query_map = GetDisplayHitTestQuery();
  const auto iter = display_hit_test_query_map.find(root_frame_sink_id);
  if (iter == display_hit_test_query_map.end()) {
    return false;
  }
  viz::HitTestQuery* query = iter->second.get();

  gfx::Transform transform_this_to_root;
  if (GetFrameSinkId() != root_frame_sink_id) {
    gfx::Transform transform_root_to_this;
    if (!query->GetTransformToTarget(GetFrameSinkId(),
                                     &transform_root_to_this)) {
      return false;
    }
    if (!transform_root_to_this.GetInverse(&transform_this_to_root)) {
      return false;
    }
  }
  gfx::Transform transform_root_to_target;
  if (!query->GetTransformToTarget(target_view->GetFrameSinkId(),
                                   &transform_root_to_target)) {
    return false;
  }

  // TODO(wjmaclean): In TransformPointToTargetCoordSpace the device scale
  // factor is taken from the original view ... does that matter? Presumably
  // all the views have the same dsf.
  float device_scale_factor = GetDeviceScaleFactor();
  gfx::Transform transform_to_pixel;
  transform_to_pixel.Scale(device_scale_factor, device_scale_factor);
  gfx::Transform transform_from_pixel;
  transform_from_pixel.Scale(1.f / device_scale_factor,
                             1.f / device_scale_factor);

  // Note: gfx::Transform includes optimizations to early-out for scale = 1 or
  // concatenating an identity matrix, so we don't add those checks here.
  transform->MakeIdentity();

  transform->PostConcat(transform_to_pixel);
  transform->PostConcat(transform_this_to_root);
  transform->PostConcat(transform_root_to_target);
  transform->PostConcat(transform_from_pixel);

  return true;
}

input::RenderWidgetHostViewInput*
RenderWidgetHostViewInput::GetParentViewInput() {
  return nullptr;
}

blink::mojom::InputEventResultState RenderWidgetHostViewInput::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  // By default, input events are simply forwarded to the renderer.
  return blink::mojom::InputEventResultState::kNotConsumed;
}

void RenderWidgetHostViewInput::WheelEventAck(
    const blink::WebMouseWheelEvent& event,
    blink::mojom::InputEventResultState ack_result) {}

void RenderWidgetHostViewInput::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {}

void RenderWidgetHostViewInput::ChildDidAckGestureEvent(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {}

void RenderWidgetHostViewInput::DisplayCursor(const ui::Cursor& cursor) {
  return;
}

input::CursorManager* RenderWidgetHostViewInput::GetCursorManager() {
  return nullptr;
}

void RenderWidgetHostViewInput::TransformPointToRootSurface(
    gfx::PointF* point) {
  return;
}

int RenderWidgetHostViewInput::GetMouseWheelMinimumGranularity() const {
  // Most platforms can specify the floating-point delta in the wheel event so
  // they don't have a minimum granularity. Android is currently the only
  // platform that overrides this.
  return 0;
}

void RenderWidgetHostViewInput::AddObserver(
    input::RenderWidgetHostViewInputObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderWidgetHostViewInput::RemoveObserver(
    input::RenderWidgetHostViewInputObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RenderWidgetHostViewInput::StopFling() {
  if (!GetViewRenderInputRouter()) {
    return;
  }

  GetViewRenderInputRouter()->StopFling();

  // In case of scroll bubbling tells the child's fling controller which is in
  // charge of generating GSUs to stop flinging.
  if (GetViewRenderInputRouter()->delegate() &&
      GetViewRenderInputRouter()->delegate()->GetInputEventRouter()) {
    GetViewRenderInputRouter()->delegate()->GetInputEventRouter()->StopFling();
  }
}

void RenderWidgetHostViewInput::StopFlingingIfNecessary(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  // Reset view_stopped_flinging_for_test_ at the beginning of the scroll
  // sequence.
  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
    view_stopped_flinging_for_test_ = false;
  }

  bool processed = blink::mojom::InputEventResultState::kConsumed == ack_result;
  if (!processed &&
      event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate &&
      event.data.scroll_update.inertial_phase ==
          blink::WebGestureEvent::InertialPhaseState::kMomentum &&
      event.SourceDevice() != blink::WebGestureDevice::kSyntheticAutoscroll) {
    StopFling();
    view_stopped_flinging_for_test_ = true;
  }
}

void RenderWidgetHostViewInput::ForwardTouchpadZoomEventIfNecessary(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  if (!event.IsTouchpadZoomEvent()) {
    return;
  }
  if (!event.NeedsWheelEvent()) {
    return;
  }

  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGesturePinchBegin:
      // Don't send the begin event until we get the first unconsumed update, so
      // that we elide pinch gesture steams consisting of only a begin and end.
      pending_touchpad_pinch_begin_ = event;
      pending_touchpad_pinch_begin_->SetNeedsWheelEvent(false);
      break;
    case blink::WebInputEvent::Type::kGesturePinchUpdate:
      if (ack_result != blink::mojom::InputEventResultState::kConsumed &&
          !event.data.pinch_update.zoom_disabled) {
        if (pending_touchpad_pinch_begin_) {
          GetViewRenderInputRouter()->ForwardGestureEvent(
              *pending_touchpad_pinch_begin_);
          pending_touchpad_pinch_begin_.reset();
        }
        // Now that the synthetic wheel event has gone unconsumed, we have the
        // pinch event actually change the page scale.
        blink::WebGestureEvent pinch_event(event);
        pinch_event.SetNeedsWheelEvent(false);
        GetViewRenderInputRouter()->ForwardGestureEvent(pinch_event);
      }
      break;
    case blink::WebInputEvent::Type::kGesturePinchEnd:
      if (pending_touchpad_pinch_begin_) {
        pending_touchpad_pinch_begin_.reset();
      } else {
        blink::WebGestureEvent pinch_end_event(event);
        pinch_end_event.SetNeedsWheelEvent(false);
        GetViewRenderInputRouter()->ForwardGestureEvent(pinch_end_event);
      }
      break;
    case blink::WebInputEvent::Type::kGestureDoubleTap:
      if (ack_result != blink::mojom::InputEventResultState::kConsumed) {
        blink::WebGestureEvent double_tap(event);
        double_tap.SetNeedsWheelEvent(false);
        // TODO(mcnee): Support double-tap zoom gesture for OOPIFs. For now,
        // we naively send this to the main frame. If this is over an OOPIF,
        // then the iframe element will incorrectly be used for the scale
        // calculation rather than the element in the OOPIF.
        // https://crbug.com/758348
        GetViewRenderInputRouter()->ForwardGestureEvent(double_tap);
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

// TODO(wjmaclean): Would it simplify this function if we re-implemented it
// using GetTransformToViewCoordSpace()?
bool RenderWidgetHostViewInput::TransformPointToTargetCoordSpace(
    input::RenderWidgetHostViewInput* original_view,
    input::RenderWidgetHostViewInput* target_view,
    const gfx::PointF& point,
    gfx::PointF* transformed_point) const {
  CHECK(original_view);
  CHECK(target_view);
  viz::FrameSinkId root_frame_sink_id = original_view->GetRootFrameSinkId();
  if (!root_frame_sink_id.is_valid()) {
    return false;
  }
  const auto& display_hit_test_query_map = GetDisplayHitTestQuery();
  const auto iter = display_hit_test_query_map.find(root_frame_sink_id);
  if (iter == display_hit_test_query_map.end()) {
    return false;
  }
  viz::HitTestQuery* query = iter->second.get();

  std::vector<viz::FrameSinkId> target_ancestors;
  target_ancestors.push_back(target_view->GetFrameSinkId());

  input::RenderWidgetHostViewInput* cur_view = target_view;
  while (cur_view->GetParentViewInput()) {
    cur_view = cur_view->GetParentViewInput();
    if (!cur_view) {
      return false;
    }
    target_ancestors.push_back(cur_view->GetFrameSinkId());
  }
  if (target_ancestors.back() != root_frame_sink_id) {
    target_ancestors.push_back(root_frame_sink_id);
  }

  float device_scale_factor = original_view->GetDeviceScaleFactor();
  CHECK_GT(device_scale_factor, 0.0f);
  // TODO(crbug.com/41460959): Optimize so that |point_in_pixels| doesn't need
  // to be in the coordinate space of the root surface in HitTestQuery.
  gfx::Transform transform_root_to_original;
  query->GetTransformToTarget(original_view->GetFrameSinkId(),
                              &transform_root_to_original);
  const std::optional<gfx::PointF> point_in_pixels =
      transform_root_to_original.InverseMapPoint(
          gfx::ConvertPointToPixels(point, device_scale_factor));
  if (!point_in_pixels.has_value()) {
    return false;
  }
  gfx::PointF transformed_point_in_physical_pixels;
  if (!query->TransformLocationForTarget(
          target_ancestors, *point_in_pixels,
          &transformed_point_in_physical_pixels)) {
    return false;
  }
  *transformed_point = gfx::ConvertPointToDips(
      transformed_point_in_physical_pixels, device_scale_factor);
  return true;
}

}  // namespace input
