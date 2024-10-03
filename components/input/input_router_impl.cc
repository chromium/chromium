// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/input_router_impl.h"

#include <math.h>

#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "components/input/gesture_event_queue.h"
#include "components/input/input_disposition_handler.h"
#include "components/input/input_event_ack_state.h"
#include "components/input/input_router_client.h"
#include "components/input/utils.h"
#include "components/input/web_touch_event_traits.h"
#include "services/tracing/public/cpp/perfetto/flow_event_utils.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-shared.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/latency/latency_info.h"

namespace input {

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using perfetto::protos::pbzero::ChromeLatencyInfo;
using perfetto::protos::pbzero::ChromeLatencyInfo2;
using perfetto::protos::pbzero::TrackEvent;
using ui::WebInputEventTraits;

namespace {

bool WasHandled(blink::mojom::InputEventResultState state) {
  switch (state) {
    case blink::mojom::InputEventResultState::kConsumed:
    case blink::mojom::InputEventResultState::kNoConsumerExists:
    case blink::mojom::InputEventResultState::kUnknown:
      return true;
    default:
      return false;
  }
}

std::unique_ptr<blink::WebCoalescedInputEvent> ScaleEvent(
    const WebInputEvent& event,
    double scale,
    const ui::LatencyInfo& latency_info) {
  std::unique_ptr<blink::WebInputEvent> event_in_viewport =
      ui::ScaleWebInputEvent(event, scale, latency_info.trace_id());
  return std::make_unique<blink::WebCoalescedInputEvent>(
      event_in_viewport ? std::move(event_in_viewport) : event.Clone(),
      std::vector<std::unique_ptr<WebInputEvent>>(),
      std::vector<std::unique_ptr<WebInputEvent>>(), latency_info);
}

}  // namespace

InputRouterImpl::InputRouterImpl(
    InputRouterClient* client,
    InputDispositionHandler* disposition_handler,
    FlingControllerSchedulerClient* fling_scheduler_client,
    const Config& config)
    : client_(client),
      disposition_handler_(disposition_handler),
      touch_scroll_started_sent_(false),
      wheel_event_queue_(this),
      touch_event_queue_(this, config.touch_config),
      touchpad_pinch_event_queue_(this),
      gesture_event_queue_(this,
                           this,
                           fling_scheduler_client,
                           config.gesture_config),
      device_scale_factor_(1.f) {
  weak_this_ = weak_ptr_factory_.GetWeakPtr();

  DCHECK(client);
  DCHECK(disposition_handler);
  DCHECK(fling_scheduler_client);
  UpdateTouchAckTimeoutEnabled();
}

InputRouterImpl::~InputRouterImpl() {}

void InputRouterImpl::SendMouseEvent(
    const MouseEventWithLatencyInfo& mouse_event,
    MouseEventCallback event_result_callback) {
  if ((mouse_event.event.GetType() == WebInputEvent::Type::kMouseDown &&
       gesture_event_queue_.GetTouchpadTapSuppressionController()
           ->ShouldSuppressMouseDown(mouse_event)) ||
      (mouse_event.event.GetType() == WebInputEvent::Type::kMouseUp &&
       gesture_event_queue_.GetTouchpadTapSuppressionController()
           ->ShouldSuppressMouseUp())) {
    std::move(event_result_callback)
        .Run(mouse_event, blink::mojom::InputEventResultSource::kBrowser,
             blink::mojom::InputEventResultState::kIgnored);
    return;
  }

  SendMouseEventImmediately(mouse_event, std::move(event_result_callback));
}

void InputRouterImpl::SendWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  wheel_event_queue_.QueueEvent(wheel_event);
}

void InputRouterImpl::SendKeyboardEvent(
    const NativeWebKeyboardEventWithLatencyInfo& key_event,
    KeyboardEventCallback event_result_callback) {
  gesture_event_queue_.StopFling();
  blink::mojom::WidgetInputHandler::DispatchEventCallback callback =
      base::BindOnce(&InputRouterImpl::KeyboardEventHandled, weak_this_,
                     key_event, std::move(event_result_callback));
  FilterAndSendWebInputEvent(key_event.event, key_event.latency,
                             std::move(callback));
}

void InputRouterImpl::SendGestureEvent(
    const GestureEventWithLatencyInfo& original_gesture_event) {
  TRACE_EVENT0("input", "InputRouterImpl::SendGestureEvent");
  input_stream_validator_.Validate(original_gesture_event.event);

  GestureEventWithLatencyInfo gesture_event(original_gesture_event);

  if (gesture_event_queue_.PassToFlingController(gesture_event)) {
    TRACE_EVENT_INSTANT0("input", "FilteredForFling", TRACE_EVENT_SCOPE_THREAD);
    disposition_handler_->OnGestureEventAck(
        gesture_event, blink::mojom::InputEventResultSource::kBrowser,
        blink::mojom::InputEventResultState::kConsumed);
    return;
  }

  FilterGestureEventResult result =
      touch_action_filter_.FilterGestureEvent(&gesture_event.event);
  if (result == FilterGestureEventResult::kDelayed) {
    TRACE_EVENT_INSTANT0("input", "DeferredForTouchAction",
                         TRACE_EVENT_SCOPE_THREAD);
    gesture_event_queue_.QueueDeferredEvents(gesture_event);
    return;
  }
  SendGestureEventWithoutQueueing(gesture_event, result);
}

void InputRouterImpl::SendGestureEventWithoutQueueing(
    GestureEventWithLatencyInfo& gesture_event,
    const FilterGestureEventResult& existing_result) {
  TRACE_EVENT0("input", "InputRouterImpl::SendGestureEventWithoutQueueing");
  DCHECK_NE(existing_result, FilterGestureEventResult::kDelayed);
  if (existing_result == FilterGestureEventResult::kFiltered) {
    TRACE_EVENT_INSTANT0("input", "FilteredForTouchAction",
                         TRACE_EVENT_SCOPE_THREAD);
    disposition_handler_->OnGestureEventAck(
        gesture_event, blink::mojom::InputEventResultSource::kBrowser,
        blink::mojom::InputEventResultState::kConsumed);
    return;
  }

  // Handle scroll gesture events for stylus writing. If we could not start
  // writing for any reason, we should not filter the scroll events.
  if (HandleGestureScrollForStylusWriting(gesture_event.event)) {
    disposition_handler_->OnGestureEventAck(
        gesture_event, blink::mojom::InputEventResultSource::kBrowser,
        blink::mojom::InputEventResultState::kConsumed);
    return;
  }

  wheel_event_queue_.OnGestureScrollEvent(gesture_event);

  if (gesture_event.event.SourceDevice() ==
      blink::WebGestureDevice::kTouchscreen) {
    if (gesture_event.event.GetType() ==
        blink::WebInputEvent::Type::kGestureScrollBegin) {
      touch_scroll_started_sent_ = false;
    } else if (!touch_scroll_started_sent_ &&
               gesture_event.event.GetType() ==
                   blink::WebInputEvent::Type::kGestureScrollUpdate) {
      // A touch scroll hasn't really started until the first
      // GestureScrollUpdate event.  Eg. if the page consumes all touchmoves
      // then no scrolling really ever occurs (even though we still send
      // GestureScrollBegin).
      touch_scroll_started_sent_ = true;
      touch_event_queue_.PrependTouchScrollNotification();
    }
  }

  if (gesture_event.event.IsTouchpadZoomEvent() &&
      gesture_event.event.NeedsWheelEvent()) {
    touchpad_pinch_event_queue_.QueueEvent(gesture_event);
    return;
  }

  if (!gesture_event_queue_.DebounceOrForwardEvent(gesture_event)) {
    TRACE_EVENT_INSTANT0("input", "FilteredForDebounce",
                         TRACE_EVENT_SCOPE_THREAD);
    disposition_handler_->OnGestureEventAck(
        gesture_event, blink::mojom::InputEventResultSource::kBrowser,
        blink::mojom::InputEventResultState::kConsumed);
  }
}

bool InputRouterImpl::HandleGestureScrollForStylusWriting(
    const blink::WebGestureEvent& event) {
  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureScrollBegin: {
      if (event.data.scroll_begin.pointer_count != 1)
        break;

      const float& deltaXHint = event.data.scroll_begin.delta_x_hint;
      const float& deltaYHint = event.data.scroll_begin.delta_y_hint;
      if (deltaXHint == 0.0 && deltaYHint == 0.0)
        break;

      if (!client_->GetStylusInterface()) {
        break;
      }

      std::optional<cc::TouchAction> allowed_touch_action =
          AllowedTouchAction();
      // Don't handle for non-writable areas as kInternalNotWritable bit is set.
      if (!allowed_touch_action.has_value() ||
          (allowed_touch_action.value() &
           cc::TouchAction::kInternalNotWritable) ==
              cc::TouchAction::kInternalNotWritable)
        break;

      // Check if we can initiate stylus writing as we have detected stylus
      // writing movement, and treat scroll gesture as stylus input if it can be
      // initiated.
      if (client_->GetStylusInterface()->ShouldInitiateStylusWriting()) {
        stylus_writing_started_ = true;
        // The below call is done to Focus the stylus writable input element.
        client_->OnStartStylusWriting();
        return true;
      }
      break;
    }
    case WebInputEvent::Type::kGestureScrollUpdate:
      // TODO(crbug.com/40843488): Pass the queued scroll delta to stylus
      // writing recognition system.
      return stylus_writing_started_;
    case WebInputEvent::Type::kGestureScrollEnd: {
      // When stylus writing starts, Touch Move events would be forwarded to
      // stylus recognition system and gesture detection would be reset. We
      // would receive the GestureScrollEnd here after that.
      if (stylus_writing_started_) {
        stylus_writing_started_ = false;
        return true;
      }
      break;
    }
    default:
      break;
  }
  return false;
}

void InputRouterImpl::SendTouchEvent(
    const TouchEventWithLatencyInfo& touch_event) {
  TouchEventWithLatencyInfo updated_touch_event = touch_event;
  SetMovementXYForTouchPoints(&updated_touch_event.event);
  input_stream_validator_.Validate(updated_touch_event.event);
  touch_event_queue_.QueueEvent(updated_touch_event);
}

void InputRouterImpl::NotifySiteIsMobileOptimized(bool is_mobile_optimized) {
  touch_event_queue_.SetIsMobileOptimizedSite(is_mobile_optimized);
}

bool InputRouterImpl::HasPendingEvents() const {
  return !touch_event_queue_.Empty() || !gesture_event_queue_.empty() ||
         wheel_event_queue_.has_pending() ||
         touchpad_pinch_event_queue_.has_pending();
}

void InputRouterImpl::SetDeviceScaleFactor(float device_scale_factor) {
  device_scale_factor_ = device_scale_factor;
}

void InputRouterImpl::SetForceEnableZoom(bool enabled) {
  touch_action_filter_.SetForceEnableZoom(enabled);
}

std::optional<cc::TouchAction> InputRouterImpl::AllowedTouchAction() {
  return touch_action_filter_.allowed_touch_action();
}

std::optional<cc::TouchAction> InputRouterImpl::ActiveTouchAction() {
  return touch_action_filter_.active_touch_action();
}

mojo::PendingRemote<blink::mojom::WidgetInputHandlerHost>
InputRouterImpl::BindNewHost(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  host_receiver_.reset();
  return host_receiver_.BindNewPipeAndPassRemote(task_runner);
}

void InputRouterImpl::StopFling() {
  gesture_event_queue_.StopFling();
}

void InputRouterImpl::ProcessDeferredGestureEventQueue() {
  TRACE_EVENT0("input", "InputRouterImpl::ProcessDeferredGestureEventQueue");
  GestureEventQueue::GestureQueue deferred_gesture_events =
      gesture_event_queue_.TakeDeferredEvents();
  for (auto& it : deferred_gesture_events) {
    FilterGestureEventResult result =
        touch_action_filter_.FilterGestureEvent(&(it.event));
    SendGestureEventWithoutQueueing(it, result);
  }
}

void InputRouterImpl::SetTouchActionFromMain(cc::TouchAction touch_action) {
  TRACE_EVENT1("input", "InputRouterImpl::SetTouchActionFromMain",
               "touch_action", TouchActionToString(touch_action));
  touch_action_filter_.OnSetTouchAction(touch_action);
  touch_event_queue_.StopTimeoutMonitor();
  ProcessDeferredGestureEventQueue();
  UpdateTouchAckTimeoutEnabled();
}

void InputRouterImpl::SetPanAction(blink::mojom::PanAction pan_action) {
  if (pan_action_ == pan_action)
    return;
  pan_action_ = pan_action;

  // TODO(mahesh.ma): Update PanAction state to view, once RenderWidgetHostView
  // is set again.
  if (!client_->GetStylusInterface()) {
    return;
  }
  client_->GetStylusInterface()->NotifyHoverActionStylusWritable(
      pan_action_ == blink::mojom::PanAction::kStylusWritable);
}

void InputRouterImpl::OnSetCompositorAllowedTouchAction(
    cc::TouchAction touch_action) {
  TRACE_EVENT1("input", "InputRouterImpl::OnSetCompositorAllowedTouchAction",
               "action", cc::TouchActionToString(touch_action));
  touch_action_filter_.OnSetCompositorAllowedTouchAction(touch_action);
  client_->OnSetCompositorAllowedTouchAction(touch_action);
  if (touch_action == cc::TouchAction::kAuto)
    FlushDeferredGestureQueue();
  UpdateTouchAckTimeoutEnabled();
}

void InputRouterImpl::DidOverscroll(
    blink::mojom::DidOverscrollParamsPtr params) {
  // Touchpad and Touchscreen flings are handled on the browser side.
  ui::DidOverscrollParams fling_updated_params = {
      params->accumulated_overscroll, params->latest_overscroll_delta,
      params->current_fling_velocity, params->causal_event_viewport_point,
      params->overscroll_behavior};
  fling_updated_params.current_fling_velocity =
      gesture_event_queue_.CurrentFlingVelocity();
  client_->DidOverscroll(fling_updated_params);
}

void InputRouterImpl::DidStartScrollingViewport() {
  client_->DidStartScrollingViewport();
}

void InputRouterImpl::ImeCancelComposition() {
  client_->OnImeCancelComposition();
}

void InputRouterImpl::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::optional<std::vector<gfx::Rect>>& character_bounds,
    const std::optional<std::vector<gfx::Rect>>& line_bounds) {
  client_->OnImeCompositionRangeChanged(range, character_bounds, line_bounds);
}

void InputRouterImpl::SetMouseCapture(bool capture) {
  client_->SetMouseCapture(capture);
}

void InputRouterImpl::SetAutoscrollSelectionActiveInMainFrame(
    bool autoscroll_selection) {
  client_->SetAutoscrollSelectionActiveInMainFrame(autoscroll_selection);
}

void InputRouterImpl::RequestMouseLock(bool from_user_gesture,
                                       bool unadjusted_movement,
                                       RequestMouseLockCallback response) {
  client_->RequestMouseLock(from_user_gesture, unadjusted_movement,
                            std::move(response));
}

void InputRouterImpl::SetMovementXYForTouchPoints(blink::WebTouchEvent* event) {
  for (size_t i = 0; i < event->touches_length; ++i) {
    blink::WebTouchPoint* touch_point = &event->touches[i];
    if (touch_point->state == blink::WebTouchPoint::State::kStateMoved) {
      const gfx::Point& last_position = global_touch_position_[touch_point->id];
      touch_point->movement_x =
          touch_point->PositionInScreen().x() - last_position.x();
      touch_point->movement_y =
          touch_point->PositionInScreen().y() - last_position.y();
      global_touch_position_[touch_point->id].SetPoint(
          touch_point->PositionInScreen().x(),
          touch_point->PositionInScreen().y());
    } else {
      touch_point->movement_x = 0;
      touch_point->movement_y = 0;
      if (touch_point->state == blink::WebTouchPoint::State::kStateReleased ||
          touch_point->state == blink::WebTouchPoint::State::kStateCancelled) {
        global_touch_position_.erase(touch_point->id);
      } else if (touch_point->state ==
                 blink::WebTouchPoint::State::kStatePressed) {
        global_touch_position_[touch_point->id] =
            gfx::Point(touch_point->PositionInScreen().x(),
                       touch_point->PositionInScreen().y());
        // Note that the line above saves the latest position in case a
        // kStatePressed is encountered for a touch_point that is still active.
        // We consistently but infrequently encountered real-world systems
        // apparently sending two touch-points with a common id without sending
        // a kStateReleased or kStateCancelled in-between, see
        // https://crbug.com/1432337.
      }
    }
  }
}

// Forwards MouseEvent without passing it through
// TouchpadTapSuppressionController.
void InputRouterImpl::SendMouseEventImmediately(
    const MouseEventWithLatencyInfo& mouse_event,
    MouseEventCallback event_result_callback) {
  blink::mojom::WidgetInputHandler::DispatchEventCallback callback =
      base::BindOnce(&InputRouterImpl::MouseEventHandled, weak_this_,
                     mouse_event, std::move(event_result_callback));
  FilterAndSendWebInputEvent(mouse_event.event, mouse_event.latency,
                             std::move(callback));
}

void InputRouterImpl::SendTouchEventImmediately(
    const TouchEventWithLatencyInfo& touch_event) {
  blink::mojom::WidgetInputHandler::DispatchEventCallback callback =
      base::BindOnce(&InputRouterImpl::TouchEventHandled, weak_this_,
                     touch_event);
  FilterAndSendWebInputEvent(touch_event.event, touch_event.latency,
                             std::move(callback));
}

void InputRouterImpl::FlushDeferredGestureQueue() {
  touch_action_filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  ProcessDeferredGestureEventQueue();
}

void InputRouterImpl::OnTouchEventAck(
    const TouchEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  if (event.event.IsTouchSequenceStart()) {
    touch_action_filter_.IncreaseActiveTouches();
  }
  disposition_handler_->OnTouchEventAck(event, ack_source, ack_result);

  if (event.event.IsTouchSequenceEnd()) {
    touch_action_filter_.DecreaseActiveTouches();
    touch_action_filter_.ReportAndResetTouchAction();
    UpdateTouchAckTimeoutEnabled();
  }
}

void InputRouterImpl::OnFilteringTouchEvent(const WebTouchEvent& touch_event) {
  // The event stream given to the renderer is not guaranteed to be
  // valid based on the current TouchEventStreamValidator rules. This event will
  // never be given to the renderer, but in order to ensure that the event
  // stream |output_stream_validator_| sees is valid, we give events which are
  // filtered out to the validator. crbug.com/589111 proposes adding an
  // additional validator for the events which are actually sent to the
  // renderer.
  output_stream_validator_.Validate(touch_event);
}

void InputRouterImpl::SendGestureEventImmediately(
    const GestureEventWithLatencyInfo& gesture_event) {
  blink::mojom::WidgetInputHandler::DispatchEventCallback callback =
      base::BindOnce(&InputRouterImpl::GestureEventHandled, weak_this_,
                     gesture_event);
  FilterAndSendWebInputEvent(gesture_event.event, gesture_event.latency,
                             std::move(callback));
}

void InputRouterImpl::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  touch_event_queue_.OnGestureEventAck(event, ack_result);
  disposition_handler_->OnGestureEventAck(event, ack_source, ack_result);
}

void InputRouterImpl::SendGeneratedWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  client_->ForwardWheelEventWithLatencyInfo(wheel_event.event,
                                            wheel_event.latency);
}

void InputRouterImpl::SendGeneratedGestureScrollEvents(
    const GestureEventWithLatencyInfo& gesture_event) {
  client_->ForwardGestureEventWithLatencyInfo(gesture_event.event,
                                              gesture_event.latency);
}

gfx::Size InputRouterImpl::GetRootWidgetViewportSize() {
  return client_->GetRootWidgetViewportSize();
}

void InputRouterImpl::SendMouseWheelEventImmediately(
    const MouseWheelEventWithLatencyInfo& wheel_event,
    MouseWheelEventQueueClient::MouseWheelEventHandledCallback
        callee_callback) {
  blink::mojom::WidgetInputHandler::DispatchEventCallback callback =
      base::BindOnce(&InputRouterImpl::MouseWheelEventHandled, weak_this_,
                     wheel_event, std::move(callee_callback));
  FilterAndSendWebInputEvent(wheel_event.event, wheel_event.latency,
                             std::move(callback));
}

void InputRouterImpl::OnMouseWheelEventAck(
    const MouseWheelEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  disposition_handler_->OnWheelEventAck(event, ack_source, ack_result);
  gesture_event_queue_.OnWheelEventAck(event, ack_source, ack_result);
}

void InputRouterImpl::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency_info) {
  client_->ForwardGestureEventWithLatencyInfo(event, latency_info);
}

void InputRouterImpl::SendMouseWheelEventForPinchImmediately(
    const MouseWheelEventWithLatencyInfo& event,
    TouchpadPinchEventQueueClient::MouseWheelEventHandledCallback callback) {
  SendMouseWheelEventImmediately(event, std::move(callback));
}

void InputRouterImpl::OnGestureEventForPinchAck(
    const GestureEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  OnGestureEventAck(event, ack_source, ack_result);
}

bool InputRouterImpl::IsWheelScrollInProgress() {
  return client_->IsWheelScrollInProgress();
}

bool InputRouterImpl::IsAutoscrollInProgress() {
  return client_->IsAutoscrollInProgress();
}

void InputRouterImpl::FilterAndSendWebInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    blink::mojom::WidgetInputHandler::DispatchEventCallback callback) {
  TRACE_EVENT1("input", "InputRouterImpl::FilterAndSendWebInputEvent", "type",
               WebInputEvent::GetName(input_event.GetType()));

  output_stream_validator_.Validate(input_event);
  blink::mojom::InputEventResultState filtered_state =
      client_->FilterInputEvent(input_event, latency_info);
  if (WasHandled(filtered_state)) {
    TRACE_EVENT_INSTANT0("input", "InputEventFiltered",
                         TRACE_EVENT_SCOPE_THREAD);
    if (filtered_state != blink::mojom::InputEventResultState::kUnknown) {
      std::move(callback).Run(blink::mojom::InputEventResultSource::kBrowser,
                              latency_info, filtered_state, nullptr, nullptr);
    }
    return;
  }

  std::unique_ptr<blink::WebCoalescedInputEvent> event =
      ScaleEvent(input_event, device_scale_factor_, latency_info);
  if (WebInputEventTraits::ShouldBlockEventStream(input_event)) {
    TRACE_EVENT_INSTANT0("input", "InputEventSentBlocking",
                         TRACE_EVENT_SCOPE_THREAD);
    client_->IncrementInFlightEventCount();
    blink::mojom::WidgetInputHandler::DispatchEventCallback renderer_callback =
        base::BindOnce(
            [](blink::mojom::WidgetInputHandler::DispatchEventCallback callback,
               base::WeakPtr<InputRouterImpl> input_router,
               blink::mojom::InputEventResultSource source,
               const ui::LatencyInfo& latency,
               blink::mojom::InputEventResultState state,
               blink::mojom::DidOverscrollParamsPtr overscroll,
               blink::mojom::TouchActionOptionalPtr touch_action) {
              // Filter source to ensure only valid values are sent from the
              // renderer.
              if (source == blink::mojom::InputEventResultSource::kBrowser) {
                if (input_router)
                  input_router->client_->OnInvalidInputEventSource();
                return;
              }

              std::move(callback).Run(source, latency, state,
                                      std::move(overscroll),
                                      std::move(touch_action));
            },
            std::move(callback), weak_this_);
    client_->GetWidgetInputHandler()->DispatchEvent(
        std::move(event), std::move(renderer_callback));
  } else {
    TRACE_EVENT_INSTANT0("input", "InputEventSentNonBlocking",
                         TRACE_EVENT_SCOPE_THREAD);
    client_->GetWidgetInputHandler()->DispatchNonBlockingEvent(
        std::move(event));
    std::move(callback).Run(
        blink::mojom::InputEventResultSource::kBrowser, latency_info,
        blink::mojom::InputEventResultState::kIgnored, nullptr, nullptr);
  }
  // Ensure that the associated PendingTask for the WidgetInputHandler is
  // recorded when tasking long-running chrome tasks. This is needed to
  // selectively record input queueing and processing time.
  base::TaskAnnotator::MarkCurrentTaskAsInterestingForTracing();
}

void InputRouterImpl::KeyboardEventHandled(
    const NativeWebKeyboardEventWithLatencyInfo& event,
    KeyboardEventCallback event_result_callback,
    blink::mojom::InputEventResultSource source,
    const ui::LatencyInfo& latency,
    blink::mojom::InputEventResultState state,
    blink::mojom::DidOverscrollParamsPtr overscroll,
    blink::mojom::TouchActionOptionalPtr touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::KeyboardEventHandled", "type",
               WebInputEvent::GetName(event.event.GetType()), "ack",
               InputEventResultStateToString(state));

  if (source != blink::mojom::InputEventResultSource::kBrowser)
    client_->DecrementInFlightEventCount(source);
  event.latency.AddNewLatencyFrom(latency);
  std::move(event_result_callback).Run(event, source, state);

  // WARNING: This InputRouterImpl can be deallocated at this point
  // (i.e.  in the case of Ctrl+W, where the call to
  // HandleKeyboardEvent destroys this InputRouterImpl).
  // TODO(jdduke): crbug.com/274029 - Make ack-triggered shutdown async.
}

void InputRouterImpl::MouseEventHandled(
    const MouseEventWithLatencyInfo& event,
    MouseEventCallback event_result_callback,
    blink::mojom::InputEventResultSource source,
    const ui::LatencyInfo& latency,
    blink::mojom::InputEventResultState state,
    blink::mojom::DidOverscrollParamsPtr overscroll,
    blink::mojom::TouchActionOptionalPtr touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::MouseEventHandled", "type",
               WebInputEvent::GetName(event.event.GetType()), "ack",
               InputEventResultStateToString(state));

  if (source != blink::mojom::InputEventResultSource::kBrowser)
    client_->DecrementInFlightEventCount(source);
  event.latency.AddNewLatencyFrom(latency);
  std::move(event_result_callback).Run(event, source, state);
}

void InputRouterImpl::TouchEventHandled(
    const TouchEventWithLatencyInfo& touch_event,
    blink::mojom::InputEventResultSource source,
    const ui::LatencyInfo& latency,
    blink::mojom::InputEventResultState state,
    blink::mojom::DidOverscrollParamsPtr overscroll,
    blink::mojom::TouchActionOptionalPtr touch_action) {
  int64_t trace_id = latency.trace_id();
  TRACE_EVENT("input,benchmark,latencyInfo", "LatencyInfo.Flow",
              [&](perfetto::EventContext ctx) {
                ui::LatencyInfo::FillTraceEvent(
                    ctx, trace_id,
                    ChromeLatencyInfo2::Step::STEP_TOUCH_EVENT_HANDLED,
                    InputEventTypeToProto(touch_event.event.GetType()),
                    InputEventResultStateToProto(state));
              });

  if (source != blink::mojom::InputEventResultSource::kBrowser)
    client_->DecrementInFlightEventCount(source);
  touch_event.latency.AddNewLatencyFrom(latency);

  // The SetTouchAction IPC occurs on a different channel so always
  // send it in the input event ack to ensure it is available at the
  // time the ACK is handled.
  if (touch_action) {
    // For main thread ACKs, Blink will directly call SetTouchActionFromMain.
    if (source == blink::mojom::InputEventResultSource::kCompositorThread)
      OnSetCompositorAllowedTouchAction(touch_action->touch_action);
  }

  // TODO(crbug.com/40623448): find a proper way to stop the timeout monitor.
  bool should_stop_timeout_monitor = true;
  // |touch_event_queue_| will forward to OnTouchEventAck when appropriate.
  touch_event_queue_.ProcessTouchAck(source, state, latency,
                                     touch_event.event.unique_touch_event_id,
                                     should_stop_timeout_monitor);
}

void InputRouterImpl::GestureEventHandled(
    const GestureEventWithLatencyInfo& gesture_event,
    blink::mojom::InputEventResultSource source,
    const ui::LatencyInfo& latency,
    blink::mojom::InputEventResultState state,
    blink::mojom::DidOverscrollParamsPtr overscroll,
    blink::mojom::TouchActionOptionalPtr touch_action) {
  int64_t trace_id = latency.trace_id();
  TRACE_EVENT("input,benchmark,latencyInfo", "LatencyInfo.Flow",
              [&](perfetto::EventContext ctx) {
                ui::LatencyInfo::FillTraceEvent(
                    ctx, trace_id,
                    ChromeLatencyInfo2::Step::STEP_GESTURE_EVENT_HANDLED,
                    InputEventTypeToProto(gesture_event.event.GetType()),
                    InputEventResultStateToProto(state));
              });

  if (source != blink::mojom::InputEventResultSource::kBrowser)
    client_->DecrementInFlightEventCount(source);

  if (overscroll) {
    DCHECK_EQ(WebInputEvent::Type::kGestureScrollUpdate,
              gesture_event.event.GetType());
    DidOverscroll(std::move(overscroll));
  }

  // |gesture_event_queue_| will forward to OnGestureEventAck when appropriate.
  gesture_event_queue_.ProcessGestureAck(
      source, state, gesture_event.event.GetType(), latency);
}

void InputRouterImpl::MouseWheelEventHandled(
    const MouseWheelEventWithLatencyInfo& event,
    MouseWheelEventQueueClient::MouseWheelEventHandledCallback callback,
    blink::mojom::InputEventResultSource source,
    const ui::LatencyInfo& latency,
    blink::mojom::InputEventResultState state,
    blink::mojom::DidOverscrollParamsPtr overscroll,
    blink::mojom::TouchActionOptionalPtr touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::MouseWheelEventHandled", "type",
               WebInputEvent::GetName(event.event.GetType()), "ack",
               InputEventResultStateToString(state));
  if (source != blink::mojom::InputEventResultSource::kBrowser)
    client_->DecrementInFlightEventCount(source);
  event.latency.AddNewLatencyFrom(latency);

  if (overscroll)
    DidOverscroll(std::move(overscroll));

  std::move(callback).Run(event, source, state);
}

void InputRouterImpl::OnHasTouchEventConsumers(
    blink::mojom::TouchEventConsumersPtr consumers) {
  TRACE_EVENT1("input", "InputRouterImpl::OnHasTouchEventHandlers",
               "has_handlers", consumers->has_touch_event_handlers);

  touch_action_filter_.OnHasTouchEventHandlers(
      consumers->has_touch_event_handlers);
  touch_event_queue_.OnHasTouchEventHandlers(
      consumers->has_touch_event_handlers ||
      consumers->has_hit_testable_scrollbar);
}

void InputRouterImpl::WaitForInputProcessed(base::OnceClosure callback) {
  // TODO(bokan): Some kinds of input is queued in one of the various queues
  // available in this class. To be truly robust, we should wait until those
  // queues are flushed before issuing this message. This will be done in a
  // follow-up. https://crbug.com/902446.
  client_->GetWidgetInputHandler()->WaitForInputProcessed(std::move(callback));
}

void InputRouterImpl::ForceSetTouchActionAuto() {
  touch_action_filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  // TODO(xidachen): Call FlushDeferredGestureQueue when this flag is enabled.
  touch_event_queue_.StopTimeoutMonitor();
  ProcessDeferredGestureEventQueue();
}

void InputRouterImpl::ForceResetTouchActionForTest() {
  touch_action_filter_.ForceResetTouchActionForTest();
}

bool InputRouterImpl::IsFlingActiveForTest() {
  return gesture_event_queue_.IsFlingActiveForTest();
}

void InputRouterImpl::UpdateTouchAckTimeoutEnabled() {
  // TouchAction::kNone will prevent scrolling, in which case the timeout serves
  // little purpose. It's also a strong signal that touch handling is critical
  // to page functionality, so the timeout could do more harm than good.
  std::optional<cc::TouchAction> allowed_touch_action =
      touch_action_filter_.allowed_touch_action();
  cc::TouchAction compositor_allowed_touch_action =
      touch_action_filter_.compositor_allowed_touch_action();
  const bool touch_ack_timeout_disabled =
      (allowed_touch_action.has_value() &&
       allowed_touch_action.value() == cc::TouchAction::kNone) ||
      (compositor_allowed_touch_action == cc::TouchAction::kNone);
  touch_event_queue_.SetAckTimeoutEnabled(!touch_ack_timeout_disabled);
}

}  // namespace input
