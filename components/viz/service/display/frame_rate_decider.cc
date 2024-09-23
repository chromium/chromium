// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_rate_decider.h"

#include <algorithm>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/constants.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "media/filters/video_cadence_estimator.h"

namespace viz {
namespace {

// The minimum number of frames for which a frame interval preference should
// persist before we toggle to it. This is only applied when lowering the frame
// rate. If the new preference is higher than the current setting, it is applied
// immediately.
constexpr size_t kMinNumOfFramesToToggleInterval = 6;

bool AreAlmostEqual(base::TimeDelta a, base::TimeDelta b) {
  if (a.is_min() || b.is_min() || a.is_max() || b.is_max())
    return a == b;

  constexpr auto kMaxDelta = base::Milliseconds(0.5);
  return (a - b).magnitude() < kMaxDelta;
}

}  // namespace

FrameRateDecider::ScopedAggregate::ScopedAggregate(FrameRateDecider* decider)
    : decider_(decider) {
  decider_->StartAggregation();
}

FrameRateDecider::ScopedAggregate::~ScopedAggregate() {
  decider_->EndAggregation();
}

FrameRateDecider::FrameRateDecider(SurfaceManager* surface_manager,
                                   Client* client,
                                   bool hw_support_for_multiple_refresh_rates,
                                   bool output_surface_supports_set_frame_rate)
    : supported_intervals_{BeginFrameArgs::DefaultInterval()},
      min_num_of_frames_to_toggle_interval_(kMinNumOfFramesToToggleInterval),
      surface_manager_(surface_manager),
      client_(client),
      hw_support_for_multiple_refresh_rates_(
          hw_support_for_multiple_refresh_rates),
      output_surface_supports_set_frame_rate_(
          output_surface_supports_set_frame_rate) {
  surface_manager_->AddObserver(this);
}

FrameRateDecider::~FrameRateDecider() {
  surface_manager_->RemoveObserver(this);
}

void FrameRateDecider::SetSupportedFrameIntervals(
    base::flat_set<base::TimeDelta> supported_intervals) {
  DCHECK(!inside_surface_aggregation_);

  supported_intervals_ = std::move(supported_intervals);
  UpdatePreferredFrameIntervalIfNeeded();
}

void FrameRateDecider::OnSurfaceWillBeDrawn(Surface* surface) {
  // If there are multiple displays, we receive callbacks when a surface is
  // drawn on any of these displays. Ensure that we only update the internal
  // tracking when the Display corresponding to this decider is drawing.
  if (!inside_surface_aggregation_)
    return;

  if (!multiple_refresh_rates_supported())
    return;

  // Update the list of surfaces drawn in this frame along with the currently
  // active CompositorFrame. We use the list from the previous frame to track
  // which surfaces were updated in this display draw.
  const SurfaceId& surface_id = surface->surface_id();
  const uint64_t active_index = surface->GetActiveFrameIndex();

  auto it = current_surface_id_to_active_index_.find(surface_id);
  if (it == current_surface_id_to_active_index_.end()) {
    current_surface_id_to_active_index_[surface_id] = active_index;
  } else {
    DCHECK_EQ(it->second, active_index)
        << "Same display frame should not draw a surface with different "
           "CompositorFrames";
  }

  it = prev_surface_id_to_active_index_.find(surface_id);
  if (it == prev_surface_id_to_active_index_.end() ||
      it->second != active_index) {
    frame_sinks_updated_in_previous_frame_.insert(surface_id.frame_sink_id());
  }
  frame_sinks_drawn_in_previous_frame_.insert(surface_id.frame_sink_id());
}

void FrameRateDecider::StartAggregation() {
  DCHECK(!inside_surface_aggregation_);

  inside_surface_aggregation_ = true;
  frame_sinks_updated_in_previous_frame_.clear();
  frame_sinks_drawn_in_previous_frame_.clear();
}

void FrameRateDecider::EndAggregation() {
  DCHECK(inside_surface_aggregation_);

  inside_surface_aggregation_ = false;
  prev_surface_id_to_active_index_.swap(current_surface_id_to_active_index_);
  current_surface_id_to_active_index_.clear();

  UpdatePreferredFrameIntervalIfNeeded();
}

void FrameRateDecider::UpdatePreferredFrameIntervalIfNeeded() {
  if (!multiple_refresh_rates_supported())
    return;

  // Prepare the list of frame intervals from the frame sinks with a video(s),
  std::vector<base::TimeDelta> fixed_interval_frame_sink_intervals;
  for (const auto& frame_sink_id : frame_sinks_drawn_in_previous_frame_) {
    auto type = mojom::CompositorFrameSinkType::kUnspecified;
    auto interval =
        client_->GetPreferredFrameIntervalForFrameSinkId(frame_sink_id, &type);

    switch (type) {
      case mojom::CompositorFrameSinkType::kUnspecified:
        DCHECK_EQ(interval, BeginFrameArgs::MinInterval());
        continue;
      case mojom::CompositorFrameSinkType::kVideo:
        fixed_interval_frame_sink_intervals.push_back(interval);
        break;
      case mojom::CompositorFrameSinkType::kMediaStream:
        fixed_interval_frame_sink_intervals.push_back(interval);
        break;
      case mojom::CompositorFrameSinkType::kLayerTree:
        break;
    }
  }

  // From the list above, get the video case for lowering the frame rate.
  ToggleFrameRateCase toggle_case =
      GetToggleFrameRateCase(fixed_interval_frame_sink_intervals);
  bool should_toggle = true;
  switch (toggle_case) {
    // If there is no fixed rate content, we don't try to lower the frame rate.
    case ToggleFrameRateCase::kNone:
    // For single video cases, we only try to toggle framerate for perfect
    // cadence when |kSingleVideoFrameRateThrottling| is enabled.
    case ToggleFrameRateCase::kSingleVideoNoPerfectCadence:
    // If a video's framerate equals to the display framerate, there's no need
    // to toggle.
    case ToggleFrameRateCase::kSingleVideoPerfectCadenceMatchesDisplay:
      should_toggle = false;
      break;
    case ToggleFrameRateCase::kSingleVideoPerfectCadenceDiffersFromDisplay:
      should_toggle = base::FeatureList::IsEnabled(
          features::kSingleVideoFrameRateThrottling);
      break;
    // If lowering the refresh rate is supported by the platform then we try to
    // do this in all cases where any content drawing onscreen animates at a
    // fixed rate. This includes surfaces backed by videos or media streams.
    case ToggleFrameRateCase::kHardwareSupported:
    // If we're reducing frame rate for the display compositor, as opposed to
    // the underlying platform compositor or physical display, then restrict it
    // to cases with multiple animating sources that can be lowered. We should
    // be able to do it for all video cases but this results in dropped frame
    // regressions which need to be investigated (see crbug.com/976583).
    case ToggleFrameRateCase::kMultipleVideos:
      break;
  }
  if (toggle_case != ToggleFrameRateCase::kNone &&
      metrics_subsampler_.ShouldSample(0.001)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Compositing.FrameRateDecider.ToggleFrameRateCase", toggle_case);
  }

  // This is not the case we want to (or we can) lower the frame rate for video.
  if (!should_toggle) {
    TRACE_EVENT_INSTANT0(
        "viz",
        "FrameRateDecider::UpdatePreferredFrameIntervalIfNeeded - not enough "
        "frame sinks to toggle",
        TRACE_EVENT_SCOPE_THREAD);
    SetPreferredInterval(UnspecifiedFrameInterval());
    return;
  }

  // The code below picks the optimal frame interval for the display based on
  // the frame sinks which were updated in this frame. This is because we want
  // the display's update rate to be decided based on onscreen content that is
  // animating. This ensures that, for instance, if we're currently displaying
  // a video while the rest of the page is static, we choose the frame interval
  // optimal for the video.
  std::optional<base::TimeDelta> min_frame_sink_interval;
  [[maybe_unused]] bool all_frame_sinks_have_same_interval = true;
  for (const auto& frame_sink_id : frame_sinks_updated_in_previous_frame_) {
    auto interval =
        client_->GetPreferredFrameIntervalForFrameSinkId(frame_sink_id);
    if (!min_frame_sink_interval) {
      min_frame_sink_interval = interval;
      continue;
    }

    if (!AreAlmostEqual(*min_frame_sink_interval, interval))
      all_frame_sinks_have_same_interval = false;
    min_frame_sink_interval = std::min(*min_frame_sink_interval, interval);
  }

  // A redraw was done with no onscreen content getting updated, avoid updating
  // the interval in this case.
  if (!min_frame_sink_interval) {
    return;
  }

  TRACE_EVENT_INSTANT1("viz",
                       "FrameRateDecider::UpdatePreferredFrameIntervalIfNeeded",
                       TRACE_EVENT_SCOPE_THREAD, "min_frame_sink_interval",
                       min_frame_sink_interval->InMillisecondsF());

  bool can_set_preferred_frame_rate = false;
#if BUILDFLAG(IS_IOS)
  // iOS will pick ideal refresh rate based on the preferred rate set, which
  // will be rounded to the nearest refresh rate that is a factor of the maximum
  // rate of the device.
  can_set_preferred_frame_rate = true;
#elif BUILDFLAG(IS_MAC)
  can_set_preferred_frame_rate = all_frame_sinks_have_same_interval &&
                                 hw_support_for_multiple_refresh_rates_;
#else
  // If only one frame sink is being updated and its frame rate can be directly
  // forwarded to the system, then prefer that over choosing one of the refresh
  // rates advertised by the system.
  can_set_preferred_frame_rate = all_frame_sinks_have_same_interval &&
                                 output_surface_supports_set_frame_rate_;
#endif
  if (can_set_preferred_frame_rate) {
    SetPreferredInterval(*min_frame_sink_interval);
    return;
  }

  // Iterate through |supported_intervals_| to pick a frame rate that is closed
  // to min_frame_sink_interval. If we don't have an explicit preference from
  // the active frame sinks, then we use a 0 value for preferred frame interval
  // to let the framework pick the ideal refresh rate.
  base::TimeDelta new_preferred_interval = UnspecifiedFrameInterval();
  if (*min_frame_sink_interval != BeginFrameArgs::MinInterval()) {
    base::TimeDelta min_delta = base::TimeDelta::Max();
    for (auto supported_interval : supported_intervals_) {
      if (toggle_case ==
          ToggleFrameRateCase::kSingleVideoPerfectCadenceDiffersFromDisplay) {
        // For single video cases, pick the maximum interval with perfect
        // cadence from supported intervals.
        bool simple_cadence = media::VideoCadenceEstimator::HasSimpleCadence(
            supported_interval, *min_frame_sink_interval,
            kMaxTimeUntilNextGlitch);
        if (simple_cadence && supported_interval > new_preferred_interval) {
          new_preferred_interval = supported_interval;
        }
      } else {
        // Pick the display interval which is closest to the preferred interval
        // and less than or equal to the min_frame_sink_interval.
        base::TimeDelta delta = (*min_frame_sink_interval - supported_interval);
        if ((AreAlmostEqual(*min_frame_sink_interval, supported_interval) ||
             delta.is_positive()) &&
            delta.magnitude() < min_delta) {
          new_preferred_interval = supported_interval;
          min_delta = delta.magnitude();
        }
      }
    }
  }

  SetPreferredInterval(new_preferred_interval);
}

ToggleFrameRateCase FrameRateDecider::GetToggleFrameRateCase(
    const std::vector<base::TimeDelta>& fixed_interval_frame_sink_intervals)
    const {
  int num_of_frame_sinks_with_fixed_interval =
      fixed_interval_frame_sink_intervals.size();

  if (num_of_frame_sinks_with_fixed_interval == 0)
    return ToggleFrameRateCase::kNone;

  if (hw_support_for_multiple_refresh_rates_)
    return ToggleFrameRateCase::kHardwareSupported;

  // For single video cases, check if it is a perfect cadence one.
  if (num_of_frame_sinks_with_fixed_interval == 1) {
    base::TimeDelta interval = fixed_interval_frame_sink_intervals.back();
    if (AreAlmostEqual(*supported_intervals_.begin(), interval)) {
      return ToggleFrameRateCase::kSingleVideoPerfectCadenceMatchesDisplay;
    }
    for (auto supported_interval : supported_intervals_) {
      if (media::VideoCadenceEstimator::HasSimpleCadence(
              supported_interval, interval, kMaxTimeUntilNextGlitch)) {
        return ToggleFrameRateCase::
            kSingleVideoPerfectCadenceDiffersFromDisplay;
      }
    }

    return ToggleFrameRateCase::kSingleVideoNoPerfectCadence;
  }

  return ToggleFrameRateCase::kMultipleVideos;
}

void FrameRateDecider::SetPreferredInterval(
    base::TimeDelta new_preferred_interval) {
  TRACE_EVENT_INSTANT1("viz", "FrameRateDecider::SetPreferredInterval",
                       TRACE_EVENT_SCOPE_THREAD, "new_preferred_interval",
                       new_preferred_interval.InMillisecondsF());

  if (AreAlmostEqual(new_preferred_interval,
                     last_computed_preferred_frame_interval_)) {
    num_of_frames_since_preferred_interval_changed_++;
  } else {
    num_of_frames_since_preferred_interval_changed_ = 0u;
  }
  last_computed_preferred_frame_interval_ = new_preferred_interval;

  if (AreAlmostEqual(current_preferred_frame_interval_, new_preferred_interval))
    return;

  // The min num of frames heuristic is to ensure we see a constant pattern
  // before toggling the global setting to avoid unnecessary switches when
  // lowering the refresh rate. For increasing the refresh rate we toggle
  // immediately to prioritize smoothness.
  bool should_toggle =
      current_preferred_frame_interval_ > new_preferred_interval ||
      num_of_frames_since_preferred_interval_changed_ >=
          min_num_of_frames_to_toggle_interval_;
  if (should_toggle) {
    current_preferred_frame_interval_ = new_preferred_interval;
    client_->SetPreferredFrameInterval(new_preferred_interval);
  }
}

bool FrameRateDecider::multiple_refresh_rates_supported() const {
#if BUILDFLAG(IS_IOS)
  // iOS doesn't announce supported intervals, but rather supports settings
  // preferred frame rate via the BeginFrameSource that listens to vsync updates
  // sent by CADisplayLink. Preferred frame rate will be then rounded to the
  // factor of a maximum refresh rate of the device. It's chosen to always
  // return true here as |supports_set_frame_rate| means whether setting a frame
  // rate is supported via OutputSurface::SetFrameRate, which is not applicable
  // to iOS.
  return true;
#elif BUILDFLAG(IS_MAC)
  // In some Mac monitor/screen doesn't announce supported intervals.
  return supported_intervals_.size() > 1u ||
         hw_support_for_multiple_refresh_rates_;
#else
  return output_surface_supports_set_frame_rate_ ||
         supported_intervals_.size() > 1u;
#endif
}

void FrameRateDecider::SetHwSupportForMultipleRefreshRates(bool support) {
  hw_support_for_multiple_refresh_rates_ = support;
}

}  // namespace viz
