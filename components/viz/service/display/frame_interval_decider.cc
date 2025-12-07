// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_interval_decider.h"

#include <inttypes.h>

#include <utility>
#include <variant>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/quads/frame_interval_inputs.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace viz {

FrameIntervalDecider::ScopedAggregate::ScopedAggregate(
    FrameIntervalDecider& decider,
    SurfaceManager& surface_manager,
    base::TimeTicks frame_time)
    : decider_(decider),
      surface_manager_(surface_manager),
      frame_time_(frame_time) {
  surface_manager_->AddObserver(this);
}

FrameIntervalDecider::ScopedAggregate::~ScopedAggregate() {
  surface_manager_->RemoveObserver(this);
  decider_->Decide(frame_time_, std::move(drawn_frame_sinks_));
}

void FrameIntervalDecider::ScopedAggregate::OnSurfaceWillBeDrawn(
    Surface* surface) {
  drawn_frame_sinks_.emplace(surface->surface_id().frame_sink_id(),
                             surface->GetFrameIntervalInputs());
}

FrameIntervalDecider::FrameIntervalDecider() = default;
FrameIntervalDecider::~FrameIntervalDecider() = default;

void FrameIntervalDecider::UpdateSettings(
    Settings settings,
    std::vector<std::unique_ptr<FrameIntervalMatcher>> matchers) {
  std::visit(absl::Overload(
                 [](const std::monostate& monostate) {},
                 [](const FixedIntervalSettings& fixed_interval_settings) {
                   CHECK(!fixed_interval_settings.supported_intervals.empty());
                 },
                 [](const ContinuousRangeSettings& continuous_range_settings) {
                   CHECK_LE(continuous_range_settings.min_interval,
                            continuous_range_settings.max_interval);
                 }),
             settings.interval_settings);

  settings_ = std::move(settings);
  matchers_ = std::move(matchers);
}

FrameIntervalDecider::ScopedAggregate FrameIntervalDecider::WrapAggregate(
    SurfaceManager& surface_manager,
    base::TimeTicks frame_time) {
  return FrameIntervalDecider::ScopedAggregate(*this, surface_manager,
                                               frame_time);
}

void FrameIntervalDecider::Decide(
    base::TimeTicks frame_time,
    base::flat_map<FrameSinkId, FrameIntervalInputs> inputs_map) {
  FrameIntervalMatcher::Inputs matcher_inputs(settings_, frame_id_++);
  matcher_inputs.aggregated_frame_time = frame_time;
  matcher_inputs.inputs_map = std::move(inputs_map);

  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                      "FrameIntervalMatcherInputs", "inputs", matcher_inputs);

  // Run through matchers in order and use the first non-null result.
  std::optional<Result> match_result;
  FrameIntervalMatcherType matcher_type = FrameIntervalMatcherType::kNone;
  for (const auto& matcher : matchers_) {
    match_result = matcher->Match(matcher_inputs);
    if (match_result) {
      matcher_type = matcher->type();
      break;
    }
  }

  if (base::ShouldRecordSubsampledMetric(0.001)) {
    base::UmaHistogramEnumeration("Viz.FrameIntervalDecider.ResultMatcherType",
                                  matcher_type);
    if (match_result &&
        std::holds_alternative<ResultInterval>(match_result.value())) {
      base::UmaHistogramCustomTimes(
          "Viz.FrameIntervalDecider.ResultTimeDelta",
          std::get<ResultInterval>(match_result.value()).interval,
          base::Milliseconds(0), base::Milliseconds(500), 50);
    }
  }

  // If nothing matched, use the default.
  if (!match_result) {
    match_result = std::visit(
        absl::Overload(
            [](const std::monostate& monostate) -> Result {
              return FrameIntervalClass::kDefault;
            },
            [](const FixedIntervalSettings& fixed_interval_settings) -> Result {
              return ResultInterval{fixed_interval_settings.default_interval};
            },
            [](const ContinuousRangeSettings& continuous_range_settings)
                -> Result {
              return ResultInterval{continuous_range_settings.default_interval};
            }),
        settings_.interval_settings);
  }

  // No need to notify client if result did not change.
  if (current_result_ == match_result) {
    current_result_frame_time_ = frame_time;
    return;
  }

  // Same as above but using epsilon comparison for frame interval.
  if (current_result_ && match_result &&
      std::holds_alternative<ResultInterval>(current_result_.value()) &&
      std::holds_alternative<ResultInterval>(match_result.value()) &&
      FrameIntervalMatcher::AreAlmostEqual(
          std::get<ResultInterval>(current_result_.value()).interval,
          std::get<ResultInterval>(match_result.value()).interval,
          settings_.epsilon)) {
    current_result_frame_time_ = frame_time;
    return;
  }

  // If result is increasing frame interval, then ensure the it's not a blip by
  // delaying updating client. Allow reducing frame interval immediately
  // however.
  if (frame_time - current_result_frame_time_ >
          settings_.increase_frame_interval_timeout ||
      MayDecreaseFrameInterval(current_result_, match_result)) {
    TRACE_EVENT_INSTANT(
        "viz", "FrameIntervalDeciderResult", "result",
        FrameIntervalMatcher::ResultToString(match_result.value()),
        "matcher_type",
        FrameIntervalMatcher::MatcherTypeToString(matcher_type));
    current_result_frame_time_ = frame_time;
    current_result_ = match_result;
    if (settings_.result_callback) {
      settings_.result_callback.Run(match_result.value(), matcher_type);
    }
  }
}

// static
bool FrameIntervalDecider::MayDecreaseFrameInterval(
    const std::optional<Result>& from,
    const std::optional<Result>& to) {
  if (!from || !to) {
    return true;
  }
  return std::visit(
      absl::Overload(
          [&](FrameIntervalClass from_frame_interval_class) {
            if (!std::holds_alternative<FrameIntervalClass>(to.value())) {
              return true;
            }
            FrameIntervalClass to_frame_interval_class =
                std::get<FrameIntervalClass>(to.value());
            return static_cast<int>(from_frame_interval_class) >
                   static_cast<int>(to_frame_interval_class);
          },
          [&](ResultInterval from_interval) {
            if (!std::holds_alternative<ResultInterval>(to.value())) {
              return true;
            }
            ResultInterval to_interval = std::get<ResultInterval>(to.value());
            return from_interval.interval > to_interval.interval;
          }),
      from.value());
}

}  // namespace viz
