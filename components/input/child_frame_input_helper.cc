// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/child_frame_input_helper.h"

#include "base/trace_event/trace_event.h"
#include "components/input/features.h"
#include "components/input/render_input_router.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"

namespace input {

ChildFrameInputHelper::~ChildFrameInputHelper() = default;

ChildFrameInputHelper::ChildFrameInputHelper(RenderWidgetHostViewInput* view,
                                             Delegate* delegate)
    : view_(view), delegate_(delegate) {
  CHECK(view);
}

void ChildFrameInputHelper::NotifyHitTestRegionUpdated(
    const viz::AggregatedHitTestRegion& region) {
  std::optional<gfx::RectF> screen_rect =
      region.transform.InverseMapRect(gfx::RectF(region.rect));
  if (!screen_rect) {
    last_stable_screen_rect_ = gfx::RectF();
    last_stable_screen_rect_for_iov2_ = gfx::RectF();
    screen_rect_stable_since_ = base::TimeTicks::Now();
    screen_rect_stable_since_for_iov2_ = base::TimeTicks::Now();
    return;
  }

  // Convert to DIP
  screen_rect->Scale(1. / view_->GetDeviceScaleFactor());

  // Movement as a proportion of frame size
  double horizontal_movement =
      screen_rect->width()
          ? std::abs(last_stable_screen_rect_.x() - screen_rect->x()) /
                screen_rect->width()
          : 0.0;
  double vertical_movement =
      screen_rect->height()
          ? std::abs(last_stable_screen_rect_.y() - screen_rect->y()) /
                screen_rect->height()
          : 0.0;
  if ((ToRoundedSize(screen_rect->size()) !=
       ToRoundedSize(last_stable_screen_rect_.size())) ||
      horizontal_movement >
          blink::FrameVisualProperties::MaxChildFrameScreenRectMovement() ||
      vertical_movement >
          blink::FrameVisualProperties::MaxChildFrameScreenRectMovement()) {
    last_stable_screen_rect_ = *screen_rect;
    screen_rect_stable_since_ = base::TimeTicks::Now();
  }
  // The legacy logic is based on manhattan distance.
  if ((ToRoundedSize(screen_rect->size()) !=
       ToRoundedSize(last_stable_screen_rect_for_iov2_.size())) ||
      (std::abs(last_stable_screen_rect_for_iov2_.x() - screen_rect->x()) +
           std::abs(last_stable_screen_rect_for_iov2_.y() - screen_rect->y()) >
       blink::FrameVisualProperties::
           MaxChildFrameScreenRectMovementForIOv2())) {
    last_stable_screen_rect_for_iov2_ = *screen_rect;
    screen_rect_stable_since_for_iov2_ = base::TimeTicks::Now();
  }
}

bool ChildFrameInputHelper::ScreenRectIsUnstableFor(
    const blink::WebInputEvent& event) {
  // Some tests generate events with artificial timestamps; ignore these.
  if (event.TimeStamp() < screen_rect_stable_since_) {
    return false;
  }
  if (event.TimeStamp() -
          base::Milliseconds(
              blink::FrameVisualProperties::MinScreenRectStableTimeMs()) <
      screen_rect_stable_since_) {
    return true;
  }
  if (auto* parent = view_->GetParentViewInput()) {
    return parent->ScreenRectIsUnstableFor(event);
  }
  return false;
}

bool ChildFrameInputHelper::ScreenRectIsUnstableForIOv2For(
    const blink::WebInputEvent& event) {
  // Some tests generate events with artificial timestamps; ignore these.
  if (event.TimeStamp() < screen_rect_stable_since_for_iov2_) {
    return false;
  }
  if (event.TimeStamp() -
          base::Milliseconds(blink::FrameVisualProperties::
                                 MinScreenRectStableTimeMsForIOv2()) <
      screen_rect_stable_since_for_iov2_) {
    return true;
  }
  if (RenderWidgetHostViewInput* parent = view_->GetParentViewInput()) {
    return parent->ScreenRectIsUnstableForIOv2For(event);
  }
  return false;
}

gfx::PointF ChildFrameInputHelper::TransformPointToRootCoordSpaceF(
    const gfx::PointF& point) {
  return TransformPointToRootCoordSpace(point);
}

gfx::PointF ChildFrameInputHelper::TransformPointToRootCoordSpace(
    const gfx::PointF& point) {
  if (!delegate_) {
    return point;
  }

  gfx::PointF transformed_point;
  TransformPointToCoordSpaceForView(point, delegate_->GetRootViewInput(),
                                    view_->GetFrameSinkId(),
                                    &transformed_point);
  return transformed_point;
}

gfx::PointF ChildFrameInputHelper::TransformRootPointToViewCoordSpace(
    const gfx::PointF& point) {
  if (!delegate_) {
    return point;
  }

  auto* root_rwhv = delegate_->GetRootViewInput();
  if (!root_rwhv) {
    return point;
  }

  gfx::PointF transformed_point;
  if (!root_rwhv->TransformPointToCoordSpaceForView(point, view_,
                                                    &transformed_point)) {
    return point;
  }
  return transformed_point;
}

bool ChildFrameInputHelper::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    input::RenderWidgetHostViewInput* target_view,
    gfx::PointF* transformed_point) {
  if (target_view == view_) {
    *transformed_point = point;
    return true;
  }

  return TransformPointToCoordSpaceForView(
      point, target_view, view_->GetFrameSinkId(), transformed_point);
}

bool ChildFrameInputHelper::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    input::RenderWidgetHostViewInput* target_view,
    const viz::FrameSinkId& local_frame_sink_id,
    gfx::PointF* transformed_point) {
  if (!delegate_) {
    return false;
  }

  RenderWidgetHostViewInput* root_view = delegate_->GetRootViewInput();
  if (!root_view) {
    return false;
  }

  // It is possible that neither the original surface or target surface is an
  // ancestor of the other in the RenderWidgetHostView tree (e.g. they could
  // be siblings). To account for this, the point is first transformed into the
  // root coordinate space and then the root is asked to perform the conversion.
  if (!root_view->TransformPointToLocalCoordSpace(point, local_frame_sink_id,
                                                  transformed_point)) {
    return false;
  }

  if (target_view == root_view) {
    return true;
  }

  return root_view->TransformPointToCoordSpaceForView(
      *transformed_point, target_view, transformed_point);
}

void ChildFrameInputHelper::TransformPointToRootSurface(gfx::PointF* point) {
  // This function is called by RenderWidgetHostInputEventRouter only for
  // root-views.
  NOTREACHED();
}

blink::mojom::InputEventResultState ChildFrameInputHelper::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  // A child renderer should never receive a GesturePinch event. Pinch events
  // can still be targeted to a child, but they must be processed without
  // sending the pinch event to the child (e.g. touchpad pinch synthesizes
  // wheel events to send to the child renderer).
  if (blink::WebInputEvent::IsPinchGestureEventType(input_event.GetType())) {
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    // Touchscreen pinch events may be targeted to a child in order to have the
    // child's TouchActionFilter filter them, but we may encounter
    // https://crbug.com/771330 which would let the pinch events through.
    if (gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchscreen) {
      return blink::mojom::InputEventResultState::kConsumed;
    }
    DUMP_WILL_BE_NOTREACHED();
  }

  if (input_event.GetType() == blink::WebInputEvent::Type::kGestureFlingStart) {
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    // Zero-velocity touchpad flings are an Aura-specific signal that the
    // touchpad scroll has ended, and should not be forwarded to the renderer.
    if (gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchpad &&
        !gesture_event.data.fling_start.velocity_x &&
        !gesture_event.data.fling_start.velocity_y) {
      // Here we indicate that there was no consumer for this event, as
      // otherwise the fling animation system will try to run an animation
      // and will also expect a notification when the fling ends. Since
      // CrOS just uses the GestureFlingStart with zero-velocity as a means
      // of indicating that touchpad scroll has ended, we don't actually want
      // a fling animation.
      // Note: this event handling is modeled on similar code in
      // TenderWidgetHostViewAura::FilterInputEvent().
      return blink::mojom::InputEventResultState::kNoConsumerExists;
    }
  }

  if (is_scroll_sequence_bubbling_ &&
      (input_event.GetType() ==
       blink::WebInputEvent::Type::kGestureScrollUpdate) &&
      delegate_) {
    // If we're bubbling, then to preserve latching behaviour, the child should
    // not consume this event. If the child has added its viewport to the scroll
    // chain, then any GSU events we send to the renderer could be consumed,
    // even though we intend for them to be bubbled. So we immediately bubble
    // any scroll updates without giving the child a chance to consume them.
    // If the child has not added its viewport to the scroll chain, then we
    // know that it will not attempt to consume the rest of the scroll
    // sequence.
    return blink::mojom::InputEventResultState::kNoConsumerExists;
  }

  return blink::mojom::InputEventResultState::kNotConsumed;
}

void ChildFrameInputHelper::StopFlingingIfNecessary(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  // In case of scroll bubbling the target view is in charge of stopping the
  // fling if needed.
  if (is_scroll_sequence_bubbling_) {
    return;
  }

  // Delegates to RenderWidgetHostViewInput to stop flinging if the GSU event
  // with momentum phase was not consumed by the renderer.
  view_->RenderWidgetHostViewInput::StopFlingingIfNecessary(event, ack_result);
}

void ChildFrameInputHelper::GestureEventAckHelper(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  // Stop flinging if a GSU event with momentum phase is sent to the renderer
  // but not consumed.
  StopFlingingIfNecessary(event, ack_result);

  if (event.IsTouchpadZoomEvent()) {
    ProcessTouchpadZoomEventAckInRoot(event, ack_source, ack_result);
  }

  // GestureScrollBegin is a blocking event; It is forwarded for bubbling if
  // its ack is not consumed. For the rest of the scroll events
  // (GestureScrollUpdate, GestureScrollEnd) are bubbled if the
  // GestureScrollBegin was bubbled. If the browser consumed the event, the
  // event was filtered and shouldn't affect the state of scroll bubbling.
  bool event_filtered =
      ack_source == blink::mojom::InputEventResultSource::kBrowser &&
      ack_result == blink::mojom::InputEventResultState::kConsumed;

  // TODO(crbug.com/346629231): Remove flag guard once this lands. Prior to the
  // fix this section was always entered.
  if (!event_filtered ||
      !base::FeatureList::IsEnabled(input::features::kScrollBubblingFix)) {
    if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
      DCHECK(!is_scroll_sequence_bubbling_);
      is_scroll_sequence_bubbling_ =
          ack_result == blink::mojom::InputEventResultState::kNotConsumed ||
          ack_result == blink::mojom::InputEventResultState::kNoConsumerExists;
    }

    if (is_scroll_sequence_bubbling_ &&
        (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin ||
         event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate ||
         event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd)) {
      const bool can_continue = BubbleScrollEvent(event);
      if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd ||
          !can_continue) {
        is_scroll_sequence_bubbling_ = false;
      }
    }
  }

  TRACE_EVENT_INSTANT0("input", "Did_Ack_To_Frame_Connector",
                       TRACE_EVENT_SCOPE_THREAD);
  DidAckGestureEvent(event, ack_result);
}

void ChildFrameInputHelper::ForwardTouchpadZoomEventIfNecessary(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  // ACKs of synthetic wheel events for touchpad pinch or double tap are
  // processed in the root RWHV.
  NOTREACHED();
}

void ChildFrameInputHelper::ProcessTouchpadZoomEventAckInRoot(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  DCHECK(event.IsTouchpadZoomEvent());
  if (!delegate_) {
    return;
  }
  auto* root_view = delegate_->GetRootViewInput();
  if (!root_view) {
    return;
  }

  blink::WebGestureEvent root_event(event);
  const gfx::PointF root_point =
      TransformPointToRootCoordSpaceF(event.PositionInWidget());
  root_event.SetPositionInWidget(root_point);
  root_view->GestureEventAck(root_event, ack_source, ack_result);
}

bool ChildFrameInputHelper::BubbleScrollEvent(
    const blink::WebGestureEvent& event) {
  TRACE_EVENT1("input", "ChildFrameInputHelper::BubbleScrollEvent", "type",
               blink::WebInputEvent::GetName(event.GetType()));
  DCHECK(event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin ||
         event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate ||
         event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd);

  if (!delegate_) {
    return false;
  }
  auto* parent_view = delegate_->GetParentViewInput();

  if (!parent_view) {
    return false;
  }

  auto* event_router = parent_view->GetViewRenderInputRouter()
                           ->delegate()
                           ->GetInputEventRouter();

  // We will only convert the coordinates back to the root here. The
  // RenderWidgetHostInputEventRouter will determine which ancestor view will
  // receive a resent gesture event, so it will be responsible for converting to
  // the coordinates of the target view.
  blink::WebGestureEvent resent_gesture_event(event);
  const gfx::PointF root_point =
      view_->TransformPointToRootCoordSpaceF(event.PositionInWidget());
  resent_gesture_event.SetPositionInWidget(root_point);
  // When a gesture event is bubbled to the parent frame, set the allowed touch
  // action of the parent frame to Auto so that this gesture event is allowed.
  parent_view->GetViewRenderInputRouter()
      ->input_router()
      ->ForceSetTouchActionAuto();

  TRACE_EVENT_INSTANT0("input", "Did_Bubble_To_InputEventRouter",
                       TRACE_EVENT_SCOPE_THREAD);
  return event_router->BubbleScrollEvent(parent_view, view_,
                                         resent_gesture_event);
}

void ChildFrameInputHelper::DidAckGestureEvent(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  if (!delegate_) {
    return;
  }

  auto* root_view = delegate_->GetRootViewInput();
  if (!root_view) {
    return;
  }

  root_view->ChildDidAckGestureEvent(event, ack_result);
}

}  // namespace input
