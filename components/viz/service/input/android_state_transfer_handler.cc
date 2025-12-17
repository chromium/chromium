// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/android_state_transfer_handler.h"

#include <utility>

#include "base/check_deref.h"
#include "base/notreached.h"
#include "components/input/features.h"
#include "components/viz/service/input/input_on_viz_state_processing_result.h"
#include "ui/events/android/events_android_utils.h"
#include "ui/events/android/motion_event_android_factory.h"

namespace viz {

namespace {

base::TimeTicks GetEventDowntime(const base::android::ScopedInputEvent& event) {
  return base::TimeTicks::FromUptimeMillis(
      AMotionEvent_getDownTime(event.a_input_event()) /
      base::Time::kNanosecondsPerMillisecond);
}

// LINT.IfChange(VizSequenceDroppedReason)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VizSequenceDroppedReason {
  kOlderSequenceInQueue = 0,
  kMaxValue = kOlderSequenceInQueue,
};

// LINT.ThenChange(
//     //tools/metrics/histograms/metadata/android/enums.xml:VizSequenceDroppedReason,
//     //base/tracing/protos/chrome_track_event.proto:VizSequenceDroppedReason)

// LINT.IfChange(DroppedSequenceEventAndDownTimeDelta)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DroppedSequenceEventAndDownTimeDelta {
  kEventTimeLessThanDownTime = 0,
  kEventTimeEqualsDownTime = 1,
  kEventTimeGreaterThanDownTime = 2,
  kMaxValue = kEventTimeGreaterThanDownTime,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:DroppedSequenceEventAndDownTimeDelta)

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
    AndroidStateTransferHandlerClient& client,
    VizTouchStateHandler* viz_touch_state_handler)
    : client_(client), viz_touch_state_handler_(viz_touch_state_handler) {}

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

  if (state_received_out_of_order) {
    // We don't expect to receive `StateOnTouchTransfer` mojo calls coming out
    // of order. But it's possible the timestamps provided by Android platform
    // are the issue.
    TRACE_EVENT_INSTANT("viz", "OutOfOrderTransferStateDropped");
    EmitStateProcessingResultHistogram(
        InputOnVizStateProcessingResult::kDroppedOutOfOrderDownTime);
    return;
  }

  MaybeDropEventsFromEarlierSequences(state);

  pending_transferred_states_.emplace(rir_support, std::move(state));
  if (pending_transferred_states_.size() > kMaxPendingTransferredStates) {
    EmitStateProcessingResultHistogram(
        InputOnVizStateProcessingResult::kDroppedTooManyPendingStates);
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

  // Viz only handles touch events, actions like button press/release are not
  // supported and should ideally not be arriving.
  if (!IsExpectedMotionEventAction(action)) {
    return true;
  }

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

bool AndroidStateTransferHandler::IsExpectedMotionEventAction(int action) {
  switch (action) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_MOVE:
    case AMOTION_EVENT_ACTION_CANCEL:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_UP:
      return true;
    default:
      break;
  }

  base::UmaHistogramEnumeration(kDroppedNonTouchActions,
                                ui::FromAndroidAction(action));
  return false;
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
    EmitStateProcessingResultHistogram(
        InputOnVizStateProcessingResult::kDroppedUnusedOlderStates);
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
      viz_touch_state_handler_->UpdateLastTransferredBackDownTimeMs(
          state.transfer_state->down_time_ms.ToUptimeMillis());
      if (client_->TransferInputBackToBrowser()) {
        EmitStateProcessingResultHistogram(
            InputOnVizStateProcessingResult::
                kTransferBackToBrowserSuccessfully);
      } else {
        EmitStateProcessingResultHistogram(
            InputOnVizStateProcessingResult::
                kDroppedTransferBackToBrowserFailed);
      }
      ignore_remaining_touch_sequence_ = true;
    } else {
      // Reset the last_transferred_back_down_time_ms since Viz is handling
      // this new sequence.
      viz_touch_state_handler_->UpdateLastTransferredBackDownTimeMs(0);
      EmitStateProcessingResultHistogram(
          InputOnVizStateProcessingResult::kProcessedSuccessfully);
    }
    state_for_curr_sequence_.emplace(std::move(state));
    pending_transferred_states_.pop();
    if (input::features::kForwardEventsSeenOnBrowserToViz.Get()) {
      HandleFirstDownEvent();
    }
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
    const int action =
        AMotionEvent_getAction(events_buffer_.front().a_input_event()) &
        AMOTION_EVENT_ACTION_MASK;
    if (action == AMOTION_EVENT_ACTION_DOWN) {
      constexpr VizSequenceDroppedReason reason =
          VizSequenceDroppedReason::kOlderSequenceInQueue;
      TRACE_EVENT_INSTANT(
          "input,input.scrolling", "SequenceDropped",
          [&](perfetto::EventContext ctx) {
            auto* event =
                ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
            auto* transfer_handler = event->set_input_transfer_handler();
            int dropped_reason_int = static_cast<int>(reason);
            // Increment by 1 to convert from histogram to proto enum. The
            // perfetto's VizSequenceDroppedReason proto enum values are
            // incremented by 1 to leave 0 value for unknown/unset field.
            transfer_handler->set_viz_sequence_dropped_reason(
                static_cast<perfetto::protos::pbzero::InputTransferHandler::
                                VizSequenceDroppedReason>(dropped_reason_int +
                                                          1));
          });
      base::UmaHistogramEnumeration(
          "Android.InputOnViz.Viz.SequenceDroppedReason", reason);
      const int64_t event_time_nanos =
          AMotionEvent_getEventTime(events_buffer_.front().a_input_event());
      const int64_t down_time_nanos =
          AMotionEvent_getDownTime(events_buffer_.front().a_input_event());

      DroppedSequenceEventAndDownTimeDelta delta;
      if (event_time_nanos < down_time_nanos) {
        delta =
            DroppedSequenceEventAndDownTimeDelta::kEventTimeLessThanDownTime;
      } else if (event_time_nanos == down_time_nanos) {
        delta = DroppedSequenceEventAndDownTimeDelta::kEventTimeEqualsDownTime;
      } else {
        delta =
            DroppedSequenceEventAndDownTimeDelta::kEventTimeGreaterThanDownTime;
      }
      base::UmaHistogramEnumeration(
          "Android.InputOnViz.Viz.DroppedSequences.EventAndDownTimeDelta",
          delta);
    }
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

void AndroidStateTransferHandler::HandleFirstDownEvent() {
  CHECK(state_for_curr_sequence_.has_value());
  CHECK(input::features::kForwardEventsSeenOnBrowserToViz.Get());

  if (!state_for_curr_sequence_->rir_support) {
    return;
  }
  CHECK(state_for_curr_sequence_->transfer_state->down_event);
  state_for_curr_sequence_->rir_support->OnTouchEvent(
      *(state_for_curr_sequence_->transfer_state->down_event->get()),
      /* emit_histograms= */ true);
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
    ignore_remaining_touch_sequence_ = true;
  }

  if (ignore_remaining_touch_sequence_) {
    if (action == AMOTION_EVENT_ACTION_CANCEL ||
        action == AMOTION_EVENT_ACTION_UP) {
      state_for_curr_sequence_.reset();
      ignore_remaining_touch_sequence_ = false;
    }
    return;
  }

  std::optional<ui::MotionEventAndroid::EventTimes> event_times = std::nullopt;
  if (action == AMOTION_EVENT_ACTION_DOWN) {
    if (input::features::kForwardEventsSeenOnBrowserToViz.Get()) {
      // Ignore the down event sent by system, since the down event received on
      // Browser is transferred to Viz and processed along with state.
      return;
    }
    event_times = ui::MotionEventAndroid::EventTimes();
    // AMotionEvent_getDownTime returns down time in nanoseconds precision.
    event_times->latest = base::TimeTicks::FromJavaNanoTime(
        AMotionEvent_getDownTime(input_event.a_input_event()));
    event_times->oldest = event_times->latest;
  }
  auto event = ui::MotionEventAndroidFactory::CreateFromNative(
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
