// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_deadline_decider.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"

namespace viz {

FrameDeadlineDecider::FrameDeadlineDecider(
    bool use_platform_preferred_deadlines)
    : max_non_interactive_idle_duration_(
#if BUILDFLAG(IS_ANDROID)
          features::kAndroidCustomFrameDeadlineMaxNonInteractiveIdleDuration
              .Get()
#else
          base::Milliseconds(50)
#endif
              ),
      max_interactive_idle_duration_(
#if BUILDFLAG(IS_ANDROID)
          features::kAndroidCustomFrameDeadlineMaxInteractionIdleDuration.Get()
#else
          base::Seconds(3)
#endif
              ),
      use_platform_preferred_deadlines_(use_platform_preferred_deadlines) {
}

FrameDeadlineDecider::~FrameDeadlineDecider() = default;

void FrameDeadlineDecider::NotifyMinSupportedVsyncInterval(
    base::TimeDelta min_vsync_interval) {
  min_supported_vsync_interval_ = min_vsync_interval;
}

bool FrameDeadlineDecider::IsPartOfOngoingFrameSequence(
    base::TimeTicks frame_time,
    bool is_handling_interaction) const {
  if (!frame_sequence_state_.has_value()) {
    return false;
  }
  // The first frame in an interaction sequence uses non-interactive idle time
  // to ensure any preceding idle gap resets the sequence state.
  const bool is_ongoing_interaction =
      is_handling_interaction && frame_sequence_state_->is_interaction_active;
  const base::TimeDelta timeout = is_ongoing_interaction
                                      ? max_interactive_idle_duration_
                                      : max_non_interactive_idle_duration_;
  const base::TimeDelta time_since_last_frame =
      frame_time - frame_sequence_state_->last_frame_time;
  return time_since_last_frame <= timeout;
}

FrameDeadlineDecider::QueryResult FrameDeadlineDecider::QueryDeadline(
    const PossibleDeadlines& possible_deadlines,
    base::TimeDelta vsync_interval,
    int max_allowed_buffers,
    base::TimeTicks frame_time,
    std::optional<base::TimeTicks> earliest_input_time,
    bool is_handling_interaction) const {
  CHECK(!possible_deadlines.deadlines.empty());

  if (use_platform_preferred_deadlines_) {
    return {possible_deadlines.os_preferred_index,
            SelectionReason::kPlatformPreferred};
  }

  if (IsPartOfOngoingFrameSequence(frame_time, is_handling_interaction)) {
    return {FindClosestDeadlineByPresentation(possible_deadlines),
            SelectionReason::kOngoingSequence};
  }

  int presentation_offset = 0;
#if BUILDFLAG(IS_ANDROID)
  presentation_offset =
      features::kAndroidCustomFrameDeadlinePresentationOffset.Get();
#endif  // BUILDFLAG(IS_ANDROID)

  // num_buffers * vsync_interval is the maximum presentation interval we would
  // want to target. Since going beyond this threshold means frames would now
  // start stalling for long time, waiting for buffers to be freed. Thus,
  // `presentation_offset` is expected to be non-positive (<= 0).
  int target_present_multiplier = max_allowed_buffers + presentation_offset;
  CHECK_GT(target_present_multiplier, 0);
  base::TimeDelta target_present_delta =
      target_present_multiplier * vsync_interval;

  // Always cap custom presentation deltas so that an imminent switch to
  // higher refresh rates never exceeds the display's maximum sustainable
  // presentation delta.
  base::TimeDelta min_interval_presentation_cap =
      min_supported_vsync_interval_.has_value()
          ? max_allowed_buffers * (*min_supported_vsync_interval_)
          : base::TimeDelta::Max();
  target_present_delta =
      std::min(target_present_delta, min_interval_presentation_cap);

  if (earliest_input_time.has_value()) {
    // The earliest input time can be in the future relative to frame_time
    // in cases like WaitForLateScroll where we wait for input events to
    // arrive after the begin frame is sent. Clamp to 0 in such cases.
    const base::TimeDelta input_delta =
        std::max(base::TimeDelta(), frame_time - *earliest_input_time);
    // We subtract 1.25 * vsync_interval from the perceptible latency threshold
    // to allow a safety buffer for potential OS side frame jank.
    const base::TimeDelta latency_cap =
        kPerceptibleLatencyThreshold - vsync_interval - (vsync_interval / 4);
    const base::TimeDelta max_present_delta = latency_cap - input_delta;
    if (max_present_delta < target_present_delta) {
      // Reduce target presentation delta to pull the deadline earlier and
      // satisfy the perceptible input-latency threshold constraints.
      target_present_delta = max_present_delta;
    }
  }

  auto it = std::upper_bound(
      possible_deadlines.deadlines.begin(), possible_deadlines.deadlines.end(),
      target_present_delta,
      [](base::TimeDelta target, const PossibleDeadline& deadline) {
        return target < deadline.present_delta;
      });

  if (it != possible_deadlines.deadlines.begin()) {
    --it;
  }

  const size_t chrome_preferred_index =
      std::distance(possible_deadlines.deadlines.begin(), it);
  const PossibleDeadline& chrome_preferred_deadline = *it;

  if (chrome_preferred_deadline.present_delta > target_present_delta) {
    return {possible_deadlines.os_preferred_index,
            SelectionReason::kOsPreferredNoDeadlineWithinTarget};
  }

  if (chrome_preferred_deadline.present_delta <
      possible_deadlines.GetOSPreferredDeadline().present_delta) {
    // Fallback to os preferred deadline instead of reducing the preferred
    // deadline. We are not sure if this would actually happen in field.
    return {possible_deadlines.os_preferred_index,
            SelectionReason::kOsPreferredChromePreferredSooner};
  }

  return {chrome_preferred_index, SelectionReason::kChromePreferredNewSequence};
}

size_t FrameDeadlineDecider::SelectDeadline(
    const PossibleDeadlines& possible_deadlines,
    base::TimeDelta vsync_interval,
    int max_allowed_buffers,
    base::TimeTicks frame_time,
    std::optional<base::TimeTicks> earliest_input_time,
    bool is_handling_interaction) {
  TRACE_EVENT_BEGIN("toplevel,graphics.pipeline,viz",
                    "FrameDeadlineDecider::SelectDeadline");

  QueryResult result =
      QueryDeadline(possible_deadlines, vsync_interval, max_allowed_buffers,
                    frame_time, earliest_input_time, is_handling_interaction);
  const auto& selected_deadline =
      possible_deadlines.deadlines[result.deadline_index];
  UMA_HISTOGRAM_ENUMERATION("Viz.FrameDeadlineDecider.SelectionReason",
                            result.reason);

  frame_sequence_state_ = FrameSequenceState{
      .present_delta = selected_deadline.present_delta,
      .deadline_index = result.deadline_index,
      .last_frame_time = frame_time,
      .is_interaction_active = is_handling_interaction,
  };
  RecordSelectedSustainableDeadlineHistogram(
      selected_deadline.present_delta, vsync_interval, max_allowed_buffers);
  TRACE_EVENT_END(
      "toplevel,graphics.pipeline,viz", [&](perfetto::EventContext ctx) {
        auto* data = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                         ->set_android_choreographer_frame_callback_data();
        auto frame_time_us = frame_time.since_origin().InMicroseconds();
        data->set_frame_time_us(frame_time_us);
        auto* timeline = data->set_chrome_preferred_frame_timeline();
        selected_deadline.SetTraceTimelineData(*timeline);
      });

  return result.deadline_index;
}

void FrameDeadlineDecider::OnDisplayInvisible() {
  frame_sequence_state_.reset();
}

size_t FrameDeadlineDecider::FindClosestDeadlineByPresentation(
    const PossibleDeadlines& possible_deadlines) const {
  // Check if the cached index is valid and within 1ms of target.
  if (frame_sequence_state_->deadline_index <
      possible_deadlines.deadlines.size()) {
    const auto& cached_deadline =
        possible_deadlines.deadlines[frame_sequence_state_->deadline_index];
    if ((cached_deadline.present_delta - frame_sequence_state_->present_delta)
            .magnitude() <= base::Milliseconds(1)) {
      return frame_sequence_state_->deadline_index;
    }
  }

  // We are trying to uphold the presentation deadline being used by the
  // previous frame in the sequence. Initializing the search with the 0th index
  // is perfectly fine for the baseline comparison.
  size_t best_index = 0;
  base::TimeDelta min_diff = (possible_deadlines.deadlines[0].present_delta -
                              frame_sequence_state_->present_delta)
                                 .magnitude();

  // Possible deadlines are guaranteed to be in chronological order from
  // Android.
  for (size_t i = 1; i < possible_deadlines.deadlines.size(); ++i) {
    const auto& deadline = possible_deadlines.deadlines[i];
    base::TimeDelta diff =
        (deadline.present_delta - frame_sequence_state_->present_delta)
            .magnitude();
    if (diff < min_diff) {
      min_diff = diff;
      best_index = i;
    }
  }
  return best_index;
}

void FrameDeadlineDecider::RecordSelectedSustainableDeadlineHistogram(
    base::TimeDelta selected_present_delta,
    base::TimeDelta vsync_interval,
    int max_allowed_buffers) const {
  // A presentation deadline is sustainable if its present delta does not exceed
  // the total time spanned by the allowed buffer queue (`max_allowed_buffers *
  // vsync_interval`). Selecting a target beyond this threshold requires
  // queueing more buffers in flight than allowed, causing multiple-vsync swap
  // throttling (`swaps throttled`) and pipeline stalls.
  const base::TimeDelta max_sustainable_delta =
      (max_allowed_buffers * vsync_interval) + base::Milliseconds(1);
  const bool is_sustainable = selected_present_delta <= max_sustainable_delta;
  UMA_HISTOGRAM_BOOLEAN("Viz.FrameDeadlineDecider.SelectedSustainableDeadline",
                        is_sustainable);
}

}  // namespace viz
