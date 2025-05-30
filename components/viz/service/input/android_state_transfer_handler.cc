// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/android_state_transfer_handler.h"

#include <utility>

#include "base/check_deref.h"
#include "base/notreached.h"
#include "ui/events/android/motion_event_android_native.h"

namespace viz {

namespace {

base::TimeTicks GetEventDowntime(const base::android::ScopedInputEvent& event) {
  return base::TimeTicks::FromUptimeMillis(
      AMotionEvent_getDownTime(event.a_input_event()) /
      base::Time::kNanosecondsPerMillisecond);
}

}  // namespace

AndroidStateTransferHandler::TransferState::TransferState(
    base::WeakPtr<RenderInputRouterSupportAndroidInterface> support,
    input::mojom::TouchTransferStatePtr state)
    : rir_support(support), transfer_state(std::move(state)) {
  CHECK(transfer_state);
}

AndroidStateTransferHandler::TransferState::~TransferState() = default;

AndroidStateTransferHandler::TransferState::TransferState(
    TransferState&& other) {
  rir_support = other.rir_support;
  other.rir_support = nullptr;
  transfer_state = std::move(other.transfer_state);
}

AndroidStateTransferHandler::AndroidStateTransferHandler(
    AndroidStateTransferHandlerClient& client)
    : client_(client) {}

AndroidStateTransferHandler::~AndroidStateTransferHandler() = default;

void AndroidStateTransferHandler::StateOnTouchTransfer(
    input::mojom::TouchTransferStatePtr state,
    base::WeakPtr<RenderInputRouterSupportAndroidInterface> rir_support) {
  TRACE_EVENT("viz", "AndroidStateTransferHandler::StateOnTouchTransfer");

  EmitPendingTransfersHistogram();

  const bool state_received_out_of_order =
      (!pending_transferred_states_.empty() &&
       (state->down_time_ms <
        pending_transferred_states_.back().transfer_state->down_time_ms));

  CHECK(!state_received_out_of_order);

  MaybeDropEventsFromEarlierSequences(state);

  pending_transferred_states_.emplace(rir_support, std::move(state));
  if (pending_transferred_states_.size() > kMaxPendingTransferredStates) {
    pending_transferred_states_.pop();
  }

  if (events_buffer_.empty()) {
    return;
  }

  if (CanStartProcessingVizEvents(events_buffer_.front())) {
    while (!events_buffer_.empty()) {
      if (!state_for_curr_sequence_.has_value()) {
        // This can happen if the whole sequence was received on Viz before the
        // state could be transferred from Browser. Early out since we wouldn't
        // want to process the events from next sequence if they made it to
        // queue as well. `state_for_curr_sequence_` is reset at the end of
        // touch sequence when touch up or cancel is seen in `HandleTouchEvent`
        // method.
        return;
      }
      HandleTouchEvent(std::move(events_buffer_.front()));
      events_buffer_.pop();
    }
  }
}

bool AndroidStateTransferHandler::OnMotionEvent(
    base::android::ScopedInputEvent input_event,
    const FrameSinkId& root_frame_sink_id) {
  TRACE_EVENT("input", "AndroidStateTransferHandler::OnMotionEvent",
              [&](perfetto::EventContext& ctx) {
                auto* chrome_track_event =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
                auto* forwarder = chrome_track_event->set_event_forwarder();

                input_event.WriteIntoTrace(ctx.Wrap(forwarder));
              });

  const int action = AMotionEvent_getAction(input_event.a_input_event()) &
                     AMOTION_EVENT_ACTION_MASK;
  if (ignore_remaining_touch_sequence_) {
    if (action == AMOTION_EVENT_ACTION_CANCEL ||
        action == AMOTION_EVENT_ACTION_UP) {
      ignore_remaining_touch_sequence_ = false;
      state_for_curr_sequence_.reset();
    }
    return true;
  }

  if (state_for_curr_sequence_.has_value() ||
      CanStartProcessingVizEvents(input_event)) {
    HandleTouchEvent(std::move(input_event));
    return true;
  }

  bool events_from_dropped_sequence =
      !pending_transferred_states_.empty() &&
      (GetEventDowntime(input_event) <
       pending_transferred_states_.front().transfer_state->down_time_ms);
  if (events_from_dropped_sequence) {
    return true;
  }

  // Queue events until we can start processing events received directly on
  // Viz.
  // TODO(crbug.com/384424270): Coalesce touch moves instead of pushing
  // individual events to queue.
  events_buffer_.push(std::move(input_event));

  // Always return true since we are receiving input on Viz after hit testing on
  // Browser already determined that web contents are being hit.
  return true;
}

bool AndroidStateTransferHandler::CanStartProcessingVizEvents(
    const base::android::ScopedInputEvent& event) {
  CHECK(!state_for_curr_sequence_.has_value());

  const jlong j_event_down_time =
      base::TimeTicks::FromJavaNanoTime(
          AMotionEvent_getDownTime(event.a_input_event()))
          .ToUptimeMillis();
  base::TimeTicks event_down_time =
      base::TimeTicks::FromUptimeMillis(j_event_down_time);

  // Drop states with smaller down time i.e. the states corresponding to pointer
  // down events.
  while (!pending_transferred_states_.empty() &&
         (pending_transferred_states_.front().transfer_state->down_time_ms <
          event_down_time)) {
    pending_transferred_states_.pop();
  }

  if (pending_transferred_states_.empty()) {
    return false;
  }

  auto& state = pending_transferred_states_.front();
  // Touch event corresponding to previous state transfer should be
  // processed before next sequence starts.
  if (event_down_time == state.transfer_state->down_time_ms) {
    if (state.transfer_state->browser_would_have_handled) {
      client_->TransferInputBackToBrowser();
      ignore_remaining_touch_sequence_ = true;
    }
    state_for_curr_sequence_.emplace(std::move(state));
    pending_transferred_states_.pop();
    return true;
  }
  return false;
}

void AndroidStateTransferHandler::MaybeDropEventsFromEarlierSequences(
    const input::mojom::TouchTransferStatePtr& state) {
  if (events_buffer_.empty()) {
    return;
  }
  while (!events_buffer_.empty() &&
         GetEventDowntime(events_buffer_.front()) < state->down_time_ms) {
    events_buffer_.pop();
  }
}

void AndroidStateTransferHandler::EmitPendingTransfersHistogram() {
  const char* histogram_name;
  if (state_for_curr_sequence_.has_value()) {
    histogram_name = kPendingTransfersHistogramNonNull;
  } else {
    histogram_name = kPendingTransfersHistogramNull;
  }
  // We don't expect histogram value i.e. `pending_transferred_states_.size()`
  // to be more than 3(`kMaxPendingTransferredStates`, but leaving histogram max
  // to 10 to have some space to increase `kMaxPendingTransferredStates`.
  base::UmaHistogramCustomCounts(histogram_name,
                                 pending_transferred_states_.size(), /*min=*/1,
                                 /*exclusive_max=*/10, /*buckets=*/10u);
}

void AndroidStateTransferHandler::HandleTouchEvent(
    base::android::ScopedInputEvent input_event) {
  // TODO(crbug.com/406986388) : Add flow events to track the events starting
  // from when they were first were processed by Viz.
  TRACE_EVENT("input", "AndroidStateTransferHandler::HandleTouchEvent");
  CHECK(state_for_curr_sequence_.has_value());
  const int action = AMotionEvent_getAction(input_event.a_input_event()) &
                     AMOTION_EVENT_ACTION_MASK;

  if (GetEventDowntime(input_event) !=
      state_for_curr_sequence_->transfer_state->down_time_ms) {
    TRACE_EVENT_INSTANT("input,input.scrolling", "DifferentDownTimeInSequence");
  }

  if (!state_for_curr_sequence_->rir_support) {
    if (action == AMOTION_EVENT_ACTION_CANCEL ||
        action == AMOTION_EVENT_ACTION_UP) {
      state_for_curr_sequence_.reset();
      ignore_remaining_touch_sequence_ = false;
    } else {
      ignore_remaining_touch_sequence_ = true;
    }
    return;
  }

  // Ignore any already queued events for the touch sequence. This can happen if
  // we are returning the sequence back to browser.
  if (ignore_remaining_touch_sequence_) {
    return;
  }

  std::optional<ui::MotionEventAndroidNative::EventTimes> event_times =
      std::nullopt;
  if (action == AMOTION_EVENT_ACTION_DOWN) {
    event_times = ui::MotionEventAndroidNative::EventTimes();
    // AMotionEvent_getDownTime returns down time in nanoseconds precision.
    event_times->latest = base::TimeTicks::FromJavaNanoTime(
        AMotionEvent_getDownTime(input_event.a_input_event()));
    event_times->oldest = event_times->latest;
  }
  auto event = ui::MotionEventAndroidNative::Create(
      std::move(input_event),
      1.f / state_for_curr_sequence_->transfer_state->dip_scale,
      state_for_curr_sequence_->transfer_state->web_contents_y_offset_pix,
      event_times);

  state_for_curr_sequence_->rir_support->OnTouchEvent(
      *event.get(), /* emit_histograms= */ true);

  if (event->GetAction() == ui::MotionEvent::Action::UP ||
      event->GetAction() == ui::MotionEvent::Action::CANCEL) {
    state_for_curr_sequence_.reset();
  }
}

}  // namespace viz
