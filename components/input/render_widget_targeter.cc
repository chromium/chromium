// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/render_widget_targeter.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "components/input/render_widget_host_view_input.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/events/blink/blink_event_util.h"

namespace input {

namespace {

gfx::PointF ComputeEventLocation(const blink::WebInputEvent& event) {
  if (blink::WebInputEvent::IsMouseEventType(event.GetType()) ||
      event.GetType() == blink::WebInputEvent::Type::kMouseWheel) {
    return static_cast<const blink::WebMouseEvent&>(event).PositionInWidget();
  }
  if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
    return static_cast<const blink::WebTouchEvent&>(event)
        .touches[0]
        .PositionInWidget();
  }
  if (blink::WebInputEvent::IsGestureEventType(event.GetType()))
    return static_cast<const blink::WebGestureEvent&>(event).PositionInWidget();

  return gfx::PointF();
}

bool IsMouseMiddleClick(const blink::WebInputEvent& event) {
  return (event.GetType() == blink::WebInputEvent::Type::kMouseDown &&
          static_cast<const blink::WebMouseEvent&>(event).button ==
              blink::WebPointerProperties::Button::kMiddle);
}

constexpr base::TimeDelta kAsyncHitTestTimeout = base::Seconds(5);

}  // namespace

RenderWidgetTargetResult::RenderWidgetTargetResult() = default;

RenderWidgetTargetResult::RenderWidgetTargetResult(
    const RenderWidgetTargetResult&) = default;

RenderWidgetTargetResult::RenderWidgetTargetResult(
    RenderWidgetHostViewInput* in_view,
    bool in_should_query_view,
    std::optional<gfx::PointF> in_location,
    bool in_latched_target)
    : view(in_view),
      should_query_view(in_should_query_view),
      target_location(in_location),
      latched_target(in_latched_target) {}

RenderWidgetTargetResult::~RenderWidgetTargetResult() = default;

RenderWidgetTargeter::TargetingRequest::TargetingRequest(
    base::WeakPtr<RenderWidgetHostViewInput> root_view,
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency) {
  this->root_view = std::move(root_view);
  this->location = ComputeEventLocation(event);
  this->event = event.Clone();
  this->latency = latency;
}

RenderWidgetTargeter::TargetingRequest::TargetingRequest(
    base::WeakPtr<RenderWidgetHostViewInput> root_view,
    const gfx::PointF& location,
    RenderWidgetHostAtPointCallback callback) {
  this->root_view = std::move(root_view);
  this->location = location;
  this->callback = std::move(callback);
}

RenderWidgetTargeter::TargetingRequest::TargetingRequest(
    TargetingRequest&& request) = default;

RenderWidgetTargeter::TargetingRequest& RenderWidgetTargeter::TargetingRequest::
operator=(TargetingRequest&&) = default;

RenderWidgetTargeter::TargetingRequest::~TargetingRequest() = default;

void RenderWidgetTargeter::TargetingRequest::RunCallback(
    RenderWidgetHostViewInput* target,
    std::optional<gfx::PointF> point) {
  if (!callback.is_null()) {
    std::move(callback).Run(target ? target->GetInputWeakPtr() : nullptr,
                            point);
  }
}

bool RenderWidgetTargeter::TargetingRequest::MergeEventIfPossible(
    const blink::WebInputEvent& new_event) {
  if (event && !blink::WebInputEvent::IsTouchEventType(new_event.GetType()) &&
      !blink::WebInputEvent::IsGestureEventType(new_event.GetType()) &&
      event->CanCoalesce(new_event)) {
    event->Coalesce(new_event);
    return true;
  }
  return false;
}

bool RenderWidgetTargeter::TargetingRequest::IsWebInputEventRequest() const {
  return !!event;
}

blink::WebInputEvent* RenderWidgetTargeter::TargetingRequest::GetEvent() {
  return event.get();
}

RenderWidgetHostViewInput*
RenderWidgetTargeter::TargetingRequest::GetRootView() const {
  return root_view.get();
}

gfx::PointF RenderWidgetTargeter::TargetingRequest::GetLocation() const {
  return location;
}

const ui::LatencyInfo& RenderWidgetTargeter::TargetingRequest::GetLatency()
    const {
  return latency;
}

RenderWidgetTargeter::RenderWidgetTargeter(Delegate* delegate)
    : async_hit_test_timeout_delay_(kAsyncHitTestTimeout),
      trace_id_(base::RandUint64()),
      delegate_(delegate) {
  DCHECK(delegate_);
}

RenderWidgetTargeter::~RenderWidgetTargeter() = default;

void RenderWidgetTargeter::FindTargetAndDispatch(
    RenderWidgetHostViewInput* root_view,
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency) {
  DCHECK(blink::WebInputEvent::IsMouseEventType(event.GetType()) ||
         event.GetType() == blink::WebInputEvent::Type::kMouseWheel ||
         blink::WebInputEvent::IsTouchEventType(event.GetType()) ||
         (blink::WebInputEvent::IsGestureEventType(event.GetType()) &&
          (static_cast<const blink::WebGestureEvent&>(event).SourceDevice() ==
               blink::WebGestureDevice::kTouchscreen ||
           static_cast<const blink::WebGestureEvent&>(event).SourceDevice() ==
               blink::WebGestureDevice::kTouchpad)));

  if (!requests_.empty()) {
    auto& request = requests_.back();
    if (request.MergeEventIfPossible(event))
      return;
  }

  TargetingRequest request(root_view->GetInputWeakPtr(), event, latency);

  ResolveTargetingRequest(std::move(request));
}

void RenderWidgetTargeter::FindTargetAndCallback(
    RenderWidgetHostViewInput* root_view,
    const gfx::PointF& point,
    RenderWidgetHostAtPointCallback callback) {
  TargetingRequest request(root_view->GetInputWeakPtr(), point,
                           std::move(callback));

  ResolveTargetingRequest(std::move(request));
}

void RenderWidgetTargeter::ResolveTargetingRequest(TargetingRequest request) {
  if (request_in_flight_) {
    requests_.push(std::move(request));
    return;
  }

  RenderWidgetTargetResult result;
  auto* request_target = request.GetRootView();
  auto request_target_location = request.GetLocation();

  if (request.IsWebInputEventRequest()) {
    result = is_autoscroll_in_progress_
                 ? middle_click_result_
                 : delegate_->FindTargetSynchronously(request_target,
                                                      *request.GetEvent());
    // |result.target_location| is utilized to update the position in widget for
    // an event. If we are in autoscroll mode, we used cached data. So we need
    // to update the target location of the |result|.
    if (is_autoscroll_in_progress_) {
      result.target_location = request_target_location;
    }

    if (!is_autoscroll_in_progress_ &&
        IsMouseMiddleClick(*request.GetEvent())) {
      if (!result.should_query_view)
        middle_click_result_ = result;
    }
  } else {
    result = delegate_->FindTargetSynchronouslyAtPoint(request_target,
                                                       request_target_location);
  }
  RenderWidgetHostViewInput* target = result.view;
  if (!is_autoscroll_in_progress_ && result.should_query_view) {
    TRACE_EVENT_WITH_FLOW2(
        "viz,benchmark", "Event.Pipeline", TRACE_ID_GLOBAL(trace_id_),
        TRACE_EVENT_FLAG_FLOW_OUT, "step", "QueryClient(Start)",
        "event_location", request.GetLocation().ToString());

    // TODO(kenrb, sadrul): When all event types support asynchronous hit
    // testing, we should be able to have FindTargetSynchronously return the
    // view and location to use for the renderer hit test query.
    // Currently it has to return the surface hit test target, for event types
    // that ignore |result.should_query_view|, and therefore we have to use
    // root_view and the original event location for the initial query.
    // Do not compare hit test results if we are forced to do async hit testing
    // by HitTestQuery.
    QueryClient(request_target, request_target_location, nullptr, gfx::PointF(),
                std::move(request));
  } else {
    FoundTarget(target, result.target_location, &request);
  }
}

void RenderWidgetTargeter::ViewWillBeDestroyed(
    RenderWidgetHostViewInput* view) {
  unresponsive_views_.erase(view);

  if (is_autoscroll_in_progress_ && middle_click_result_.view == view) {
    SetIsAutoScrollInProgress(false);
  }
}

bool RenderWidgetTargeter::HasEventsPendingDispatch() const {
  return request_in_flight_ || !requests_.empty();
}

void RenderWidgetTargeter::SetIsAutoScrollInProgress(
    bool autoscroll_in_progress) {
  is_autoscroll_in_progress_ = autoscroll_in_progress;

  // If middle click autoscroll ends, reset |middle_click_result_|.
  if (!autoscroll_in_progress)
    middle_click_result_ = RenderWidgetTargetResult();
}

void RenderWidgetTargeter::QueryClient(
    RenderWidgetHostViewInput* target,
    const gfx::PointF& target_location,
    RenderWidgetHostViewInput* last_request_target,
    const gfx::PointF& last_target_location,
    TargetingRequest request) {
  auto& target_client =
      target->GetViewRenderInputRouter()->input_target_client();
  // |target_client| may not be set yet for this |target| on Mac, need to
  // understand why this happens. https://crbug.com/859492.
  // We do not verify hit testing result under this circumstance.
  if (!target_client.is_bound() || !target_client.is_connected()) {
    FoundTarget(target, target_location, &request);
    return;
  }

  const gfx::PointF location = request.GetLocation();

  request_in_flight_ = std::move(request);

  async_hit_test_timeout_.Start(
      FROM_HERE, async_hit_test_timeout_delay_,
      base::BindOnce(&RenderWidgetTargeter::AsyncHitTestTimedOut,
                     weak_ptr_factory_.GetWeakPtr(), target->GetInputWeakPtr(),
                     target_location,
                     last_request_target
                         ? last_request_target->GetInputWeakPtr()
                         : nullptr,
                     last_target_location));

  target_client.set_disconnect_handler(
      base::BindOnce(&RenderWidgetTargeter::OnInputTargetDisconnect,
                     weak_ptr_factory_.GetWeakPtr(), target->GetInputWeakPtr(),
                     target_location));

  TRACE_EVENT_WITH_FLOW2(
      "viz,benchmark", "Event.Pipeline", TRACE_ID_GLOBAL(trace_id_),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "QueryClient", "event_location", location.ToString());

  target_client->FrameSinkIdAt(
      target_location, trace_id_,
      base::BindOnce(&RenderWidgetTargeter::FoundFrameSinkId,
                     weak_ptr_factory_.GetWeakPtr(), target->GetInputWeakPtr(),
                     ++last_request_id_, target_location));
}

void RenderWidgetTargeter::FlushEventQueue() {
  bool events_being_flushed = false;
  while (!request_in_flight_ && !requests_.empty()) {
    auto request = std::move(requests_.front());
    requests_.pop();
    // The root-view has gone away. Ignore this event, and try to process the
    // next event.
    if (!request.GetRootView())
      continue;

    // Only notify the delegate once that the current event queue is being
    // flushed. Once all the events are flushed, notify the delegate again.
    if (!events_being_flushed) {
      delegate_->SetEventsBeingFlushed(true);
      events_being_flushed = true;
    }
      ResolveTargetingRequest(std::move(request));
  }
    delegate_->SetEventsBeingFlushed(false);
}

void RenderWidgetTargeter::FoundFrameSinkId(
    base::WeakPtr<RenderWidgetHostViewInput> target,
    uint32_t request_id,
    const gfx::PointF& target_location,
    const viz::FrameSinkId& frame_sink_id,
    const gfx::PointF& transformed_location) {
  if (!target) {
    return;
  }

  uint32_t last_id = last_request_id_;
  bool in_flight = request_in_flight_.has_value();
  if (request_id != last_id || !in_flight) {
    // This is a response to a request that already timed out, so the event
    // should have already been dispatched. Mark the renderer as responsive
    // and otherwise ignore this response.
    unresponsive_views_.erase(target.get());
    return;
  }

  TargetingRequest request = std::move(request_in_flight_.value());

  request_in_flight_.reset();
  async_hit_test_timeout_.Stop();
  target->GetViewRenderInputRouter()
      ->input_target_client()
      .set_disconnect_handler(base::OnceClosure());

  auto* view = delegate_->FindViewFromFrameSinkId(frame_sink_id);
  if (!view) {
    view = target.get();
  }

  // If a client returned an embedded target, then it might be necessary to
  // continue asking the clients until a client claims an event for itself.
  if (view == target.get() ||
      unresponsive_views_.find(view) != unresponsive_views_.end() ||
      !delegate_->ShouldContinueHitTesting(view)) {
    // Reduced scope is required since FoundTarget can trigger another query
    // which would end up linked to the current query.
    {
      TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Event.Pipeline",
                             TRACE_ID_GLOBAL(trace_id_),
                             TRACE_EVENT_FLAG_FLOW_IN, "step", "FoundTarget");
    }

    if (request.IsWebInputEventRequest() &&
        IsMouseMiddleClick(*request.GetEvent())) {
      middle_click_result_ = {view, false, transformed_location, false};
    }

    FoundTarget(view, transformed_location, &request);
  } else {
    QueryClient(view, transformed_location, target.get(), target_location,
                std::move(request));
  }
}

void RenderWidgetTargeter::FoundTarget(
    RenderWidgetHostViewInput* target,
    const std::optional<gfx::PointF>& target_location,
    TargetingRequest* request) {
  DCHECK(request);
  // RenderWidgetHostViewMac can be deleted asynchronously, in which case the
  // View will be valid but there will no longer be a RenderWidgetHostImpl.
  if (!request->GetRootView() ||
      !request->GetRootView()->GetViewRenderInputRouter()) {
    return;
  }

  if (request->IsWebInputEventRequest()) {
    delegate_->DispatchEventToTarget(request->GetRootView(), target,
                                     request->GetEvent(), request->GetLatency(),
                                     target_location);
  } else {
    request->RunCallback(target, target_location);
  }

  FlushEventQueue();
}

void RenderWidgetTargeter::AsyncHitTestTimedOut(
    base::WeakPtr<RenderWidgetHostViewInput> current_request_target,
    const gfx::PointF& current_target_location,
    base::WeakPtr<RenderWidgetHostViewInput> last_request_target,
    const gfx::PointF& last_target_location) {
  DCHECK(request_in_flight_);

  TargetingRequest request = std::move(request_in_flight_.value());
  request_in_flight_.reset();

  if (!request.GetRootView())
    return;

  if (current_request_target) {
    // Mark view as unresponsive so further events will not be sent to it.
    unresponsive_views_.insert(current_request_target.get());

    // Reset disconnect handler for view.
    current_request_target->GetViewRenderInputRouter()
        ->input_target_client()
        .set_disconnect_handler(base::OnceClosure());
  }

  if (request.GetRootView() == current_request_target.get()) {
    // When a request to the top-level frame times out then the event gets
    // sent there anyway. It will trigger the hung renderer dialog if the
    // renderer fails to process it.
    FoundTarget(current_request_target.get(), current_target_location,
                &request);
  } else {
    FoundTarget(last_request_target.get(), last_target_location, &request);
  }
}

void RenderWidgetTargeter::OnInputTargetDisconnect(
    base::WeakPtr<RenderWidgetHostViewInput> target,
    const gfx::PointF& location) {
  if (!async_hit_test_timeout_.IsRunning())
    return;

  async_hit_test_timeout_.Stop();
  TargetingRequest request = std::move(request_in_flight_.value());
  request_in_flight_.reset();

  // Since we couldn't find the target frame among the child-frames
  // we process the event in the current frame.
  FoundTarget(target.get(), location, &request);
}

}  // namespace input
