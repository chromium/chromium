// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_interval_decider.h"

#include <inttypes.h>

#include <utility>

#include "base/functional/overloaded.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/viz/common/quads/frame_interval_inputs.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"

namespace viz {

namespace {

std::unique_ptr<base::trace_event::TracedValue>
FrameIntervalMatcherInputsTracedValue(
    const FrameIntervalMatcher::Inputs& inputs) {
  auto traced_value = std::make_unique<base::trace_event::TracedValue>();

  for (const auto& [frame_sink_id, interval_inputs] : inputs.inputs_map) {
    auto frame_sink_scope = traced_value->BeginDictionaryScopedWithCopiedName(
        frame_sink_id.ToString());
    traced_value->SetInteger("time_diff_us",
                             static_cast<int>((inputs.aggregated_frame_time -
                                               interval_inputs.frame_time)
                                                  .InMicroseconds()));
    traced_value->SetBoolean("has_input", interval_inputs.has_input);
    traced_value->SetBoolean(
        "only_content",
        interval_inputs.has_only_content_frame_interval_updates);

    int index = 0;
    for (const ContentFrameIntervalInfo& content_info :
         interval_inputs.content_interval_info) {
      auto content_info_scope =
          traced_value->BeginDictionaryScopedWithCopiedName(
              base::StringPrintf("content_info_%d", index));
      traced_value->SetString(
          "type", ContentFrameIntervalTypeToString(content_info.type));
      traced_value->SetInteger(
          "interval_us",
          static_cast<int>(content_info.frame_interval.InMicroseconds()));
      traced_value->SetInteger("duplicate_count", content_info.duplicate_count);
      index++;
    }
  }

  return traced_value;
}
}  // namespace

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
  absl::visit(base::Overloaded(
                  [](const absl::monostate& monostate) {},
                  [](const FixedIntervalSettings& fixed_interval_settings) {
                    CHECK(!fixed_interval_settings.supported_intervals.empty());
                    CHECK(fixed_interval_settings.supported_intervals.contains(
                        fixed_interval_settings.default_interval));
                  },
                  [](const ContinuousRangeSettings& continuous_range_settings) {
                    CHECK_LE(continuous_range_settings.min_interval,
                             continuous_range_settings.max_interval);
                  }),
              settings.interval_settings);

  settings_ = std::move(settings);
  matchers_ = std::move(matchers);
}

std::unique_ptr<FrameIntervalDecider::ScopedAggregate>
FrameIntervalDecider::WrapAggregate(SurfaceManager& surface_manager,
                                    base::TimeTicks frame_time) {
  return base::WrapUnique(new FrameIntervalDecider::ScopedAggregate(
      *this, surface_manager, frame_time));
}

void FrameIntervalDecider::Decide(
    base::TimeTicks frame_time,
    base::flat_map<FrameSinkId, FrameIntervalInputs> inputs_map) {
  FrameIntervalMatcher::Inputs matcher_inputs(settings_);
  matcher_inputs.aggregated_frame_time = frame_time;
  matcher_inputs.inputs_map = std::move(inputs_map);

  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                      "FrameIntervalMatcherInputs", "inputs",
                      FrameIntervalMatcherInputsTracedValue(matcher_inputs));

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

  // If nothing matched, use the default.
  if (!match_result) {
    match_result = absl::visit(
        base::Overloaded(
            [](const absl::monostate& monostate) -> Result {
              return FrameIntervalClass::kDefault;
            },
            [](const FixedIntervalSettings& fixed_interval_settings) -> Result {
              return fixed_interval_settings.default_interval;
            },
            [](const ContinuousRangeSettings& continuous_range_settings)
                -> Result { return continuous_range_settings.min_interval; }),
        settings_.interval_settings);
  }

  // No need to notify client if result did not change.
  if (current_result_ == match_result) {
    current_result_frame_time_ = frame_time;
    return;
  }

  // Same as above but using epsilon comparison for frame interval.
  if (current_result_ && match_result &&
      absl::holds_alternative<base::TimeDelta>(current_result_.value()) &&
      absl::holds_alternative<base::TimeDelta>(match_result.value()) &&
      FrameIntervalMatcher::AreAlmostEqual(
          absl::get<base::TimeDelta>(current_result_.value()),
          absl::get<base::TimeDelta>(match_result.value()),
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
  return absl::visit(
      base::Overloaded(
          [&](FrameIntervalClass from_frame_interval_class) {
            if (!absl::holds_alternative<FrameIntervalClass>(to.value())) {
              return true;
            }
            FrameIntervalClass to_frame_interval_class =
                absl::get<FrameIntervalClass>(to.value());
            return static_cast<int>(from_frame_interval_class) >
                   static_cast<int>(to_frame_interval_class);
          },
          [&](base::TimeDelta from_interval) {
            if (!absl::holds_alternative<base::TimeDelta>(to.value())) {
              return true;
            }
            base::TimeDelta to_interval =
                absl::get<base::TimeDelta>(to.value());
            return from_interval > to_interval;
          }),
      from.value());
}

}  // namespace viz
