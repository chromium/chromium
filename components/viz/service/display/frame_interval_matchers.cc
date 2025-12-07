// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_interval_matchers.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "base/strings/stringprintf.h"
#include "base/trace_event/typed_macros.h"
#include "components/viz/common/quads/frame_interval_inputs.h"
#include "media/filters/video_cadence_estimator.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace viz {

namespace {

// Matches case where only has content of type `type` is updating and they all
// have the same content frame interval. Then return that interval.
std::optional<FrameIntervalMatcher::Result> MatchContentIntervalType(
    const FrameIntervalMatcher::Inputs& matcher_inputs,
    ContentFrameIntervalType type,
    FrameIntervalMatcher::ResultIntervalType interval_type) {
  std::optional<base::TimeDelta> content_interval;
  for (const auto& [frame_sink_id, inputs] : matcher_inputs.inputs_map) {
    // Skip frame sinks that are old.
    if ((matcher_inputs.aggregated_frame_time - inputs.frame_time) >
        matcher_inputs.settings->ignore_frame_sink_timeout) {
      continue;
    }
    // Don't match if viz client has other updates.
    if (!inputs.has_only_content_frame_interval_updates) {
      return std::nullopt;
    }
    for (const ContentFrameIntervalInfo& content_frame_interval_info :
         inputs.content_interval_info) {
      // Skip if there are other types of content.
      if (content_frame_interval_info.type != type) {
        return std::nullopt;
      }
      // Don't match if content interval are not all the same.
      if (content_interval && !FrameIntervalMatcher::AreAlmostEqual(
                                  content_interval.value(),
                                  content_frame_interval_info.frame_interval,
                                  matcher_inputs.settings->epsilon)) {
        return std::nullopt;
      }
      content_interval = inputs.content_interval_info.front().frame_interval;
    }
  }
  // Fail to match if no content type matches.
  if (!content_interval) {
    return std::nullopt;
  }

  base::TimeDelta interval = std::visit(
      absl::Overload(
          [&](const std::monostate& monostate) {
            // If no intervals settings are given, then just return the content
            // interval.
            return content_interval.value();
          },
          [&](const FrameIntervalMatcher::FixedIntervalSettings&
                  fixed_interval_settings) {
            // Pick the best interval from supported intervals using
            // `HasSimpleCadence`.
            std::optional<base::TimeDelta> best_interval;
            for (auto supported_interval :
                 fixed_interval_settings.supported_intervals) {
              bool simple_cadence =
                  media::VideoCadenceEstimator::HasSimpleCadence(
                      supported_interval, content_interval.value(),
                      matcher_inputs.settings->max_time_until_next_glitch);
              if (simple_cadence &&
                  (!best_interval || supported_interval > best_interval)) {
                best_interval = supported_interval;
              }
            }
            return best_interval.value_or(
                fixed_interval_settings.default_interval);
          },
          [&](const FrameIntervalMatcher::ContinuousRangeSettings&
                  continuous_range_settings) {
            // Pick the best interval within the continuous range, such that the
            // chosen value has a perfect integer cadence relative to the
            // target.
            base::TimeDelta range_min = continuous_range_settings.min_interval;
            base::TimeDelta range_max = continuous_range_settings.max_interval;
            // If the target is below the range minimum (too fast), determine
            // the minimum cadence necessary to reach the minimum interval.
            if (content_interval.value() < range_min) {
              int cadence = std::ceil(range_min / content_interval.value());
              base::TimeDelta cadence_interval =
                  cadence * content_interval.value();
              // Use the calculated cadence if it didn't overshoot the range
              // maximum. Otherwise use the range minimum as the closest
              // fallback.
              return cadence_interval <= range_max ? cadence_interval
                                                   : range_min;
            }
            // If the target is above the range maximum (too slow), determine
            // the minimum cadence necessary to reach the maximum interval.
            if (content_interval.value() > range_max) {
              // Use inverse cadence (i.e. 1/2, 1/3, 1/4, ... of the content
              // interval.)
              int cadence = std::ceil(content_interval.value() / range_max);
              base::TimeDelta cadence_interval =
                  content_interval.value() / cadence;
              // Use the calculated cadence if it didn't undershoot the range
              // minimum. Otherwise use the range maximum as the closest
              // fallback.
              return cadence_interval >= range_min ? cadence_interval
                                                   : range_max;
            }
            // Content falls within the supported range and can be used
            // directly.
            return content_interval.value();
          }),
      matcher_inputs.settings->interval_settings);
  return FrameIntervalMatcher::ResultInterval{interval, interval_type};
}

}  // namespace

bool FrameIntervalMatcher::ResultInterval::operator==(
    const ResultInterval& other) const = default;
FrameIntervalMatcher::FixedIntervalSettings::FixedIntervalSettings() = default;
FrameIntervalMatcher::FixedIntervalSettings::FixedIntervalSettings(
    const FixedIntervalSettings&) = default;
FrameIntervalMatcher::FixedIntervalSettings::~FixedIntervalSettings() = default;

FrameIntervalMatcher::ContinuousRangeSettings::ContinuousRangeSettings() =
    default;
FrameIntervalMatcher::ContinuousRangeSettings::ContinuousRangeSettings(
    const ContinuousRangeSettings&) = default;
FrameIntervalMatcher::ContinuousRangeSettings::~ContinuousRangeSettings() =
    default;

FrameIntervalMatcher::Settings::Settings() = default;
FrameIntervalMatcher::Settings::~Settings() = default;
FrameIntervalMatcher::Settings::Settings(const Settings& other) = default;
FrameIntervalMatcher::Settings& FrameIntervalMatcher::Settings::operator=(
    const Settings& other) = default;
FrameIntervalMatcher::Settings::Settings(Settings&& other) = default;
FrameIntervalMatcher::Settings& FrameIntervalMatcher::Settings::operator=(
    Settings&& other) = default;

FrameIntervalMatcher::Inputs::Inputs(const Settings& settings,
                                     uint64_t frame_id)
    : settings(settings), frame_id(frame_id) {}
FrameIntervalMatcher::Inputs::~Inputs() = default;
FrameIntervalMatcher::Inputs::Inputs(const Inputs& other) = default;
FrameIntervalMatcher::Inputs& FrameIntervalMatcher::Inputs::operator=(
    const Inputs& other) = default;

void FrameIntervalMatcher::Inputs::WriteIntoTrace(
    perfetto::TracedValue trace_context) const {
  auto dict = std::move(trace_context).WriteDictionary();
  for (const auto& [frame_sink_id, interval_inputs] : inputs_map) {
    // frame_sink_dict needs to be enclosed in a dedicated code block so that it
    // would go out of scope and be destructed before content_info_dict is
    // created. See https://crbug.com/371227621.
    {
      std::string frame_sink_str = frame_sink_id.ToString();
      auto frame_sink_dict =
          dict.AddDictionary(perfetto::DynamicString(frame_sink_str));
      frame_sink_dict.Add("time_diff_us",
                          (aggregated_frame_time - interval_inputs.frame_time)
                              .InMicroseconds());
      frame_sink_dict.Add("has_input", interval_inputs.has_input);
      frame_sink_dict.Add(
          "only_content",
          interval_inputs.has_only_content_frame_interval_updates);
    }

    int index = 0;
    for (const ContentFrameIntervalInfo& content_info :
         interval_inputs.content_interval_info) {
      std::string content_info_str =
          base::StringPrintf("content_info_%d", index);
      auto content_info_dict =
          dict.AddDictionary(perfetto::DynamicString(content_info_str));
      content_info_dict.Add(
          "type", ContentFrameIntervalTypeToString(content_info.type));
      content_info_dict.Add("interval_us",
                            content_info.frame_interval.InMicroseconds());
      content_info_dict.Add("duplicate_count", content_info.duplicate_count);
      index++;
    }
  }
}

// static
std::string FrameIntervalMatcher::ResultToString(const Result& result) {
  return std::visit(
      absl::Overload(
          [](FrameIntervalClass frame_interval_class) -> std::string {
            switch (frame_interval_class) {
              case FrameIntervalClass::kBoost:
                return "kBoost";
              case FrameIntervalClass::kDefault:
                return "kDefault";
            }
          },
          [](ResultInterval interval) {
            return base::StringPrintf("%" PRId64 "us type:%d",
                                      interval.interval.InMicroseconds(),
                                      static_cast<int>(interval.type));
          }),
      result);
}

// static
std::string FrameIntervalMatcher::MatcherTypeToString(
    FrameIntervalMatcherType type) {
  switch (type) {
    case FrameIntervalMatcherType::kNone:
      return "None";
    case FrameIntervalMatcherType::kInputBoost:
      return "InputBoost";
    case FrameIntervalMatcherType::kSlowScrollThrottle:
      return "SlowScrollThrottle";
    case FrameIntervalMatcherType::kOnlyVideo:
      return "OnlyVideo";
    case FrameIntervalMatcherType::kVideoConference:
      return "VideoConference";
    case FrameIntervalMatcherType::kOnlyAnimatingImage:
      return "kOnlyAnimatingImage";
    case FrameIntervalMatcherType::kUserInputBoost:
      return "UserInputBoost";
    case FrameIntervalMatcherType::kOnlyScrollBarFadeOut:
      return "OnlyScrollBarFadeOut";
  }
}

// static
bool FrameIntervalMatcher::AreAlmostEqual(base::TimeDelta a,
                                          base::TimeDelta b,
                                          base::TimeDelta epsilon) {
  if (a.is_min() || b.is_min() || a.is_max() || b.is_max()) {
    return a == b;
  }

  return (a - b).magnitude() <= epsilon;
}

FrameIntervalMatcher::FrameIntervalMatcher(FrameIntervalMatcherType type)
    : type_(type) {}

#define DefineSimpleMatcherConstructorDestructor(ClassName, MatcherType) \
  ClassName::ClassName()                                                 \
      : FrameIntervalMatcher(FrameIntervalMatcherType::MatcherType) {}   \
  ClassName::~ClassName() = default

// If there's any input, return kBoost or the highest supported frame interval.
DefineSimpleMatcherConstructorDestructor(InputBoostMatcher, kInputBoost);
std::optional<FrameIntervalMatcher::Result> InputBoostMatcher::Match(
    const Inputs& matcher_inputs) {
  for (const auto& [frame_sink_id, inputs] : matcher_inputs.inputs_map) {
    if (inputs.has_input &&
        (matcher_inputs.aggregated_frame_time - inputs.frame_time) <
            matcher_inputs.settings->ignore_frame_sink_timeout) {
      return std::visit(
          absl::Overload(
              [](const std::monostate& monostate) -> Result {
                return FrameIntervalClass::kBoost;
              },
              [](const FixedIntervalSettings& fixed_interval_settings)
                  -> Result {
                return ResultInterval{
                    *fixed_interval_settings.supported_intervals.begin(),
                    ResultIntervalType::kAtLeast};
              },
              [](const ContinuousRangeSettings& continuous_range_settings)
                  -> Result {
                return ResultInterval{continuous_range_settings.min_interval,
                                      ResultIntervalType::kAtLeast};
              }),
          matcher_inputs.settings->interval_settings);
    }
  }
  return std::nullopt;
}

// Matches when there are only videos of same frame interval updating.
// Returns the video frame interval or the ideal supported interval (if
// supplied).
DefineSimpleMatcherConstructorDestructor(OnlyVideoMatcher, kOnlyVideo);
std::optional<FrameIntervalMatcher::Result> OnlyVideoMatcher::Match(
    const Inputs& matcher_inputs) {
  return MatchContentIntervalType(matcher_inputs,
                                  ContentFrameIntervalType::kVideo,
                                  ResultIntervalType::kExact);
}

// Matches video conference case by using heuristic of 2 or more videos.
// Videos do not need to have the same frame interval. If supported intervals
// are supplied, then pick the biggest interval that is smaller than the video's
// interval, which may not be the ideal interval.
DefineSimpleMatcherConstructorDestructor(VideoConferenceMatcher,
                                         kVideoConference);
std::optional<FrameIntervalMatcher::Result> VideoConferenceMatcher::Match(
    const Inputs& matcher_inputs) {
  size_t num_videos = 0;
  std::optional<base::TimeDelta> min_interval;
  for (const auto& [frame_sink_id, inputs] : matcher_inputs.inputs_map) {
    for (const ContentFrameIntervalInfo& content_frame_interval_info :
         inputs.content_interval_info) {
      // Ignore non-video.
      if (content_frame_interval_info.type !=
          ContentFrameIntervalType::kVideo) {
        continue;
      }
      // Skip video that hasn't updated 5 times its interval. These videos are
      // probably paused. Not using `ignore_frame_sink_timeout` since these can
      // be larger in practice.
      if ((matcher_inputs.aggregated_frame_time - inputs.frame_time) >
          5 * content_frame_interval_info.frame_interval) {
        continue;
      }

      if (!min_interval ||
          min_interval.value() > content_frame_interval_info.frame_interval) {
        min_interval = content_frame_interval_info.frame_interval;
      }
      num_videos += 1u + content_frame_interval_info.duplicate_count;
    }
  }
  if (num_videos < 2u) {
    return std::nullopt;
  }

  base::TimeDelta interval = std::visit(
      absl::Overload(
          [&](const std::monostate& monostate) { return min_interval.value(); },
          [&](const FixedIntervalSettings& fixed_interval_settings) {
            // Pick closest supported interval amongst discrete list.
            base::TimeDelta closest_supported_interval;
            base::TimeDelta min_delta = base::TimeDelta::Max();
            for (auto supported_interval :
                 fixed_interval_settings.supported_intervals) {
              base::TimeDelta delta = min_interval.value() - supported_interval;
              if ((AreAlmostEqual(min_interval.value(), supported_interval,
                                  matcher_inputs.settings->epsilon) ||
                   delta.is_positive()) &&
                  delta.magnitude() < min_delta) {
                closest_supported_interval = supported_interval;
                min_delta = delta.magnitude();
              }
            }
            return closest_supported_interval;
          },
          [&](const ContinuousRangeSettings& continuous_range_settings) {
            // Pick closest supported interval within continuous range.
            return std::clamp(min_interval.value(),
                              continuous_range_settings.min_interval,
                              continuous_range_settings.max_interval);
          }),
      matcher_inputs.settings->interval_settings);
  return ResultInterval{interval};
}

DefineSimpleMatcherConstructorDestructor(OnlyAnimatingImageMatcher,
                                         kOnlyAnimatingImage);
std::optional<FrameIntervalMatcher::Result> OnlyAnimatingImageMatcher::Match(
    const Inputs& matcher_inputs) {
  return MatchContentIntervalType(matcher_inputs,
                                  ContentFrameIntervalType::kAnimatingImage,
                                  ResultIntervalType::kExact);
}

DefineSimpleMatcherConstructorDestructor(OnlyScrollBarFadeOutAnimationMatcher,
                                         kOnlyScrollBarFadeOut);
std::optional<FrameIntervalMatcher::Result>
OnlyScrollBarFadeOutAnimationMatcher::Match(const Inputs& matcher_inputs) {
  return MatchContentIntervalType(
      matcher_inputs, ContentFrameIntervalType::kScrollBarFadeOutAnimation,
      ResultIntervalType::kAtLeast);
}

DefineSimpleMatcherConstructorDestructor(UserInputBoostMatcher,
                                         kUserInputBoost);
std::optional<FrameIntervalMatcher::Result> UserInputBoostMatcher::Match(
    const Inputs& matcher_inputs) {
  for (const auto& [frame_sink_id, inputs] : matcher_inputs.inputs_map) {
    if (inputs.has_user_input &&
        (matcher_inputs.aggregated_frame_time - inputs.frame_time) <
            matcher_inputs.settings->ignore_frame_sink_timeout) {
      return std::visit(
          absl::Overload(
              [](const std::monostate& monostate) -> Result {
                return FrameIntervalClass::kBoost;
              },
              [](const FixedIntervalSettings& fixed_interval_settings)
                  -> Result {
                return ResultInterval{
                    *fixed_interval_settings.supported_intervals.begin(),
                    ResultIntervalType::kAtLeast};
              },
              [](const ContinuousRangeSettings& continuous_range_settings)
                  -> Result {
                return ResultInterval{continuous_range_settings.min_interval,
                                      ResultIntervalType::kAtLeast};
              }),
          matcher_inputs.settings->interval_settings);
    }
  }
  return std::nullopt;
}

SlowScrollThrottleMatcher::SlowScrollThrottleMatcher(float device_scale_factor)
    : FrameIntervalMatcher(FrameIntervalMatcherType::kSlowScrollThrottle),
      device_scale_factor_(device_scale_factor) {}
SlowScrollThrottleMatcher::~SlowScrollThrottleMatcher() = default;

std::optional<FrameIntervalMatcher::Result> SlowScrollThrottleMatcher::Match(
    const Inputs& matcher_inputs) {
  CHECK(std::holds_alternative<std::monostate>(
      matcher_inputs.settings->interval_settings));
  float scroll_speed = 0.f;
  bool ignored_extra_update = false;
  for (const auto& [frame_sink_id, inputs] : matcher_inputs.inputs_map) {
    // Skip frame sinks that are old.
    if (matcher_inputs.aggregated_frame_time - inputs.frame_time >=
        matcher_inputs.settings->ignore_frame_sink_timeout) {
      continue;
    }
    bool has_non_scroll_update =
        !inputs.has_only_content_frame_interval_updates ||
        std::any_of(
            inputs.content_interval_info.begin(),
            inputs.content_interval_info.end(),
            [](const ContentFrameIntervalInfo& content_frame_interval_info) {
              return content_frame_interval_info.type !=
                     ContentFrameIntervalType::kCompositorScroll;
            });
    // Scroll can occasionally have new tiles rastered or new content recorded
    // in a commit. Ignore these one off frames that has updates beyond just
    // scroll. Otherwise, do not match.
    if (has_non_scroll_update &&
        (matcher_inputs.frame_id - last_frame_id_matched_without_extra_update_ >
         1)) {
      return std::nullopt;
    }
    if (has_non_scroll_update) {
      ignored_extra_update = true;
    }
    if (inputs.major_scroll_speed_in_pixels_per_second > scroll_speed) {
      scroll_speed = inputs.major_scroll_speed_in_pixels_per_second;
    }
  }

  // No scroll.
  if (scroll_speed <= 0) {
    return std::nullopt;
  }

  if (!ignored_extra_update) {
    last_frame_id_matched_without_extra_update_ = matcher_inputs.frame_id;
  }
  float speed_dps = scroll_speed / device_scale_factor_;
  // The hard-coded values are copied from AOSP View.convertVelocityToFrameRate.
  if (speed_dps > 300) {
    return FrameIntervalClass::kBoost;
  } else if (speed_dps > 125) {
    return ResultInterval{base::Hertz(80), ResultIntervalType::kAtLeast};
  } else {
    return ResultInterval{base::Hertz(60), ResultIntervalType::kAtLeast};
  }
}

}  // namespace viz
