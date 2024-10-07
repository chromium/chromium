// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/render_widget_host_input_event_router.h"

#include <algorithm>
#include <deque>
#include <memory>
#include <vector>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "components/input/cursor_manager.h"
#include "components/input/touch_emulator.h"
#include "components/viz/common/features.h"
#include "components/viz/common/hit_test/hit_test_data_provider.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/dip_util.h"

namespace {

// Transforms WebTouchEvent touch positions from the root view coordinate
// space to the target view coordinate space.
void TransformEventTouchPositions(blink::WebTouchEvent* event,
                                  const gfx::Transform& transform) {
  for (unsigned i = 0; i < event->touches_length; ++i) {
    event->touches[i].SetPositionInWidget(
        transform.MapPoint(event->touches[i].PositionInWidget()));
  }
}

bool IsMouseButtonDown(const blink::WebMouseEvent& event) {
  constexpr int mouse_button_modifiers =
      blink::WebInputEvent::kLeftButtonDown |
      blink::WebInputEvent::kMiddleButtonDown |
      blink::WebInputEvent::kRightButtonDown |
      blink::WebInputEvent::kBackButtonDown |
      blink::WebInputEvent::kForwardButtonDown;

  return event.GetModifiers() & mouse_button_modifiers;
}

}  // anonymous namespace

namespace input {

// Helper method also used from hit_test_debug_key_event_observer.cc
viz::HitTestQuery* GetHitTestQuery(viz::HitTestDataProvider* provider,
                                   const viz::FrameSinkId& frame_sink_id) {
  if (!frame_sink_id.is_valid())
    return nullptr;
  const auto& display_hit_test_query_map = provider->GetDisplayHitTestQuery();
  const auto iter = display_hit_test_query_map.find(frame_sink_id);
  if (iter == display_hit_test_query_map.end())
    return nullptr;
  return iter->second.get();
}

// A class to implement a queue for tracking outbound TouchEvents, and making
// sure that their acks are returned to the appropriate root view in order.
// This is important to ensure proper operation of the GestureProvider.
// Some challenges include:
// * differentiating between native and emulated TouchEvents, as the latter ack
//   to the TouchEmulator's GestureProvider,
// * making sure all events from destroyed renderers are acked properly, and
//   without delaying acks from other renderers, and
// * making sure events are only acked if the root_view (at the time of the
//   out-bound event) is still valid.
// Some of this logic, e.g. the last item above, is shared with
// RenderWidgetHostViewBase.
class TouchEventAckQueue {
 public:
  enum class TouchEventAckStatus { TouchEventNotAcked, TouchEventAcked };
  enum class TouchEventSource { SystemTouchEvent, EmulatedTouchEvent };
  struct AckData {
    TouchEventWithLatencyInfo touch_event;
    raw_ptr<RenderWidgetHostViewInput> target_view;
    raw_ptr<RenderWidgetHostViewInput> root_view;
    TouchEventSource touch_event_source;
    TouchEventAckStatus touch_event_ack_status;
    blink::mojom::InputEventResultState ack_result;
  };

  explicit TouchEventAckQueue(RenderWidgetHostInputEventRouter* client)
      : client_(client) {
    DCHECK(client_);
  }

  void Add(const TouchEventWithLatencyInfo& touch_event,
           RenderWidgetHostViewInput* target_view,
           RenderWidgetHostViewInput* root_view,
           TouchEventSource touch_event_source,
           TouchEventAckStatus touch_event_ack_status,
           blink::mojom::InputEventResultState ack_result);

  void Add(const TouchEventWithLatencyInfo& touch_event,
           RenderWidgetHostViewInput* target_view,
           RenderWidgetHostViewInput* root_view,
           TouchEventSource touch_event_source);

  void MarkAcked(const TouchEventWithLatencyInfo& touch_event,
                 blink::mojom::InputEventResultState ack_result,
                 RenderWidgetHostViewInput* target_view);

  void UpdateQueueAfterTargetDestroyed(RenderWidgetHostViewInput* target_view);

  size_t length_for_testing() { return ack_queue_.size(); }

 private:
  void ProcessAckedTouchEvents();

  std::deque<AckData> ack_queue_;
  raw_ptr<RenderWidgetHostInputEventRouter> client_;
};

void TouchEventAckQueue::Add(
    const TouchEventWithLatencyInfo& touch_event,
    RenderWidgetHostViewInput* target_view,
    RenderWidgetHostViewInput* root_view,
    TouchEventSource touch_event_source,
    TouchEventAckStatus touch_event_ack_status,
    blink::mojom::InputEventResultState ack_result) {
  AckData data = {touch_event,
                  target_view,
                  root_view,
                  touch_event_source,
                  touch_event_ack_status,
                  ack_result};
  ack_queue_.push_back(data);
  if (touch_event_ack_status == TouchEventAckStatus::TouchEventAcked)
    ProcessAckedTouchEvents();
}

void TouchEventAckQueue::Add(
    const TouchEventWithLatencyInfo& touch_event,
    RenderWidgetHostViewInput* target_view,
    RenderWidgetHostViewInput* root_view,
    TouchEventSource touch_event_source) {
  Add(touch_event, target_view, root_view, touch_event_source,
      TouchEventAckStatus::TouchEventNotAcked,
      blink::mojom::InputEventResultState::kUnknown);
}

void TouchEventAckQueue::MarkAcked(
    const TouchEventWithLatencyInfo& touch_event,
    blink::mojom::InputEventResultState ack_result,
    RenderWidgetHostViewInput* target_view) {
  auto it = find_if(ack_queue_.begin(), ack_queue_.end(),
                    [touch_event](AckData data) {
                      return data.touch_event.event.unique_touch_event_id ==
                             touch_event.event.unique_touch_event_id;
                    });
  if (it == ack_queue_.end()) {
    // If the touch-event was sent directly to the view without going through
    // RenderWidgetHostInputEventRouter, as is the case with AndroidWebView,
    // then we must ack it directly.
    if (target_view)
      target_view->GetRootView()->ProcessAckedTouchEvent(touch_event,
                                                         ack_result);
    return;
  }

  DCHECK(it->touch_event_ack_status != TouchEventAckStatus::TouchEventAcked);
  DCHECK(target_view && target_view == it->target_view);
  it->touch_event = touch_event;
  it->touch_event_ack_status = TouchEventAckStatus::TouchEventAcked;
  it->ack_result = ack_result;
  ProcessAckedTouchEvents();
}

void TouchEventAckQueue::ProcessAckedTouchEvents() {
  if (ack_queue_.empty())
    return;

  TouchEmulator* touch_emulator =
      client_->GetTouchEmulator(/*create_if_necessary=*/false);
  while (!ack_queue_.empty() && ack_queue_.front().touch_event_ack_status ==
                                    TouchEventAckStatus::TouchEventAcked) {
    TouchEventAckQueue::AckData ack_data = ack_queue_.front();
    ack_queue_.pop_front();

    if ((!touch_emulator ||
         !touch_emulator->HandleTouchEventAck(ack_data.touch_event.event,
                                              ack_data.ack_result)) &&
        (client_->IsViewInMap(ack_data.root_view) ||
         client_->ViewMapIsEmpty())) {
      // Forward acked event and result to the root view associated with the
      // event. The view map is only empty for AndroidWebView.
      ack_data.root_view->ProcessAckedTouchEvent(ack_data.touch_event,
                                                 ack_data.ack_result);
    }
  }
}

void TouchEventAckQueue::UpdateQueueAfterTargetDestroyed(
    RenderWidgetHostViewInput* target_view) {
  // If a queue entry's root view is being destroyed, just delete it.
  std::erase_if(ack_queue_, [target_view](const AckData& data) {
    return data.root_view == target_view;
  });

  // Otherwise, mark its status accordingly.
  for_each(ack_queue_.begin(), ack_queue_.end(), [target_view](AckData& data) {
    if (data.target_view == target_view) {
      data.touch_event_ack_status = TouchEventAckStatus::TouchEventAcked;
      data.ack_result = blink::mojom::InputEventResultState::kNoConsumerExists;
    }
  });

  ProcessAckedTouchEvents();
}

RenderWidgetHostInputEventRouter::TouchscreenPinchState::TouchscreenPinchState()
    : state_(PinchState::NONE) {}

bool RenderWidgetHostInputEventRouter::TouchscreenPinchState::IsInPinch()
    const {
  switch (state_) {
    case PinchState::NONE:
    case PinchState::EXISTING_BUBBLING_TO_ROOT:
      return false;
    case PinchState::PINCH_WITH_ROOT_GESTURE_TARGET:
    case PinchState::PINCH_WHILE_BUBBLING_TO_ROOT:
    case PinchState::PINCH_DURING_CHILD_GESTURE:
      return true;
  }
}

bool RenderWidgetHostInputEventRouter::TouchscreenPinchState::
    NeedsWrappingScrollSequence() const {
  switch (state_) {
    case PinchState::NONE:
    case PinchState::PINCH_DURING_CHILD_GESTURE:
      return true;
    case PinchState::EXISTING_BUBBLING_TO_ROOT:
    case PinchState::PINCH_WITH_ROOT_GESTURE_TARGET:
    case PinchState::PINCH_WHILE_BUBBLING_TO_ROOT:
      return false;
  }
}

void RenderWidgetHostInputEventRouter::TouchscreenPinchState::
    DidStartBubblingToRoot() {
  // TODO(350383805): Cleanup this and other related trace events after
  // investigating crash bug.
  TRACE_EVENT("input", "TouchscreenPinchState::DidStartBubblingToRoot");
  switch (state_) {
    case PinchState::NONE:
      state_ = PinchState::EXISTING_BUBBLING_TO_ROOT;
      break;
    case PinchState::PINCH_DURING_CHILD_GESTURE:
      state_ = PinchState::PINCH_WHILE_BUBBLING_TO_ROOT;
      break;
    case PinchState::EXISTING_BUBBLING_TO_ROOT:
    case PinchState::PINCH_WITH_ROOT_GESTURE_TARGET:
    case PinchState::PINCH_WHILE_BUBBLING_TO_ROOT:
      DUMP_WILL_BE_NOTREACHED();
  }
}

void RenderWidgetHostInputEventRouter::TouchscreenPinchState::
    DidStopBubblingToRoot() {
  TRACE_EVENT("input", "TouchscreenPinchState::DidStopBubblingToRoot");
  DCHECK_EQ(PinchState::EXISTING_BUBBLING_TO_ROOT, state_);
  state_ = PinchState::NONE;
}

void RenderWidgetHostInputEventRouter::TouchscreenPinchState::
    DidStartPinchInRoot() {
  TRACE_EVENT("input", "TouchscreenPinchState::DidStartPinchInRoot");
  DCHECK_EQ(PinchState::NONE, state_);
  state_ = PinchState::PINCH_WITH_ROOT_GESTURE_TARGET;
}

void RenderWidgetHostInputEventRouter::TouchscreenPinchState::
    DidStartPinchInChild() {
  TRACE_EVENT("input", "TouchscreenPinchState::DidStartPinchInChild");
  switch (state_) {
    case PinchState::NONE:
      state_ = PinchState::PINCH_DURING_CHILD_GESTURE;
      break;
    case PinchState::EXISTING_BUBBLING_TO_ROOT:
      state_ = PinchState::PINCH_WHILE_BUBBLING_TO_ROOT;
      break;
    case PinchState::PINCH_WITH_ROOT_GESTURE_TARGET:
    case PinchState::PINCH_WHILE_BUBBLING_TO_ROOT:
    case PinchState::PINCH_DURING_CHILD_GESTURE:
      TRACE_EVENT_INSTANT(
          "input", "DUMP_WILL_BE_NOTREACHED called from DidStartPinchInChild");
      DUMP_WILL_BE_NOTREACHED();
  }
}

void RenderWidgetHostInputEventRouter::TouchscreenPinchState::DidStopPinch() {
  TRACE_EVENT("input", "TouchscreenPinchState::DidStopPinch");
  switch (state_) {
    case PinchState::PINCH_WITH_ROOT_GESTURE_TARGET:
      state_ = PinchState::NONE;
      break;
    case PinchState::PINCH_WHILE_BUBBLING_TO_ROOT:
      state_ = PinchState::EXISTING_BUBBLING_TO_ROOT;
      break;
    case PinchState::PINCH_DURING_CHILD_GESTURE:
      state_ = PinchState::NONE;
      break;
    case PinchState::NONE:
    case PinchState::EXISTING_BUBBLING_TO_ROOT:
      NOTREACHED_IN_MIGRATION();
  }
}

bool RenderWidgetHostInputEventRouter::HasEventsPendingDispatch() const {
  return event_targeter_->HasEventsPendingDispatch();
}

size_t RenderWidgetHostInputEventRouter::TouchEventAckQueueLengthForTesting()
    const {
  return touch_event_ack_queue_->length_for_testing();
}

size_t RenderWidgetHostInputEventRouter::RegisteredViewCountForTesting() const {
  return owner_map_.size();
}

RenderWidgetHostViewInput*
RenderWidgetHostInputEventRouter::GetLastMouseMoveTargetForTest() {
  return last_mouse_move_target_.get();
}

RenderWidgetHostViewInput*
RenderWidgetHostInputEventRouter::GetLastMouseMoveRootViewForTest() {
  return last_mouse_move_root_view_.get();
}

void RenderWidgetHostInputEventRouter::OnRenderWidgetHostViewInputDestroyed(
    RenderWidgetHostViewInput* view) {
  // RenderWidgetHostViewInput::RemoveObserver() should only ever be called
  // in this function, except during the shutdown of this class. This prevents
  // removal of an observed view that is being tracked as an event target
  // without cleaning up dangling pointers to it.
  view->RemoveObserver(this);

  // Remove this view from the owner_map.
  for (auto entry : owner_map_) {
    if (entry.second.get() == view) {
      owner_map_.erase(entry.first);
      // There will only be one instance of a particular view in the map.
      break;
    }
  }

  auto* touch_emulator = GetTouchEmulator(/*create_if_necessary=*/false);
  if (touch_emulator) {
    touch_emulator->OnViewDestroyed(view);
  }

  if (view == touch_target_) {
    touch_target_ = nullptr;
    active_touches_ = 0;
  }
  touch_event_ack_queue_->UpdateQueueAfterTargetDestroyed(view);

  if (view == wheel_target_)
    wheel_target_ = nullptr;

  // If the target that's being destroyed is in the gesture target map, we
  // replace it with nullptr so that we maintain the 1:1 correspondence between
  // map entries and the touch sequences that underly them.
  for (auto it : touchscreen_gesture_target_map_) {
    if (it.second.get() == view)
      it.second = nullptr;
  }

  if (view == mouse_capture_target_)
    mouse_capture_target_ = nullptr;

  if (view == touchscreen_gesture_target_.get()) {
    ClearTouchscreenGestureTarget();
  }

  if (view == touchpad_gesture_target_)
    touchpad_gesture_target_ = nullptr;

  if (view == bubbling_gesture_scroll_target_) {
    bubbling_gesture_scroll_target_ = nullptr;
    bubbling_gesture_scroll_origin_ = nullptr;
  } else if (view == bubbling_gesture_scroll_origin_) {
    bubbling_gesture_scroll_origin_ = nullptr;
  }

  if (view == last_mouse_move_root_view_) {
    last_mouse_move_target_ = nullptr;
    last_mouse_move_root_view_ = nullptr;
  }

  if (view == last_mouse_move_target_) {
    // When a child iframe is destroyed, consider its parent to be to be the
    // most recent target, if possible. In some cases the parent might already
    // have been destroyed, in which case the last target is cleared.
    if (view != last_mouse_move_root_view_) {
      last_mouse_move_target_ = last_mouse_move_target_->GetParentViewInput();
    } else {
      last_mouse_move_target_ = nullptr;
    }

    // If both target and root are the view being destroyed, or the parent
    // has already been destroyed, then also clear the root view pointer
    // along with the target pointer.
    if (!last_mouse_move_target_)
      last_mouse_move_root_view_ = nullptr;
  }

  if (view == last_fling_start_target_)
    last_fling_start_target_ = nullptr;

  if (view == last_mouse_down_target_)
    last_mouse_down_target_ = nullptr;

  if (view == last_emulated_event_root_view_)
    last_emulated_event_root_view_ = nullptr;

  event_targeter_->ViewWillBeDestroyed(view);
}

void RenderWidgetHostInputEventRouter::ClearAllObserverRegistrations() {
  // Since we're shutting down, it's safe to call RenderWidgetHostViewInput::
  // RemoveObserver() directly here.
  for (auto entry : owner_map_) {
    if (entry.second)
      entry.second->RemoveObserver(this);
  }
  owner_map_.clear();
  hit_test_provider_->RemoveHitTestRegionObserver(this);
}

RenderWidgetHostInputEventRouter::RenderWidgetHostInputEventRouter(
    viz::HitTestDataProvider* provider,
    Delegate* delegate)
    : last_mouse_move_target_(nullptr),
      last_mouse_move_root_view_(nullptr),
      last_emulated_event_root_view_(nullptr),
      last_device_scale_factor_(1.f),
      active_touches_(0),
      hit_test_provider_(provider),
      event_targeter_(std::make_unique<RenderWidgetTargeter>(this)),
      delegate_(delegate),
      touch_event_ack_queue_(new TouchEventAckQueue(this)) {
  DCHECK(hit_test_provider_);
  CHECK(delegate_);
  hit_test_provider_->AddHitTestRegionObserver(this);
}

RenderWidgetHostInputEventRouter::~RenderWidgetHostInputEventRouter() {
  // We may be destroyed before some of the owners in the map, so we must
  // remove ourself from their observer lists.
  ClearAllObserverRegistrations();
}

RenderWidgetTargetResult RenderWidgetHostInputEventRouter::FindMouseEventTarget(
    RenderWidgetHostViewInput* root_view,
    const blink::WebMouseEvent& event) const {
  RenderWidgetHostViewInput* target = nullptr;
  bool needs_transform_point = true;
  bool latched_target = true;
  // Allow devtools to route events into the root view based on the
  // browser-side inspector overlay state.
  if (route_to_root_for_devtools_)
    target = root_view;

  if (!target && root_view->IsPointerLocked()) {
    target =
        root_view->GetViewRenderInputRouter()->delegate()->GetPointerLockView();
  }

  gfx::PointF transformed_point;

  // Ignore mouse_capture_target_ if there are no mouse buttons currently down
  // because this is only for the purpose of dragging.
  if (!target && mouse_capture_target_ &&
      (event.GetType() == blink::WebInputEvent::Type::kMouseUp ||
       IsMouseButtonDown(event))) {
    target = mouse_capture_target_;
    // Hit testing is skipped for MouseUp with mouse capture which is enabled by
    // the OOPIF renderer. Instead of using the coordinate transformation in the
    // browser process process, use the cached coordinates that were determined
    // by the renderer process on the previous MouseDown.
    // TODO(crbug.com/41473630): Currently there is a mismatch between the
    // coordinate transforms from browser process and renderer process. We need
    // to fix it so that we don't need to cache the transform from MouseDown.
    if (event.GetType() == blink::WebInputEvent::Type::kMouseUp &&
        target == last_mouse_down_target_ &&
        mouse_down_pre_transformed_coordinate_ == event.PositionInWidget()) {
      transformed_point = mouse_down_post_transformed_coordinate_;
      needs_transform_point = false;
    }
  }

  if (!target) {
    latched_target = false;
    auto result =
        FindViewAtLocation(root_view, event.PositionInWidget(),
                           viz::EventSource::MOUSE, &transformed_point);
    if (event.GetType() == blink::WebInputEvent::Type::kMouseDown) {
      mouse_down_pre_transformed_coordinate_ = event.PositionInWidget();
    }
    if (result.should_query_view)
      return {result.view, true, transformed_point, latched_target};

    target = result.view;
    // |transformed_point| is already transformed.
    needs_transform_point = false;
  }

  if (needs_transform_point) {
    if (!root_view->TransformPointToCoordSpaceForView(
            event.PositionInWidget(), target, &transformed_point)) {
      return {nullptr, false, std::nullopt, latched_target};
    }
  }
  return {target, false, transformed_point, latched_target};
}

RenderWidgetTargetResult
RenderWidgetHostInputEventRouter::FindMouseWheelEventTarget(
    RenderWidgetHostViewInput* root_view,
    const blink::WebMouseWheelEvent& event) const {
  RenderWidgetHostViewInput* target = nullptr;
  gfx::PointF transformed_point;
  if (root_view->IsPointerLocked()) {
    target =
        root_view->GetViewRenderInputRouter()->delegate()->GetPointerLockView();
    if (!root_view->TransformPointToCoordSpaceForView(
            event.PositionInWidget(), target, &transformed_point)) {
      return {nullptr, false, std::nullopt, true};
    }
    return {target, false, transformed_point, true};
  }

  if (event.phase == blink::WebMouseWheelEvent::kPhaseBegan) {
    auto result =
        FindViewAtLocation(root_view, event.PositionInWidget(),
                           viz::EventSource::MOUSE, &transformed_point);
    return {result.view, result.should_query_view, transformed_point, false};
  }
  // For non-begin events, the target found for the previous phaseBegan is
  // used.
  return {nullptr, false, std::nullopt, true};
}

RenderWidgetTargetResult RenderWidgetHostInputEventRouter::FindViewAtLocation(
    RenderWidgetHostViewInput* root_view,
    const gfx::PointF& point,
    viz::EventSource source,
    gfx::PointF* transformed_point) const {
  // Short circuit if owner_map has only one RenderWidgetHostView, no need for
  // hit testing.
  if (owner_map_.size() <= 1) {
    *transformed_point = point;
    return {root_view, false, *transformed_point, false};
  }

  viz::FrameSinkId frame_sink_id;
  bool query_renderer = false;
  viz::HitTestQuery* query =
      GetHitTestQuery(hit_test_provider_, root_view->GetRootFrameSinkId());
  if (!query) {
    *transformed_point = point;
    return {root_view, false, *transformed_point, false};
  }
  float device_scale_factor = root_view->GetDeviceScaleFactor();
  DCHECK_GT(device_scale_factor, 0.0f);
  gfx::PointF point_in_pixels =
      gfx::ConvertPointToPixels(point, device_scale_factor);
  viz::Target target = query->FindTargetForLocationStartingFrom(
      source, point_in_pixels, root_view->GetFrameSinkId());
  frame_sink_id = target.frame_sink_id;
  if (frame_sink_id.is_valid()) {
    *transformed_point =
        gfx::ConvertPointToDips(target.location_in_target, device_scale_factor);
  } else {
    *transformed_point = point;
  }
  // To ensure the correctness of viz hit testing with cc generated data, we
  // verify hit test results when:
  // a) We use cc generated data to do synchronous hit testing and
  // b) We use HitTestQuery to find the target (instead of reusing previous
  // targets when hit testing latched events) and
  // c) We are not hit testing MouseMove events which is too frequent to
  // verify it without impacting performance.
  // The code that implements c) locates in |FindMouseEventTarget|.
  if (target.flags & viz::HitTestRegionFlags::kHitTestAsk)
    query_renderer = true;

  auto* view = FindViewFromFrameSinkId(frame_sink_id);
  // Send the event to |root_view| if |view| is not in |root_view|'s sub-tree
  // anymore.
  if (!view) {
    view = root_view;
    *transformed_point = point;
  }

  return {view, query_renderer, *transformed_point, false};
}

void RenderWidgetHostInputEventRouter::RouteMouseEvent(
    RenderWidgetHostViewInput* root_view,
    const blink::WebMouseEvent* event,
    const ui::LatencyInfo& latency) {
  event_targeter_->FindTargetAndDispatch(root_view, *event, latency);
}

void RenderWidgetHostInputEventRouter::DispatchMouseEvent(
    RenderWidgetHostViewInput* root_view,
    RenderWidgetHostViewInput* target,
    const blink::WebMouseEvent& mouse_event,
    const ui::LatencyInfo& latency,
    const std::optional<gfx::PointF>& target_location) {
  // TODO(wjmaclean): Should we be sending a no-consumer ack to the root_view
  // if there is no target?
  if (!target)
    return;

  // Implicitly release any capture when a MouseUp arrives, so that if any
  // events arrive before the renderer can explicitly release capture, we can
  // target those correctly. This also releases if there are no mouse buttons
  // down, which is to protect against problems that can occur on some
  // platforms where MouseUps are not received when the mouse cursor is off the
  // browser window.
  // Also, this is strictly necessary for touch emulation.
  if (mouse_capture_target_ &&
      (mouse_event.GetType() == blink::WebInputEvent::Type::kMouseUp ||
       !IsMouseButtonDown(mouse_event))) {
    mouse_capture_target_ = nullptr;

    // Since capture is being lost it is possible that MouseMoves over a hit
    // test region might have been going to a different region, and now the
    // CursorManager might need to be notified that the view underneath the
    // cursor has changed, which could cause the display cursor to update.
    gfx::PointF transformed_point;
    auto hit_test_result =
        FindViewAtLocation(root_view, mouse_event.PositionInWidget(),
                           viz::EventSource::MOUSE, &transformed_point);
    // TODO(crbug.com/41419447): This is skipped if the HitTestResult is
    // requiring an asynchronous hit test to the renderer process, because it
    // might mean sending extra MouseMoves to renderers that don't need the
    // event updates which is a worse outcome than the cursor being delayed in
    // updating. An asynchronous hit test can be added here to fix the problem.
    if (hit_test_result.view != target && !hit_test_result.should_query_view) {
      SendMouseEnterOrLeaveEvents(
          mouse_event, hit_test_result.view, root_view,
          blink::WebInputEvent::Modifiers::kRelativeMotionEvent, true);
      if (root_view->GetCursorManager())
        root_view->GetCursorManager()->UpdateViewUnderCursor(
            hit_test_result.view);
    }
  }

  // When touch emulation is active, mouse events have to act like touch
  // events, which requires that there be implicit capture between MouseDown
  // and MouseUp.

  auto* touch_emulator = GetTouchEmulator(/*create_if_necessary=*/false);
  if (mouse_event.GetType() == blink::WebInputEvent::Type::kMouseDown &&
      touch_emulator && touch_emulator->IsEnabled()) {
    mouse_capture_target_ = target;
  }

  if (target) {
    ui::EventType type = mouse_event.GetTypeAsUiEventType();
    bool hovering = (type != ui::EventType::kMouseDragged) &&
                    (type != ui::EventType::kMousePressed);
    ForwardDelegatedInkPoint(target, root_view, mouse_event, mouse_event,
                             hovering);
  }

  DCHECK(target_location.has_value());
  blink::WebMouseEvent event = mouse_event;
  event.SetPositionInWidget(target_location->x(), target_location->y());

  // SendMouseEnterOrLeaveEvents is called with the original event
  // coordinates, which are transformed independently for each view that will
  // receive an event. Also, since the view under the mouse has changed,
  // notify the CursorManager that it might need to change the cursor.
  if ((event.GetType() == blink::WebInputEvent::Type::kMouseLeave ||
       event.GetType() == blink::WebInputEvent::Type::kMouseMove) &&
      target != last_mouse_move_target_ && !root_view->IsPointerLocked()) {
    SendMouseEnterOrLeaveEvents(mouse_event, target, root_view);
    if (root_view->GetCursorManager())
      root_view->GetCursorManager()->UpdateViewUnderCursor(target);
  }

  target->ProcessMouseEvent(event, latency);

  if (root_view_receive_additional_mouse_up_ && target != root_view &&
      mouse_event.GetType() == blink::WebInputEvent::Type::kMouseUp) {
    root_view->ProcessMouseEvent(event, latency);
  }
}

void RenderWidgetHostInputEventRouter::RouteMouseWheelEvent(
    RenderWidgetHostViewInput* root_view,
    blink::WebMouseWheelEvent* event,
    const ui::LatencyInfo& latency) {
  event_targeter_->FindTargetAndDispatch(root_view, *event, latency);
}

// TODO(crbug.com/346629231): Temporary for debugging.
static const char* PhaseToString(blink::WebMouseWheelEvent::Phase phase) {
  switch (phase) {
    case blink::WebMouseWheelEvent::kPhaseNone:
      return "PhaseNone";
    case blink::WebMouseWheelEvent::kPhaseBegan:
      return "PhaseBegan";
    case blink::WebMouseWheelEvent::kPhaseStationary:
      return "PhaseStationary";
    case blink::WebMouseWheelEvent::kPhaseChanged:
      return "PhaseChanged";
    case blink::WebMouseWheelEvent::kPhaseEnded:
      return "PhaseEnded";
    case blink::WebMouseWheelEvent::kPhaseCancelled:
      return "PhaseCancelled";
    case blink::WebMouseWheelEvent::kPhaseMayBegin:
      return "PhaseMayBegin";
    case blink::WebMouseWheelEvent::kPhaseBlocked:
      return "PhaseMayBlocked";
  }
}

void RenderWidgetHostInputEventRouter::DispatchMouseWheelEvent(
    RenderWidgetHostViewInput* root_view,
    RenderWidgetHostViewInput* target,
    const blink::WebMouseWheelEvent& mouse_wheel_event,
    const ui::LatencyInfo& latency,
    const std::optional<gfx::PointF>& target_location) {
  TRACE_EVENT("input",
              "RenderWidgetHostInputEventRouter::DispatchMouseWheelEvent",
              "phase", PhaseToString(mouse_wheel_event.phase), "target",
              static_cast<void*>(target), "wheel_target_",
              static_cast<void*>(wheel_target_));
  if (!root_view->IsPointerLocked()) {
    if (mouse_wheel_event.phase == blink::WebMouseWheelEvent::kPhaseBegan) {
      wheel_target_ = target;
    } else {
      if (wheel_target_) {
        // If middle click autoscroll is in progress, browser routes all input
        // events to single renderer. So if autoscroll is in progress, route
        // mouse wheel events to the |target| instead of |wheel_target_|.
        DCHECK(!target || event_targeter_->is_auto_scroll_in_progress());
        if (!event_targeter_->is_auto_scroll_in_progress())
          target = wheel_target_;
      } else if ((mouse_wheel_event.phase ==
                      blink::WebMouseWheelEvent::kPhaseEnded ||
                  mouse_wheel_event.momentum_phase ==
                      blink::WebMouseWheelEvent::kPhaseEnded) &&
                 bubbling_gesture_scroll_target_) {
        // Send a GSE to the bubbling target and cancel scroll bubbling since
        // the wheel target view is destroyed and the wheel end event won't get
        // processed.
        CancelScrollBubbling();
      }
    }
  }

  if (!target) {
    root_view->WheelEventAck(
        mouse_wheel_event,
        blink::mojom::InputEventResultState::kNoConsumerExists);
    return;
  }

  blink::WebMouseWheelEvent event = mouse_wheel_event;
  gfx::PointF point_in_target;
  if (target_location) {
    point_in_target = target_location.value();
  } else {
    point_in_target = target->TransformRootPointToViewCoordSpace(
        mouse_wheel_event.PositionInWidget());
  }
  event.SetPositionInWidget(point_in_target.x(), point_in_target.y());
  target->ProcessMouseWheelEvent(event, latency);

  if (mouse_wheel_event.phase == blink::WebMouseWheelEvent::kPhaseEnded ||
      mouse_wheel_event.momentum_phase ==
          blink::WebMouseWheelEvent::kPhaseEnded) {
    wheel_target_ = nullptr;
  }
}

void RenderWidgetHostInputEventRouter::RouteGestureEvent(
    RenderWidgetHostViewInput* root_view,
    const blink::WebGestureEvent* event,
    const ui::LatencyInfo& latency) {
  if (event->IsTargetViewport()) {
    root_view->ProcessGestureEvent(*event, latency);
    return;
  }

  switch (event->SourceDevice()) {
    case blink::WebGestureDevice::kUninitialized:
      NOTREACHED_IN_MIGRATION() << "Uninitialized device type is not allowed";
      break;
    case blink::WebGestureDevice::kSyntheticAutoscroll:
      NOTREACHED_IN_MIGRATION()
          << "Only target_viewport synthetic autoscrolls are "
             "currently supported";
      break;
    case blink::WebGestureDevice::kTouchpad:
      RouteTouchpadGestureEvent(root_view, event, latency);
      break;
    case blink::WebGestureDevice::kTouchscreen:
      RouteTouchscreenGestureEvent(root_view, event, latency);
      break;
    case blink::WebGestureDevice::kScrollbar:
      NOTREACHED_IN_MIGRATION()
          << "This gesture source is only ever generated inside the renderer "
             "and is designated for compositor threaded scrollbar scrolling. "
             "We should never see it in the browser.";
      break;
  };
}

namespace {

unsigned CountChangedTouchPoints(const blink::WebTouchEvent& event) {
  unsigned changed_count = 0;

  blink::WebTouchPoint::State required_state =
      blink::WebTouchPoint::State::kStateUndefined;
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kTouchStart:
      required_state = blink::WebTouchPoint::State::kStatePressed;
      break;
    case blink::WebInputEvent::Type::kTouchEnd:
      required_state = blink::WebTouchPoint::State::kStateReleased;
      break;
    case blink::WebInputEvent::Type::kTouchCancel:
      required_state = blink::WebTouchPoint::State::kStateCancelled;
      break;
    default:
      // We'll only ever call this method for TouchStart, TouchEnd
      // and TounchCancel events, so mark the rest as not-reached.
      NOTREACHED_IN_MIGRATION();
  }
  for (unsigned i = 0; i < event.touches_length; ++i) {
    if (event.touches[i].state == required_state)
      ++changed_count;
  }

  DCHECK(event.GetType() == blink::WebInputEvent::Type::kTouchCancel ||
         changed_count == 1);
  return changed_count;
}

}  // namespace

// Any time a touch start event is handled/consumed/default prevented it is
// removed from the gesture map, because it will never create a gesture.
void RenderWidgetHostInputEventRouter::OnHandledTouchStartOrFirstTouchMove(
    uint32_t unique_touch_event_id) {
  // unique_touch_event_id of 0 implies a gesture not created by a touch.
  DCHECK_NE(unique_touch_event_id, 0U);
  touchscreen_gesture_target_map_.erase(unique_touch_event_id);
}

RenderWidgetTargetResult RenderWidgetHostInputEventRouter::FindTouchEventTarget(
    RenderWidgetHostViewInput* root_view,
    const blink::WebTouchEvent& event) {
  // Tests may call this without an initial TouchStart, so check event type
  // explicitly here.
  if (active_touches_ ||
      event.GetType() != blink::WebInputEvent::Type::kTouchStart)
    return {nullptr, false, std::nullopt, true};

  active_touches_ += CountChangedTouchPoints(event);
  gfx::PointF original_point = gfx::PointF(event.touches[0].PositionInWidget());
  gfx::PointF transformed_point;

  return FindViewAtLocation(root_view, original_point, viz::EventSource::TOUCH,
                            &transformed_point);
}

void RenderWidgetHostInputEventRouter::DispatchTouchEvent(
    RenderWidgetHostViewInput* root_view,
    RenderWidgetHostViewInput* target,
    const blink::WebTouchEvent& touch_event,
    const ui::LatencyInfo& latency,
    const std::optional<gfx::PointF>& target_location,
    bool is_emulated_touchevent) {
  DCHECK(blink::WebInputEvent::IsTouchEventType(touch_event.GetType()) &&
         touch_event.GetType() !=
             blink::WebInputEvent::Type::kTouchScrollStarted);

  bool is_sequence_start = !touch_target_ && target;
  if (is_sequence_start) {
    touch_target_ = target;
    DCHECK(touchscreen_gesture_target_map_.find(
               touch_event.unique_touch_event_id) ==
           touchscreen_gesture_target_map_.end());
    touchscreen_gesture_target_map_[touch_event.unique_touch_event_id] =
        touch_target_->GetInputWeakPtr();
  } else if (touch_event.GetType() == blink::WebInputEvent::Type::kTouchStart) {
    active_touches_ += CountChangedTouchPoints(touch_event);
  }

  // Test active_touches_ before decrementing, since its value can be
  // reset to 0 in OnRenderWidgetHostViewInputDestroyed, and this can
  // happen between the TouchStart and a subsequent TouchMove/End/Cancel.
  if ((touch_event.GetType() == blink::WebInputEvent::Type::kTouchEnd ||
       touch_event.GetType() == blink::WebInputEvent::Type::kTouchCancel) &&
      active_touches_) {
    active_touches_ -= CountChangedTouchPoints(touch_event);
  }
  DCHECK_GE(active_touches_, 0);

  // Debugging for crbug.com/814674.
  if (touch_target_ && !IsViewInMap(touch_target_)) {
    NOTREACHED_IN_MIGRATION()
        << "Touch events should not be routed to a destroyed target "
           "View.";
    touch_target_ = nullptr;
    base::debug::DumpWithoutCrashing();
  }

  if (touch_target_) {
    ForwardDelegatedInkPoint(touch_target_, root_view, touch_event,
                             touch_event.touches[0], touch_event.hovering);
  }

  TouchEventAckQueue::TouchEventSource event_source =
      is_emulated_touchevent
          ? TouchEventAckQueue::TouchEventSource::EmulatedTouchEvent
          : TouchEventAckQueue::TouchEventSource::SystemTouchEvent;
  if (!touch_target_) {
    touch_event_ack_queue_->Add(
        TouchEventWithLatencyInfo(touch_event), nullptr, root_view,
        event_source, TouchEventAckQueue::TouchEventAckStatus::TouchEventAcked,
        blink::mojom::InputEventResultState::kNoConsumerExists);
    return;
  }

  gfx::Transform transform;
  if (!root_view->GetTransformToViewCoordSpace(touch_target_, &transform)) {
    // Fall-back to just using the delta if we are unable to get the full
    // transform.
    transform.MakeIdentity();
    if (target_location.has_value()) {
      transform.Translate(target_location.value() -
                          touch_event.touches[0].PositionInWidget());
    } else {
      // GetTransformToViewCoordSpace() fails when viz_hit_test is off but
      // TransformRootPointToViewCoordSpace() still works at this case.
      // TODO(crbug.com/41432837) remove the extra code when viz_hit_test is
      // always on.
      gfx::PointF point_in_target =
          touch_target_->TransformRootPointToViewCoordSpace(
              touch_event.touches[0].PositionInWidget());
      transform.Translate(point_in_target -
                          touch_event.touches[0].PositionInWidget());
    }
  }

  if (is_sequence_start) {
    CancelScrollBubblingIfConflicting(touch_target_);
  }

  touch_event_ack_queue_->Add(TouchEventWithLatencyInfo(touch_event),
                              touch_target_, root_view, event_source);

  blink::WebTouchEvent event(touch_event);
  TransformEventTouchPositions(&event, transform);
  touch_target_->ProcessTouchEvent(event, latency);

  if (!active_touches_)
    touch_target_ = nullptr;
}

void RenderWidgetHostInputEventRouter::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& event,
    blink::mojom::InputEventResultState ack_result,
    RenderWidgetHostViewInput* view) {
  TRACE_EVENT("input",
              "RenderWidgetHostInputEventRouter::ProcessAckedTouchEvent");
  touch_event_ack_queue_->MarkAcked(event, ack_result, view);
}

void RenderWidgetHostInputEventRouter::RouteTouchEvent(
    RenderWidgetHostViewInput* root_view,
    blink::WebTouchEvent* event,
    const ui::LatencyInfo& latency) {
  event_targeter_->FindTargetAndDispatch(root_view, *event, latency);
}

void RenderWidgetHostInputEventRouter::SendMouseEnterOrLeaveEvents(
    const blink::WebMouseEvent& event,
    RenderWidgetHostViewInput* target,
    RenderWidgetHostViewInput* root_view,
    blink::WebInputEvent::Modifiers extra_modifiers,
    bool include_target_view) {
  // This method treats RenderWidgetHostViews as a tree, where the mouse
  // cursor is potentially leaving one node and entering another somewhere
  // else in the tree. Since iframes are graphically self-contained (i.e. an
  // iframe can't have a descendant that renders outside of its rect
  // boundaries), all affected RenderWidgetHostViews are ancestors of either
  // the node being exited or the node being entered.
  // Approach:
  // 1. Find lowest common ancestor (LCA) of the last view and current target
  //    view.
  // 2. The last view, and its ancestors up to but not including the LCA,
  //    receive a MouseLeave.
  // 3. The LCA itself, unless it is the new target, receives a MouseOut
  //    because the cursor has passed between elements within its bounds.
  // 4. The new target view's ancestors, up to but not including the LCA,
  //    receive a MouseEnter.
  // Ordering does not matter since these are handled asynchronously relative
  // to each other.

  // If the mouse has moved onto a different root view (typically meaning it
  // has crossed over a popup or context menu boundary), then we invalidate
  // last_mouse_move_target_ because we have no reference for its coordinate
  // space.
  if (root_view != last_mouse_move_root_view_)
    last_mouse_move_target_ = nullptr;

  // Finding the LCA uses a standard approach. We build vectors of the
  // ancestors of each node up to the root, and then remove common ancestors.
  std::vector<RenderWidgetHostViewInput*> entered_views;
  std::vector<RenderWidgetHostViewInput*> exited_views;
  RenderWidgetHostViewInput* cur_view = target;
  entered_views.push_back(cur_view);
  // Non-root RWHVs are guaranteed to be RenderWidgetHostViewChildFrames,
  // as long as they are the only embeddable RWHVs.
  while (cur_view->GetParentViewInput()) {
    cur_view = cur_view->GetParentViewInput();
    entered_views.push_back(cur_view);
  }
  // There are two cases where cur_view != root_view :
  // 1. On Windows, render widget targeting could produce a target that is
  // outside of the specified root. See https://crbug.com/851958. In this case,
  // we'll just give up.
  // 2. cur_view's GetParentViewInput() can possibly be nullptr for
  // guestviews (Chrome App or WebUI) that are not currently connected to
  // the webcontents tree. It is fine to return early since this would be
  // attempted again on next mouse event and once guestview is connected, the
  // ancestors will be notified and state would be updated.
  if (cur_view != root_view) {
    return;
  }

  cur_view = last_mouse_move_target_;
  if (cur_view) {
    exited_views.push_back(cur_view);
    while (cur_view->GetParentViewInput()) {
      cur_view = cur_view->GetParentViewInput();
      exited_views.push_back(cur_view);
    }
    if (cur_view != root_view) {
      return;
    }
    DCHECK_EQ(cur_view, root_view);
  }

  // This removes common ancestors from the root downward.
  RenderWidgetHostViewInput* common_ancestor = nullptr;
  while (entered_views.size() > 0 && exited_views.size() > 0 &&
         entered_views.back() == exited_views.back()) {
    common_ancestor = entered_views.back();
    entered_views.pop_back();
    exited_views.pop_back();
  }

  gfx::PointF transformed_point;
  // Send MouseLeaves.
  for (auto* view : exited_views) {
    blink::WebMouseEvent mouse_leave(event);
    mouse_leave.SetType(blink::WebInputEvent::Type::kMouseLeave);
    mouse_leave.SetModifiers(mouse_leave.GetModifiers() | extra_modifiers);
    // There is a chance of a race if the last target has recently created a
    // new compositor surface. The SurfaceID for that might not have
    // propagated to its embedding surface, which makes it impossible to
    // compute the transformation for it
    if (!root_view->TransformPointToCoordSpaceForView(
            event.PositionInWidget(), view, &transformed_point)) {
      transformed_point = gfx::PointF();
    }
    mouse_leave.SetPositionInWidget(transformed_point.x(),
                                    transformed_point.y());
    view->ProcessMouseEvent(mouse_leave, ui::LatencyInfo());
  }

  // The ancestor might need to trigger MouseOut handlers.
  if (common_ancestor && (include_target_view || common_ancestor != target)) {
    blink::WebMouseEvent mouse_move(event);
    mouse_move.SetModifiers(mouse_move.GetModifiers() | extra_modifiers);
    mouse_move.SetType(blink::WebInputEvent::Type::kMouseMove);
    if (!root_view->TransformPointToCoordSpaceForView(
            event.PositionInWidget(), common_ancestor, &transformed_point)) {
      transformed_point = gfx::PointF();
    }
    mouse_move.SetPositionInWidget(transformed_point.x(),
                                   transformed_point.y());
    common_ancestor->ProcessMouseEvent(mouse_move, ui::LatencyInfo());
  }

  // Send MouseMoves to trigger MouseEnter handlers.
  for (auto* view : entered_views) {
    if (view == target && !include_target_view)
      continue;
    blink::WebMouseEvent mouse_enter(event);
    mouse_enter.SetModifiers(mouse_enter.GetModifiers() | extra_modifiers);
    mouse_enter.SetType(blink::WebInputEvent::Type::kMouseMove);
    if (!root_view->TransformPointToCoordSpaceForView(
            event.PositionInWidget(), view, &transformed_point)) {
      transformed_point = gfx::PointF();
    }
    mouse_enter.SetPositionInWidget(transformed_point.x(),
                                    transformed_point.y());
    view->ProcessMouseEvent(mouse_enter, ui::LatencyInfo());
  }

  last_mouse_move_target_ = target;
  last_mouse_move_root_view_ = root_view;
}

void RenderWidgetHostInputEventRouter::ReportBubblingScrollToSameView(
    const blink::WebGestureEvent& event,
    const RenderWidgetHostViewInput* view) {
#if 0
  // For now, we've disabled the DumpWithoutCrashing as it's no longer
  // providing useful information.
  // TODO(crbug.com/41380487): Determine useful crash keys and reenable the
  // report.
  base::debug::DumpWithoutCrashing();
#endif
}

namespace {

// Returns true if |target_view| is one of |starting_view|'s ancestors.
// If |stay_within| is provided, we only consider ancestors within that
// sub-tree.
bool IsAncestorView(RenderWidgetHostViewInput* starting_view,
                    const RenderWidgetHostViewInput* target_view,
                    const RenderWidgetHostViewInput* stay_within = nullptr) {
  RenderWidgetHostViewInput* cur_view = starting_view->GetParentViewInput();
  while (cur_view) {
    if (cur_view == target_view)
      return true;

    if (stay_within && cur_view == stay_within)
      return false;

    cur_view = cur_view->GetParentViewInput();
  }
  return false;
}

// Given |event| in root coordinates, return an event in |target_view|'s
// coordinates.
blink::WebGestureEvent GestureEventInTarget(
    const blink::WebGestureEvent& event,
    RenderWidgetHostViewInput* target_view) {
  const gfx::PointF point_in_target =
      target_view->TransformRootPointToViewCoordSpace(event.PositionInWidget());
  blink::WebGestureEvent event_for_target(event);
  event_for_target.SetPositionInWidget(point_in_target);
  return event_for_target;
}

}  // namespace

bool RenderWidgetHostInputEventRouter::BubbleScrollEvent(
    RenderWidgetHostViewInput* target_view,
    RenderWidgetHostViewInput* resending_view,
    const blink::WebGestureEvent& event) {
  TRACE_EVENT1("input", "RenderWidgetHostInputEventRouter::BubbleScrollEvent",
               "type", blink::WebInputEvent::GetName(event.GetType()));
  DCHECK(target_view);
  DCHECK(event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin ||
         event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate ||
         event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd);

  TRACE_EVENT_INSTANT("input", "Values", "target_view",
                      static_cast<void*>(target_view), "resending_view",
                      static_cast<void*>(resending_view),
                      "bubbling_gesture_scroll_target_",
                      static_cast<void*>(bubbling_gesture_scroll_target_),
                      "bubbling_gesture_scroll_origin_",
                      static_cast<void*>(bubbling_gesture_scroll_origin_));

  ui::LatencyInfo latency_info;

  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
    forced_last_fling_start_target_to_stop_flinging_for_test_ = false;
    // If target_view has unrelated gesture events in progress, do
    // not proceed. This could cause confusion between independent
    // scrolls.
    if (target_view == touchscreen_gesture_target_.get() ||
        target_view == touchpad_gesture_target_ ||
        target_view == touch_target_) {
      TRACE_EVENT_INSTANT("input", "EarlyOut-GestureInProgress", "target_view",
                          static_cast<void*>(target_view),
                          "touchscreen_gesture_target_",
                          static_cast<void*>(touchscreen_gesture_target_.get()),
                          "touchpad_gesture_target_",
                          static_cast<void*>(touchpad_gesture_target_),
                          "touch_target_", static_cast<void*>(touch_target_));
      return false;
    }

    // A view is trying to bubble a separate scroll sequence while we have
    // ongoing bubbling.
    if (bubbling_gesture_scroll_target_ &&
        bubbling_gesture_scroll_target_ != resending_view) {
      TRACE_EVENT_INSTANT0("input", "EarlyOut-Reentry",
                           TRACE_EVENT_SCOPE_THREAD);
      return false;
    }

    // This accounts for bubbling through nested OOPIFs. A gesture scroll
    // begin has been bubbled but the target has sent back a gesture scroll
    // event ack which didn't consume any scroll delta, and so another level
    // of bubbling is needed. This requires a GestureScrollEnd be sent to the
    // last view, which will no longer be the scroll target.
    if (bubbling_gesture_scroll_target_) {
      SendGestureScrollEnd(
          bubbling_gesture_scroll_target_,
          GestureEventInTarget(event, bubbling_gesture_scroll_target_));
    } else {
      bubbling_gesture_scroll_origin_ = resending_view;
    }

    bubbling_gesture_scroll_target_ = target_view;
    bubbling_gesture_scroll_source_device_ = event.SourceDevice();
    DCHECK(IsAncestorView(bubbling_gesture_scroll_origin_,
                          bubbling_gesture_scroll_target_));
  } else {  // !(event.GetType() ==
            // blink::WebInputEvent::Type::kGestureScrollBegin)
    if (!bubbling_gesture_scroll_target_) {
      // Drop any acked events that come in after bubbling has ended.
      // TODO(mcnee): If we inform |bubbling_gesture_scroll_origin_| and the
      // intermediate views of the end of bubbling, we could presumably DCHECK
      // that we have a target.
      TRACE_EVENT_INSTANT0("input", "Drop_Event_Target_Gone",
                           TRACE_EVENT_SCOPE_THREAD);
      return false;
    }

    // Don't bubble the GSE events that are generated and sent to intermediate
    // bubbling targets.
    if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd &&
        resending_view != bubbling_gesture_scroll_origin_) {
      TRACE_EVENT_INSTANT0("input",
                           "Dont_Bubble_GestureScrollEnd_Intermediate_Sender",
                           TRACE_EVENT_SCOPE_THREAD);
      return true;
    }
  }

  // If the router tries to resend a gesture scroll event back to the same
  // view, we could hang.
  DCHECK_NE(resending_view, bubbling_gesture_scroll_target_);
  // We've seen reports of this, but don't know the cause yet. For now,
  // instead of CHECKing or hanging, we'll report the issue and abort scroll
  // bubbling.
  // TODO(crbug.com/41380487): Remove once this issue no longer occurs.
  if (resending_view == bubbling_gesture_scroll_target_) {
    ReportBubblingScrollToSameView(event, resending_view);
    CancelScrollBubbling();
    TRACE_EVENT_INSTANT0("input", "EarlyOut-SameView",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  const bool touchscreen_bubble_to_root =
      event.SourceDevice() == blink::WebGestureDevice::kTouchscreen &&
      !bubbling_gesture_scroll_target_->GetParentViewInput();
  if (touchscreen_bubble_to_root) {
    if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
      touchscreen_pinch_state_.DidStartBubblingToRoot();

      // If a pinch's scroll sequence is sent to an OOPIF and the pinch
      // begin event is dispatched to the root before the scroll begin is
      // bubbled to the root, a wrapping scroll begin will have already been
      // sent to the root.
      if (touchscreen_pinch_state_.IsInPinch()) {
        TRACE_EVENT_INSTANT0("input", "EarlyOut-IsInPinch",
                             TRACE_EVENT_SCOPE_THREAD);
        return true;
      }
    } else if (event.GetType() ==
               blink::WebInputEvent::Type::kGestureScrollEnd) {
      touchscreen_pinch_state_.DidStopBubblingToRoot();
    }
  }

  TRACE_EVENT_INSTANT0("input", "Did_Bubble_Scroll_Event_To_Target",
                       TRACE_EVENT_SCOPE_THREAD);
  bubbling_gesture_scroll_target_->ProcessGestureEvent(
      GestureEventInTarget(event, bubbling_gesture_scroll_target_),
      latency_info);

  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd) {
    bubbling_gesture_scroll_origin_ = nullptr;
    bubbling_gesture_scroll_target_ = nullptr;
    bubbling_gesture_scroll_source_device_ =
        blink::WebGestureDevice::kUninitialized;
  }
  return true;
}

void RenderWidgetHostInputEventRouter::SendGestureScrollBegin(
    RenderWidgetHostViewInput* view,
    const blink::WebGestureEvent& event) {
  DCHECK_EQ(blink::WebInputEvent::Type::kGesturePinchBegin, event.GetType());
  DCHECK_EQ(blink::WebGestureDevice::kTouchscreen, event.SourceDevice());
  blink::WebGestureEvent scroll_begin(event);
  scroll_begin.SetType(blink::WebInputEvent::Type::kGestureScrollBegin);
  scroll_begin.data.scroll_begin.delta_x_hint = 0;
  scroll_begin.data.scroll_begin.delta_y_hint = 0;
  scroll_begin.data.scroll_begin.delta_hint_units =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_begin.data.scroll_begin.scrollable_area_element_id = 0;
  view->ProcessGestureEvent(scroll_begin, ui::LatencyInfo());
}

void RenderWidgetHostInputEventRouter::SendGestureScrollEnd(
    RenderWidgetHostViewInput* view,
    const blink::WebGestureEvent& event) {
  blink::WebGestureEvent scroll_end(event);
  scroll_end.SetType(blink::WebInputEvent::Type::kGestureScrollEnd);
  scroll_end.SetTimeStamp(base::TimeTicks::Now());
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin:
      scroll_end.data.scroll_end.inertial_phase =
          event.data.scroll_begin.inertial_phase;
      scroll_end.data.scroll_end.delta_units =
          event.data.scroll_begin.delta_hint_units;
      break;
    case blink::WebInputEvent::Type::kGesturePinchEnd:
      DCHECK_EQ(blink::WebGestureDevice::kTouchscreen, event.SourceDevice());
      scroll_end.data.scroll_end.inertial_phase =
          blink::WebGestureEvent::InertialPhaseState::kUnknownMomentum;
      scroll_end.data.scroll_end.delta_units =
          ui::ScrollGranularity::kScrollByPrecisePixel;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  view->ProcessGestureEvent(scroll_end, ui::LatencyInfo());
}

void RenderWidgetHostInputEventRouter::SendGestureScrollEnd(
    RenderWidgetHostViewInput* view,
    blink::WebGestureDevice source_device) {
  blink::WebGestureEvent scroll_end(
      blink::WebInputEvent::Type::kGestureScrollEnd,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
      source_device);
  scroll_end.data.scroll_end.inertial_phase =
      blink::WebGestureEvent::InertialPhaseState::kUnknownMomentum;
  scroll_end.data.scroll_end.delta_units =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  view->ProcessGestureEvent(scroll_end, ui::LatencyInfo());
}

void RenderWidgetHostInputEventRouter::WillDetachChildView(
    const RenderWidgetHostViewInput* detaching_view) {
  // If necessary, cancel ongoing scroll bubbling in response to a frame
  // connector change.
  if (!bubbling_gesture_scroll_target_ || !bubbling_gesture_scroll_origin_)
    return;

  // We cancel bubbling only when the child view affects the current scroll
  // bubbling sequence.
  if (detaching_view == bubbling_gesture_scroll_origin_ ||
      IsAncestorView(bubbling_gesture_scroll_origin_, detaching_view)) {
    CancelScrollBubbling();
  }
}

void RenderWidgetHostInputEventRouter::CancelScrollBubbling() {
  DCHECK(bubbling_gesture_scroll_target_);
  SendGestureScrollEnd(bubbling_gesture_scroll_target_,
                       bubbling_gesture_scroll_source_device_);

  const bool touchscreen_bubble_to_root =
      bubbling_gesture_scroll_source_device_ ==
          blink::WebGestureDevice::kTouchscreen &&
      !bubbling_gesture_scroll_target_->GetParentViewInput();
  if (touchscreen_bubble_to_root)
    touchscreen_pinch_state_.DidStopBubblingToRoot();

  // TODO(mcnee): We should also inform |bubbling_gesture_scroll_origin_| that
  // we are no longer bubbling its events, otherwise it could continue to send
  // them and interfere with a new scroll gesture being bubbled.
  // See https://crbug.com/828422
  bubbling_gesture_scroll_origin_ = nullptr;
  bubbling_gesture_scroll_target_ = nullptr;
  bubbling_gesture_scroll_source_device_ =
      blink::WebGestureDevice::kUninitialized;
}

void RenderWidgetHostInputEventRouter::CancelScrollBubblingIfConflicting(
    const RenderWidgetHostViewInput* target) {
  if (!target)
    return;
  if (!bubbling_gesture_scroll_target_ || !bubbling_gesture_scroll_origin_)
    return;

  if (IsAncestorView(bubbling_gesture_scroll_origin_, target,
                     bubbling_gesture_scroll_target_)) {
    CancelScrollBubbling();
  }
}

void RenderWidgetHostInputEventRouter::StopFling() {
  if (!bubbling_gesture_scroll_target_)
    return;

  if (!last_fling_start_target_ ||
      !last_fling_start_target_->GetViewRenderInputRouter()) {
    return;
  }

  // The last_fling_start_target_'s fling controller must stop flinging when its
  // generated GSUs are not consumed by the bubbling target view.
  last_fling_start_target_->GetViewRenderInputRouter()->StopFling();
  forced_last_fling_start_target_to_stop_flinging_for_test_ = true;
}
void RenderWidgetHostInputEventRouter::AddFrameSinkIdOwner(
    const viz::FrameSinkId& id,
    RenderWidgetHostViewInput* owner) {
  DCHECK(owner_map_.find(id) == owner_map_.end());
  // We want to be notified if the owner is destroyed so we can remove it from
  // our map.
  owner->AddObserver(this);
  owner_map_.insert(std::make_pair(id, owner->GetInputWeakPtr()));
}

void RenderWidgetHostInputEventRouter::RemoveFrameSinkIdOwner(
    const viz::FrameSinkId& id) {
  auto it_to_remove = owner_map_.find(id);
  if (it_to_remove != owner_map_.end()) {
    // If we remove a view from the observer list, we need to be sure to do a
    // cleanup of the various targets and target maps, else we will end up with
    // stale values if the view destructs and isn't an observer anymore.
    // Note: the view the iterator points at will be deleted in the following
    // call, and shouldn't be used after this point.
    if (it_to_remove->second)
      OnRenderWidgetHostViewInputDestroyed(it_to_remove->second.get());
  }
}

RenderWidgetHostViewInput*
RenderWidgetHostInputEventRouter::GetRenderWidgetHostViewInputAtPoint(
    RenderWidgetHostViewInput* root_view,
    const gfx::PointF& point,
    gfx::PointF* transformed_point) {
  if (!root_view) {
    return nullptr;
  }
  return FindViewAtLocation(root_view, point, viz::EventSource::MOUSE,
                            transformed_point)
      .view;
}

void RenderWidgetHostInputEventRouter::GetRenderWidgetHostAtPointAsynchronously(
    RenderWidgetHostViewInput* root_view,
    const gfx::PointF& point,
    RenderWidgetTargeter::RenderWidgetHostAtPointCallback callback) {
  event_targeter_->FindTargetAndCallback(root_view, point, std::move(callback));
}

RenderWidgetTargetResult
RenderWidgetHostInputEventRouter::FindTouchscreenGestureEventTarget(
    RenderWidgetHostViewInput* root_view,
    const blink::WebGestureEvent& gesture_event) {
  // Since DispatchTouchscreenGestureEvent() doesn't pay any attention to the
  // target we could just return nullptr for pinch events, but since we know
  // where they are going we return the correct target.
  if (blink::WebInputEvent::IsPinchGestureEventType(gesture_event.GetType()))
    return {root_view, false, gesture_event.PositionInWidget(), true};

  // Android sends gesture events that have no corresponding touch sequence, so
  // these we hit-test explicitly.
  if (gesture_event.unique_touch_event_id == 0) {
    gfx::PointF transformed_point;
    gfx::PointF original_point(gesture_event.PositionInWidget());
    return FindViewAtLocation(root_view, original_point,
                              viz::EventSource::TOUCH, &transformed_point);
  }

  // Remaining gesture events will defer to the gesture event target queue
  // during dispatch.
  return {nullptr, false, std::nullopt, true};
}

bool RenderWidgetHostInputEventRouter::IsViewInMap(
    const RenderWidgetHostViewInput* view) const {
  if (!view)
    return false;

  auto it = owner_map_.find(view->GetFrameSinkId());
  if (it == owner_map_.end()) {
    return false;
  }

  return it->second.get() == view;
}

bool RenderWidgetHostInputEventRouter::ViewMapIsEmpty() const {
  return owner_map_.empty();
}

namespace {

bool IsPinchCurrentlyAllowedInTarget(RenderWidgetHostViewInput* target) {
  std::optional<cc::TouchAction> target_active_touch_action(
      cc::TouchAction::kNone);
  if (target) {
    target_active_touch_action =
        target->GetViewRenderInputRouter()->input_router()->ActiveTouchAction();
  }
  // This function is called on GesturePinchBegin, by which time there should
  // be an active touch action assessed for the target.
  // DCHECK(target_active_touch_action.has_value());
  // TODO(wjmaclean): Find out why we can be in the middle of a gesture
  // sequence and not have a valid touch action assigned.
  if (!target_active_touch_action)
    target_active_touch_action = cc::TouchAction::kNone;
  return (target_active_touch_action.value() & cc::TouchAction::kPinchZoom) !=
         cc::TouchAction::kNone;
}

}  // namespace

void RenderWidgetHostInputEventRouter::DispatchTouchscreenGestureEvent(
    RenderWidgetHostViewInput* root_view,
    RenderWidgetHostViewInput* target,
    const blink::WebGestureEvent& gesture_event,
    const ui::LatencyInfo& latency,
    const std::optional<gfx::PointF>& target_location) {
  if (gesture_event.GetType() ==
      blink::WebInputEvent::Type::kGesturePinchBegin) {
    if (root_view == touchscreen_gesture_target_.get()) {
      // If the root view is the current gesture target, there is no need to
      // wrap the pinch events ourselves.
      touchscreen_pinch_state_.DidStartPinchInRoot();
    } else if (IsPinchCurrentlyAllowedInTarget(
                   touchscreen_gesture_target_.get())) {
      // If pinch is not allowed in the child, don't do any diverting of the
      // pinch events to the root. Have the events go to the child whose
      // TouchActionFilter will discard them.

      auto* rir = root_view->GetViewRenderInputRouter();

      // The pinch gesture will be sent to the root view and it may not have a
      // valid touch action yet. In this case, set the touch action to auto.
      rir->input_router()->ForceSetTouchActionAuto();

      if (touchscreen_pinch_state_.NeedsWrappingScrollSequence()) {
        // If the root view is not the gesture target, and a scroll gesture has
        // not already started in the root from scroll bubbling, then we need
        // to warp the diverted pinch events in a GestureScrollBegin/End.
        DCHECK(!rir->is_in_touchscreen_gesture_scroll());
        SendGestureScrollBegin(root_view, gesture_event);
      }

      touchscreen_pinch_state_.DidStartPinchInChild();
    }
  }

  if (touchscreen_pinch_state_.IsInPinch()) {
    root_view->ProcessGestureEvent(gesture_event, latency);

    if (gesture_event.GetType() ==
        blink::WebInputEvent::Type::kGesturePinchEnd) {
      const bool send_scroll_end =
          touchscreen_pinch_state_.NeedsWrappingScrollSequence();
      touchscreen_pinch_state_.DidStopPinch();

      if (send_scroll_end) {
        auto* rir = root_view->GetViewRenderInputRouter();
        DCHECK(rir->is_in_touchscreen_gesture_scroll());
        SendGestureScrollEnd(root_view, gesture_event);
      }
    }

    return;
  }

  if (gesture_event.GetType() ==
          blink::WebInputEvent::Type::kGestureFlingCancel &&
      last_fling_start_target_) {
    last_fling_start_target_->ProcessGestureEvent(gesture_event, latency);
    return;
  }

  auto gesture_target_it =
      touchscreen_gesture_target_map_.find(gesture_event.unique_touch_event_id);
  bool no_matching_id =
      gesture_target_it == touchscreen_gesture_target_map_.end();

  // We use GestureTapDown to detect the start of a gesture sequence since
  // there is no WebGestureEvent equivalent for EventType::kGestureBegin.
  const bool is_gesture_start =
      gesture_event.GetType() == blink::WebInputEvent::Type::kGestureTapDown;

  std::optional<gfx::PointF> fallback_target_location;

  if (gesture_event.unique_touch_event_id == 0) {
    // On Android it is possible for touchscreen gesture events to arrive that
    // are not associated with touch events, because non-synthetic events can be
    // created by ContentView. These will use the target found by the
    // RenderWidgetTargeter. These gesture events should always have a
    // unique_touch_event_id of 0. They must have a non-null target in order
    // to get the coordinate transform.
    DCHECK(target);
    fallback_target_location = target_location;
  } else if (no_matching_id && is_gesture_start) {
    // A long-standing Windows issues where occasionally a GestureStart is
    // encountered with no targets in the event queue. We never had a repro for
    // this, but perhaps we should drop these events and wait to see if a bug
    // (with a repro) gets filed, then just fix it.
    //
    // For now, we do a synchronous-only hit test here, which even though
    // incorrect is not likely to have a large effect in the short term.
    // It is still safe to continue; we will recalculate the target.
    gfx::PointF transformed_point;
    gfx::PointF original_point(gesture_event.PositionInWidget());
    auto result = FindViewAtLocation(
        root_view, original_point, viz::EventSource::TOUCH, &transformed_point);
    // Re https://crbug.com/796656): Since we are already in an error case,
    // don't worry about the fact we're ignoring |result.should_query_view|, as
    // this is the best we can do until we fix https://crbug.com/595422.
    target = result.view;

    fallback_target_location = transformed_point;
  } else if (is_gesture_start) {
    target = gesture_target_it->second.get();

    touchscreen_gesture_target_map_.erase(gesture_target_it);

    // Abort any scroll bubbling in progress to avoid double entry.
    CancelScrollBubblingIfConflicting(target);
  }

  if (gesture_event.unique_touch_event_id == 0 || is_gesture_start) {
    bool moved_recently = touchscreen_gesture_target_moved_recently_;
    bool moved_recently_for_iov2 =
        touchscreen_gesture_target_moved_recently_for_iov2_;
    // It seem that |target| can be nullptr here, not sure why. Until we know
    // why, let's avoid dereferencing it in that case.
    // https://bugs.chromium.org/p/chromium/issues/detail?id=1155297
    if (is_gesture_start && target) {
      moved_recently = target->ScreenRectIsUnstableFor(gesture_event);
      moved_recently_for_iov2 =
          target->ScreenRectIsUnstableForIOv2For(gesture_event);
    }
    SetTouchscreenGestureTarget(target, moved_recently,
                                moved_recently_for_iov2);
  }

  // If we set a target and it's not in the map, we won't get notified if the
  // target goes away, so drop the target and the resulting events.
  if (!IsViewInMap(touchscreen_gesture_target_.get())) {
    ClearTouchscreenGestureTarget();
  }

  if (!touchscreen_gesture_target_) {
    root_view->GestureEventAck(
        gesture_event, blink::mojom::InputEventResultSource::kBrowser,
        blink::mojom::InputEventResultState::kNoConsumerExists);
    return;
  }

  blink::WebGestureEvent event(gesture_event);
  if (touchscreen_gesture_target_moved_recently_) {
    event.SetTargetFrameMovedRecently();
  }
  if (touchscreen_gesture_target_moved_recently_for_iov2_) {
    event.SetTargetFrameMovedRecentlyForIOv2();
  }

  gfx::PointF point_in_target;
  // This |fallback_target_location| is fast path when
  // |gesture_event.unique_touch_event_id == 0| or
  // |no_matching_id && is_gesture_start|, we did actually do hit testing in
  // these two cases. |target_location| pass in maybe wrong in other cases.
  if (fallback_target_location) {
    point_in_target = fallback_target_location.value();
  } else {
    point_in_target =
        touchscreen_gesture_target_->TransformRootPointToViewCoordSpace(
            gesture_event.PositionInWidget());
  }

  event.SetPositionInWidget(point_in_target);

  if (events_being_flushed_) {
    touchscreen_gesture_target_->GetViewRenderInputRouter()
        ->input_router()
        ->ForceSetTouchActionAuto();
  }
  touchscreen_gesture_target_->ProcessGestureEvent(event, latency);

  if (gesture_event.GetType() == blink::WebInputEvent::Type::kGestureFlingStart)
    last_fling_start_target_ = touchscreen_gesture_target_.get();

  // If we have one of the following events, then the user has lifted their
  // last finger.
  const bool is_gesture_end =
      gesture_event.GetType() == blink::WebInputEvent::Type::kGestureTap ||
      gesture_event.GetType() == blink::WebInputEvent::Type::kGestureLongTap ||
      gesture_event.GetType() ==
          blink::WebInputEvent::Type::kGestureDoubleTap ||
      gesture_event.GetType() ==
          blink::WebInputEvent::Type::kGestureTwoFingerTap ||
      gesture_event.GetType() ==
          blink::WebInputEvent::Type::kGestureScrollEnd ||
      gesture_event.GetType() == blink::WebInputEvent::Type::kGestureFlingStart;

  if (is_gesture_end) {
    ClearTouchscreenGestureTarget();
  }
}

void RenderWidgetHostInputEventRouter::RouteTouchscreenGestureEvent(
    RenderWidgetHostViewInput* root_view,
    const blink::WebGestureEvent* event,
    const ui::LatencyInfo& latency) {
  DCHECK_EQ(blink::WebGestureDevice::kTouchscreen, event->SourceDevice());
  event_targeter_->FindTargetAndDispatch(root_view, *event, latency);
}

RenderWidgetTargetResult
RenderWidgetHostInputEventRouter::FindTouchpadGestureEventTarget(
    RenderWidgetHostViewInput* root_view,
    const blink::WebGestureEvent& event) const {
  if (event.GetType() != blink::WebInputEvent::Type::kGesturePinchBegin &&
      event.GetType() != blink::WebInputEvent::Type::kGestureFlingCancel &&
      event.GetType() != blink::WebInputEvent::Type::kGestureDoubleTap) {
    return {nullptr, false, std::nullopt, true};
  }

  gfx::PointF transformed_point;
  return FindViewAtLocation(root_view, event.PositionInWidget(),
                            viz::EventSource::MOUSE, &transformed_point);
}

void RenderWidgetHostInputEventRouter::RouteTouchpadGestureEvent(
    RenderWidgetHostViewInput* root_view,
    const blink::WebGestureEvent* event,
    const ui::LatencyInfo& latency) {
  DCHECK_EQ(blink::WebGestureDevice::kTouchpad, event->SourceDevice());
  event_targeter_->FindTargetAndDispatch(root_view, *event, latency);
}

void RenderWidgetHostInputEventRouter::DispatchTouchpadGestureEvent(
    RenderWidgetHostViewInput* root_view,
    RenderWidgetHostViewInput* target,
    const blink::WebGestureEvent& touchpad_gesture_event,
    const ui::LatencyInfo& latency,
    const std::optional<gfx::PointF>& target_location) {
  // Touchpad gesture flings should be treated as mouse wheels for the purpose
  // of routing.
  if (touchpad_gesture_event.GetType() ==
      blink::WebInputEvent::Type::kGestureFlingStart) {
    if (wheel_target_) {
      blink::WebGestureEvent gesture_fling = touchpad_gesture_event;
      gfx::PointF point_in_target =
          wheel_target_->TransformRootPointToViewCoordSpace(
              gesture_fling.PositionInWidget());
      gesture_fling.SetPositionInWidget(point_in_target);
      wheel_target_->ProcessGestureEvent(gesture_fling, latency);
      last_fling_start_target_ = wheel_target_;
    } else {
      root_view->GestureEventAck(
          touchpad_gesture_event,
          blink::mojom::InputEventResultSource::kBrowser,
          blink::mojom::InputEventResultState::kNoConsumerExists);
    }
    return;
  }

  if (touchpad_gesture_event.GetType() ==
      blink::WebInputEvent::Type::kGestureFlingCancel) {
    if (last_fling_start_target_) {
      last_fling_start_target_->ProcessGestureEvent(touchpad_gesture_event,
                                                    latency);
    } else if (target) {
      target->ProcessGestureEvent(touchpad_gesture_event, latency);
    } else {
      root_view->GestureEventAck(
          touchpad_gesture_event,
          blink::mojom::InputEventResultSource::kBrowser,
          blink::mojom::InputEventResultState::kNoConsumerExists);
    }
    return;
  }

  if (target) {
    touchpad_gesture_target_ = target;

    // Abort any scroll bubbling in progress to avoid double entry.
    CancelScrollBubblingIfConflicting(touchpad_gesture_target_);
  }

  if (!touchpad_gesture_target_) {
    root_view->GestureEventAck(
        touchpad_gesture_event, blink::mojom::InputEventResultSource::kBrowser,
        blink::mojom::InputEventResultState::kNoConsumerExists);
    return;
  }

  blink::WebGestureEvent gesture_event = touchpad_gesture_event;
  gfx::PointF point_in_target;
  if (target_location) {
    point_in_target = target_location.value();
  } else {
    point_in_target =
        touchpad_gesture_target_->TransformRootPointToViewCoordSpace(
            gesture_event.PositionInWidget());
  }
  gesture_event.SetPositionInWidget(point_in_target);
  touchpad_gesture_target_->ProcessGestureEvent(gesture_event, latency);

  if (touchpad_gesture_event.GetType() ==
          blink::WebInputEvent::Type::kGesturePinchEnd ||
      touchpad_gesture_event.GetType() ==
          blink::WebInputEvent::Type::kGestureDoubleTap) {
    touchpad_gesture_target_ = nullptr;
  }
}

RenderWidgetHostViewInput*
RenderWidgetHostInputEventRouter::FindViewFromFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) const {
  // TODO(kenrb): There should be a better way to handle hit tests to surfaces
  // that are no longer valid for hit testing. See https://crbug.com/790044.
  auto iter = owner_map_.find(frame_sink_id);
  // If the point hit a Surface whose namspace is no longer in the map, then
  // it likely means the RenderWidgetHostView has been destroyed but its
  // parent frame has not sent a new compositor frame since that happened.
  return iter == owner_map_.end() ? nullptr : iter->second.get();
}

bool RenderWidgetHostInputEventRouter::ShouldContinueHitTesting(
    RenderWidgetHostViewInput* target_view) const {
  // Determine if |view| has any embedded children that could potentially
  // receive the event.
  auto* rir = target_view->GetViewRenderInputRouter();
  std::unique_ptr<RenderInputRouterIterator> child_rirs(
      rir->GetEmbeddedRenderInputRouters());
  if (child_rirs->GetNextRouter()) {
    return true;
  }

  return false;
}

std::vector<RenderWidgetHostViewInput*>
RenderWidgetHostInputEventRouter::GetRenderWidgetHostViewInputsForTests()
    const {
  std::vector<RenderWidgetHostViewInput*> hosts;
  for (auto entry : owner_map_) {
    DCHECK(entry.second);
    hosts.push_back(entry.second.get());
  }

  return hosts;
}

RenderWidgetTargeter*
RenderWidgetHostInputEventRouter::GetRenderWidgetTargeterForTests() {
  return event_targeter_.get();
}

RenderWidgetTargetResult
RenderWidgetHostInputEventRouter::FindTargetSynchronouslyAtPoint(
    RenderWidgetHostViewInput* root_view,
    const gfx::PointF& location) {
  gfx::PointF transformed_pt;
  // The transformed point is already in the return value of FindViewAtLocation.
  return FindViewAtLocation(root_view, location, viz::EventSource::MOUSE,
                            &transformed_pt);
}

RenderWidgetTargetResult
RenderWidgetHostInputEventRouter::FindTargetSynchronously(
    RenderWidgetHostViewInput* root_view,
    const blink::WebInputEvent& event) {
  if (blink::WebInputEvent::IsMouseEventType(event.GetType())) {
    return FindMouseEventTarget(
        root_view, static_cast<const blink::WebMouseEvent&>(event));
  }
  if (event.GetType() == blink::WebInputEvent::Type::kMouseWheel) {
    return FindMouseWheelEventTarget(
        root_view, static_cast<const blink::WebMouseWheelEvent&>(event));
  }
  if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
    return FindTouchEventTarget(
        root_view, static_cast<const blink::WebTouchEvent&>(event));
  }
  if (blink::WebInputEvent::IsGestureEventType(event.GetType())) {
    const auto& gesture_event =
        static_cast<const blink::WebGestureEvent&>(event);
    if (gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchscreen) {
      return FindTouchscreenGestureEventTarget(root_view, gesture_event);
    }
    if (gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchpad) {
      return FindTouchpadGestureEventTarget(root_view, gesture_event);
    }
  }
  NOTREACHED_IN_MIGRATION();
  return RenderWidgetTargetResult();
}

void RenderWidgetHostInputEventRouter::SetEventsBeingFlushed(
    bool events_being_flushed) {
  events_being_flushed_ = events_being_flushed;
}

void RenderWidgetHostInputEventRouter::SetTouchscreenGestureTarget(
    RenderWidgetHostViewInput* target,
    bool moved_recently,
    bool moved_recently_for_iov2) {
  touchscreen_gesture_target_ = target ? target->GetInputWeakPtr() : nullptr;
  touchscreen_gesture_target_moved_recently_ = moved_recently;
  touchscreen_gesture_target_moved_recently_for_iov2_ = moved_recently_for_iov2;
}

void RenderWidgetHostInputEventRouter::ClearTouchscreenGestureTarget() {
  SetTouchscreenGestureTarget(nullptr, false, false);
}

void RenderWidgetHostInputEventRouter::DispatchEventToTarget(
    RenderWidgetHostViewInput* root_view,
    RenderWidgetHostViewInput* target,
    blink::WebInputEvent* event,
    const ui::LatencyInfo& latency,
    const std::optional<gfx::PointF>& target_location) {
  DCHECK(event);
  if (target && target->ScreenRectIsUnstableFor(*event))
    event->SetTargetFrameMovedRecently();
  if (blink::WebInputEvent::IsMouseEventType(event->GetType())) {
    if (target && event->GetType() == blink::WebInputEvent::Type::kMouseDown) {
      mouse_down_post_transformed_coordinate_.SetPoint(target_location->x(),
                                                       target_location->y());
      last_mouse_down_target_ = target;
    }
    DispatchMouseEvent(root_view, target,
                       *static_cast<blink::WebMouseEvent*>(event), latency,
                       target_location);
    return;
  }
  if (event->GetType() == blink::WebInputEvent::Type::kMouseWheel) {
    DispatchMouseWheelEvent(root_view, target,
                            *static_cast<blink::WebMouseWheelEvent*>(event),
                            latency, target_location);
    return;
  }
  if (blink::WebInputEvent::IsTouchEventType(event->GetType())) {
    auto& touch_event = *static_cast<blink::WebTouchEvent*>(event);
    TouchEventWithLatencyInfo touch_with_latency(touch_event, latency);
    auto* touch_emulator = GetTouchEmulator(/*create_if_necessary=*/false);
    if (touch_emulator &&
        touch_emulator->HandleTouchEvent(touch_with_latency.event)) {
      // We cheat a little bit here, and assume that we know that even if the
      // target is a RenderWidgetHostViewChildFrame, that it would only try to
      // forward the ack to the root view anyways, so we send it there directly.
      root_view->ProcessAckedTouchEvent(
          touch_with_latency, blink::mojom::InputEventResultState::kConsumed);
      return;
    }
    DispatchTouchEvent(root_view, target, touch_event, latency, target_location,
                       false /* not emulated */);
    return;
  }
  if (blink::WebInputEvent::IsGestureEventType(event->GetType())) {
    auto& gesture_event = *static_cast<blink::WebGestureEvent*>(event);
    if (gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchscreen) {
      DispatchTouchscreenGestureEvent(root_view, target, gesture_event, latency,
                                      target_location);
      return;
    }
    if (gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchpad) {
      DispatchTouchpadGestureEvent(root_view, target, gesture_event, latency,
                                   target_location);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

TouchEmulator* RenderWidgetHostInputEventRouter::GetTouchEmulator(
    bool create_if_necessary) {
  return delegate_->GetTouchEmulator(create_if_necessary);
}

void RenderWidgetHostInputEventRouter::ForwardEmulatedGestureEvent(
    const blink::WebGestureEvent& event) {
  TRACE_EVENT0("input",
               "RenderWidgetHostInputEventRouter::ForwardEmulatedGestureEvent");
  // It's possible that since |last_emulated_event_root_view_| was set by the
  // outbound touch event that the view may have gone away. Before with dispatch
  // the GestureEvent, confirm the view is still available.
  if (!IsViewInMap(last_emulated_event_root_view_))
    return;
  DispatchTouchscreenGestureEvent(last_emulated_event_root_view_, nullptr,
                                  event, ui::LatencyInfo(),
                                  event.PositionInWidget());
}

void RenderWidgetHostInputEventRouter::ForwardEmulatedTouchEvent(
    const blink::WebTouchEvent& event,
    RenderWidgetHostViewInput* target) {
  TRACE_EVENT0("input",
               "RenderWidgetHostInputEventRouter::ForwardEmulatedTouchEvent");
  // Here we re-use the last root view we saw for a mouse move event, or fall
  // back to using |target| as the root_view if we haven't seen a mouse event;
  // this latter case only happens for injected touch events.
  // TODO(wjmaclean): Why doesn't this class just track its root view?
  DCHECK(IsViewInMap(target));
  last_emulated_event_root_view_ =
      last_mouse_move_root_view_ ? last_mouse_move_root_view_.get() : target;

  if (event.GetType() == blink::WebInputEvent::Type::kTouchStart)
    active_touches_ += CountChangedTouchPoints(event);
  gfx::PointF transformed_point = target->TransformRootPointToViewCoordSpace(
      event.touches[0].PositionInWidget());
  DispatchTouchEvent(last_emulated_event_root_view_, target, event,
                     ui::LatencyInfo(), transformed_point, true /* emulated */);
}

void RenderWidgetHostInputEventRouter::SetCursor(const ui::Cursor& cursor) {
  if (!last_mouse_move_root_view_)
    return;

  last_device_scale_factor_ =
      last_mouse_move_root_view_->GetDeviceScaleFactor();
  auto* touch_emulator = GetTouchEmulator(/*create_if_necessary=*/false);
  if (touch_emulator) {
    touch_emulator->SetDeviceScaleFactor(last_device_scale_factor_);
  }
  if (auto* cursor_manager = last_mouse_move_root_view_->GetCursorManager()) {
    for (auto it : owner_map_) {
      if (it.second)
        cursor_manager->UpdateCursor(it.second.get(), cursor);
    }
  }
}

void RenderWidgetHostInputEventRouter::ShowContextMenuAtPoint(
    const gfx::Point& point,
    const ui::MenuSourceType source_type,
    RenderWidgetHostViewInput* target) {
  DCHECK(IsViewInMap(target));
  auto* rir = target->GetViewRenderInputRouter();
  DCHECK(rir);
  rir->ShowContextMenuAtPoint(point, source_type);
}

void RenderWidgetHostInputEventRouter::OnAggregatedHitTestRegionListUpdated(
    const viz::FrameSinkId& frame_sink_id,
    const std::vector<viz::AggregatedHitTestRegion>& hit_test_data) {
  for (auto& region : hit_test_data) {
    auto iter = owner_map_.find(region.frame_sink_id);
    if (iter != owner_map_.end() && iter->second)
      iter->second->NotifyHitTestRegionUpdated(region);
  }
}

void RenderWidgetHostInputEventRouter::SetMouseCaptureTarget(
    RenderWidgetHostViewInput* target,
    bool capture) {
  auto* touch_emulator = GetTouchEmulator(/*create_if_necessary=*/false);
  if (touch_emulator && touch_emulator->IsEnabled()) {
    return;
  }

  if (capture) {
    mouse_capture_target_ = target;
    return;
  }

  if (mouse_capture_target_ == target)
    mouse_capture_target_ = nullptr;
}

void RenderWidgetHostInputEventRouter::RootViewReceivesMouseUpIfNecessary(
    bool root_view_receives_mouse_up) {
  root_view_receive_additional_mouse_up_ = root_view_receives_mouse_up;
}

void RenderWidgetHostInputEventRouter::SetAutoScrollInProgress(
    bool is_autoscroll_in_progress) {
  event_targeter_->SetIsAutoScrollInProgress(is_autoscroll_in_progress);
}

bool IsMoveEvent(ui::EventType type) {
  return type == ui::EventType::kMouseMoved ||
         type == ui::EventType::kMouseDragged ||
         type == ui::EventType::kTouchMoved;
}

void RenderWidgetHostInputEventRouter::ForwardDelegatedInkPoint(
    RenderWidgetHostViewInput* target_view,
    RenderWidgetHostViewInput* root_view,
    const blink::WebInputEvent& input_event,
    const blink::WebPointerProperties& pointer_properties,
    bool hovering) {
  const std::optional<cc::DelegatedInkBrowserMetadata>& metadata =
      target_view->GetViewRenderInputRouter()
          ->delegate()
          ->GetLastRenderFrameMetadata()
          .delegated_ink_metadata;

  if (IsMoveEvent(input_event.GetTypeAsUiEventType()) && metadata &&
      hovering == metadata.value().delegated_ink_is_hovering) {
    gfx::PointF position = pointer_properties.PositionInWidget();
    root_view->TransformPointToRootSurface(&position);
    position.Scale(target_view->GetDeviceScaleFactor());

    gfx::DelegatedInkPoint delegated_ink_point(
        position, input_event.TimeStamp(), pointer_properties.id);

    target_view->GetViewRenderInputRouter()
        ->delegate()
        ->ForwardDelegatedInkPoint(delegated_ink_point,
                                   ended_delegated_ink_trail_);
  } else {
    target_view->GetViewRenderInputRouter()
        ->delegate()
        ->ResetDelegatedInkPointPrediction(ended_delegated_ink_trail_);
  }
}

}  // namespace input
