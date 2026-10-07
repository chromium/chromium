// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_interval_matchers.h"

#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/time/time.h"
#include "perfetto/test/traced_value_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

using FrameIntervalClass = FrameIntervalMatcher::FrameIntervalClass;
using ResultIntervalType = FrameIntervalMatcher::ResultIntervalType;
using ResultInterval = FrameIntervalMatcher::ResultInterval;
using Result = FrameIntervalMatcher::Result;
using FixedIntervalSettings = FrameIntervalMatcher::FixedIntervalSettings;
using ContinuousRangeSettings = FrameIntervalMatcher::ContinuousRangeSettings;
using Settings = FrameIntervalMatcher::Settings;
using Inputs = FrameIntervalMatcher::Inputs;

constexpr base::TimeTicks kNow = base::TimeTicks() + base::Seconds(1234);

void ExpectResult(const std::optional<Result> result_opt,
                  FrameIntervalClass frame_interval_class) {
  ASSERT_TRUE(result_opt.has_value());
  const Result& result = result_opt.value();
  ASSERT_TRUE(std::holds_alternative<FrameIntervalClass>(result));
  EXPECT_EQ(frame_interval_class, std::get<FrameIntervalClass>(result));
}

void ExpectResult(const std::optional<Result> result_opt,
                  base::TimeDelta interval,
                  ResultIntervalType interval_type) {
  ASSERT_TRUE(result_opt.has_value());
  const Result& result = result_opt.value();
  ASSERT_TRUE(std::holds_alternative<ResultInterval>(result));
  EXPECT_EQ(interval, std::get<ResultInterval>(result).interval);
  EXPECT_EQ(interval_type, std::get<ResultInterval>(result).type);
}

void ExpectNullResult(const std::optional<Result> result_opt) {
  EXPECT_FALSE(result_opt.has_value());
}

FixedIntervalSettings BuildDefaultFixedIntervalSettings() {
  FixedIntervalSettings fixed_interval_settings;
  fixed_interval_settings.supported_intervals.insert(base::Milliseconds(8));
  fixed_interval_settings.supported_intervals.insert(base::Milliseconds(16));
  fixed_interval_settings.default_interval = base::Milliseconds(16);
  return fixed_interval_settings;
}

// Returns a list of fixed intervals settings where the supported intervals are
// extremely close in value. Some displays (usually desktop) can support this.
// 60Hz & 59.94Hz are a real example.
FixedIntervalSettings BuildDenseFixedIntervalSettings() {
  FixedIntervalSettings fixed_interval_settings;
  fixed_interval_settings.supported_intervals.insert(base::Hertz(60));
  fixed_interval_settings.supported_intervals.insert(base::Hertz(59.94));
  fixed_interval_settings.default_interval =
      *fixed_interval_settings.supported_intervals.begin();
  return fixed_interval_settings;
}

ContinuousRangeSettings BuildContinuousRangeSettings(
    base::TimeDelta min_interval = base::Hertz(120),
    base::TimeDelta max_interval = base::Hertz(40)) {
  ContinuousRangeSettings continuous_range_settings;
  continuous_range_settings.min_interval = min_interval;
  continuous_range_settings.max_interval = max_interval;
  return continuous_range_settings;
}

Inputs BuildDefaultInputs(Settings& settings, uint32_t num_sinks) {
  Inputs inputs(settings, /*frame_id=*/0u);

  inputs.aggregated_frame_time = kNow;
  for (uint32_t sink_id = 1; sink_id <= num_sinks; ++sink_id) {
    FrameSinkId id(0, sink_id);
    FrameIntervalInputs frame_interval_inputs;
    frame_interval_inputs.frame_time = kNow;
    inputs.inputs_map.insert({id, frame_interval_inputs});
  }

  return inputs;
}

TEST(FrameIntervalMatchersTest, InputBoost) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  InputBoostMatcher matcher;

  inputs.inputs_map[FrameSinkId(0, 1)].has_input = true;
  ExpectResult(matcher.Match(inputs), FrameIntervalClass::kBoost);

  inputs.inputs_map[FrameSinkId(0, 1)].has_input = false;
  ExpectNullResult(matcher.Match(inputs));
}

TEST(FrameIntervalMatchersTest, InputBoostFixedInterval) {
  Settings settings;
  settings.interval_settings = BuildDefaultFixedIntervalSettings();
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  InputBoostMatcher matcher;

  inputs.inputs_map[FrameSinkId(0, 1)].has_input = true;
  ExpectResult(matcher.Match(inputs), base::Milliseconds(8),
               ResultIntervalType::kAtLeast);

  inputs.inputs_map[FrameSinkId(0, 1)].has_input = false;
  ExpectNullResult(matcher.Match(inputs));
}

TEST(FrameIntervalMatchersTest, InputBoostIgnoreOldSinks) {
  Settings settings;
  settings.ignore_frame_sink_timeout = base::Milliseconds(100);
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  InputBoostMatcher matcher;

  FrameIntervalInputs& frame_interval_inputs =
      inputs.inputs_map[FrameSinkId(0, 1)];
  frame_interval_inputs.has_input = true;
  frame_interval_inputs.frame_time = kNow - base::Milliseconds(200);
  ExpectNullResult(matcher.Match(inputs));
}

TEST(FrameIntervalMatchersTest, OnlyVideo) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
  OnlyVideoMatcher matcher;

  FrameIntervalInputs& frame_interval_inputs =
      inputs.inputs_map[FrameSinkId(0, 1)];
  frame_interval_inputs.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(32)});
  frame_interval_inputs.has_only_content_frame_interval_updates = true;

  ExpectResult(matcher.Match(inputs), base::Milliseconds(32),
               ResultIntervalType::kExact);

  frame_interval_inputs.has_only_content_frame_interval_updates = false;
  ExpectNullResult(matcher.Match(inputs));
}

TEST(FrameIntervalMatchersTest, OnlyVideoFixedInterval) {
  Settings settings;
  settings.interval_settings = BuildDefaultFixedIntervalSettings();
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
  OnlyVideoMatcher matcher;

  FrameIntervalInputs& frame_interval_inputs =
      inputs.inputs_map[FrameSinkId(0, 1)];
  frame_interval_inputs.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(24)});
  frame_interval_inputs.has_only_content_frame_interval_updates = true;

  ExpectResult(matcher.Match(inputs), base::Milliseconds(8),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, OnlyVideoFixedIntervalNoSimpleCadence) {
  Settings settings;
  settings.interval_settings = BuildDefaultFixedIntervalSettings();
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
  OnlyVideoMatcher matcher;

  FrameIntervalInputs& frame_interval_inputs =
      inputs.inputs_map[FrameSinkId(0, 1)];
  frame_interval_inputs.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(23)});
  frame_interval_inputs.has_only_content_frame_interval_updates = true;

  // Should return default if there is no simple cadence with any fixed
  // supported intervals.
  ExpectResult(matcher.Match(inputs), base::Milliseconds(16),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, OnlyVideoDifferentIntervals) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  OnlyVideoMatcher matcher;

  FrameIntervalInputs& interval_inputs1 = inputs.inputs_map[FrameSinkId(0, 1)];
  interval_inputs1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(32)});
  interval_inputs1.has_only_content_frame_interval_updates = true;

  FrameIntervalInputs& interval_inputs2 = inputs.inputs_map[FrameSinkId(0, 2)];
  interval_inputs2.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(24)});
  interval_inputs2.has_only_content_frame_interval_updates = true;

  ExpectNullResult(matcher.Match(inputs));

  interval_inputs2.content_interval_info[0].frame_interval =
      base::Milliseconds(32);
  ExpectResult(matcher.Match(inputs), base::Milliseconds(32),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, OnlyVideoContinuousRange) {
  Settings settings;
  settings.interval_settings = BuildContinuousRangeSettings();
  settings.epsilon = base::Milliseconds(0.1f);
  OnlyVideoMatcher matcher;

  // Verify that the exact content interval is chosen when it falls within the
  // supported range.
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
    FrameIntervalInputs& frame_interval_inputs =
        inputs.inputs_map[FrameSinkId(0, 1)];
    frame_interval_inputs.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(60)});
    frame_interval_inputs.has_only_content_frame_interval_updates = true;

    ExpectResult(matcher.Match(inputs), base::Hertz(60),
                 ResultIntervalType::kExact);
  }

  // Verify that the lowest perfect cadence (= 2) is chosen when the target
  // interval falls above the supported range.
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
    FrameIntervalInputs& frame_interval_inputs =
        inputs.inputs_map[FrameSinkId(0, 1)];
    frame_interval_inputs.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(35)});
    frame_interval_inputs.has_only_content_frame_interval_updates = true;

    ExpectResult(matcher.Match(inputs), base::Hertz(70),
                 ResultIntervalType::kExact);
  }

  // Verify the same (where the expected cadence = 3).
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
    FrameIntervalInputs& frame_interval_inputs =
        inputs.inputs_map[FrameSinkId(0, 1)];
    frame_interval_inputs.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(15)});
    frame_interval_inputs.has_only_content_frame_interval_updates = true;

    ExpectResult(matcher.Match(inputs), base::Hertz(45),
                 ResultIntervalType::kExact);
  }

  // Verify that the lowest perfect cadence (= 2) is chosen when the target
  // interval falls below the supported range.
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
    FrameIntervalInputs& frame_interval_inputs =
        inputs.inputs_map[FrameSinkId(0, 1)];
    frame_interval_inputs.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(160)});
    frame_interval_inputs.has_only_content_frame_interval_updates = true;

    ExpectResult(matcher.Match(inputs), base::Hertz(80),
                 ResultIntervalType::kExact);
  }

  // Verify the same (where the expected cadence = 4).
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
    FrameIntervalInputs& frame_interval_inputs =
        inputs.inputs_map[FrameSinkId(0, 1)];
    frame_interval_inputs.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(400)});
    frame_interval_inputs.has_only_content_frame_interval_updates = true;

    ExpectResult(matcher.Match(inputs), base::Hertz(100),
                 ResultIntervalType::kExact);
  }

  settings.interval_settings =
      BuildContinuousRangeSettings(base::Hertz(60), base::Hertz(48));

  // Verify that the maximum supported interval is chosen if there is no perfect
  // cadence when the target interval falls above the supported range.
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
    FrameIntervalInputs& frame_interval_inputs =
        inputs.inputs_map[FrameSinkId(0, 1)];
    frame_interval_inputs.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(40)});
    frame_interval_inputs.has_only_content_frame_interval_updates = true;

    ExpectResult(matcher.Match(inputs), base::Hertz(48),
                 ResultIntervalType::kExact);
  }

  // Verify that the maximum supported interval is chosen if there is no perfect
  // cadence when the target interval falls below the supported range.
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
    FrameIntervalInputs& frame_interval_inputs =
        inputs.inputs_map[FrameSinkId(0, 1)];
    frame_interval_inputs.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(80)});
    frame_interval_inputs.has_only_content_frame_interval_updates = true;

    ExpectResult(matcher.Match(inputs), base::Hertz(60),
                 ResultIntervalType::kExact);
  }
}

// Android configuration: bounded only by the display's max refresh rate.
TEST(FrameIntervalMatchersTest, OnlyVideoContinuousRangeUnboundedMax) {
  constexpr base::TimeDelta kMinInterval = base::Microseconds(8333);  // 120Hz.
  Settings settings;
  settings.interval_settings =
      BuildContinuousRangeSettings(kMinInterval, base::TimeDelta::Max());
  OnlyVideoMatcher matcher;

  auto match = [&](base::TimeDelta content_interval) {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
    FrameIntervalInputs& frame_interval_inputs =
        inputs.inputs_map[FrameSinkId(0, 1)];
    frame_interval_inputs.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, content_interval});
    frame_interval_inputs.has_only_content_frame_interval_updates = true;
    return matcher.Match(inputs);
  };

  // Content slower than the display max passes through unchanged, including
  // very slow content, since the max interval is unbounded.
  ExpectResult(match(base::Hertz(30)), base::Hertz(30),
               ResultIntervalType::kExact);
  ExpectResult(match(base::Hertz(60)), base::Hertz(60),
               ResultIntervalType::kExact);
  ExpectResult(match(base::Seconds(1)), base::Seconds(1),
               ResultIntervalType::kExact);

  // Content far faster than the display max (e.g. ~8849Hz) picks a cadence
  // just above the range minimum: ceil(8333 / 113) = 74, 74 * 113 = 8362us.
  ExpectResult(match(base::Microseconds(113)), base::Microseconds(8362),
               ResultIntervalType::kExact);

  // A zero content interval uses the range minimum.
  ExpectResult(match(base::TimeDelta()), kMinInterval,
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, VideoConferenceContinuousRangeUnboundedMax) {
  constexpr base::TimeDelta kMinInterval = base::Microseconds(8333);  // 120Hz.
  Settings settings;
  settings.interval_settings =
      BuildContinuousRangeSettings(kMinInterval, base::TimeDelta::Max());
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/3u);
  VideoConferenceMatcher matcher;

  FrameIntervalInputs& interval_inputs1 = inputs.inputs_map[FrameSinkId(0, 1)];
  interval_inputs1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(30)});
  FrameIntervalInputs& interval_inputs2 = inputs.inputs_map[FrameSinkId(0, 2)];
  interval_inputs2.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(1000)});

  // Minimum content interval faster than the display max is clamped.
  ExpectResult(matcher.Match(inputs), kMinInterval, ResultIntervalType::kExact);

  // Otherwise the minimum content interval passes through.
  interval_inputs2.content_interval_info[0].frame_interval = base::Hertz(24);
  ExpectResult(matcher.Match(inputs), base::Hertz(30),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, VideoConference) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/3u);
  VideoConferenceMatcher matcher;

  FrameIntervalInputs& interval_inputs1 = inputs.inputs_map[FrameSinkId(0, 1)];
  interval_inputs1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(32)});
  FrameIntervalInputs& interval_inputs2 = inputs.inputs_map[FrameSinkId(0, 2)];
  interval_inputs2.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(24)});
  ExpectResult(matcher.Match(inputs), base::Milliseconds(24),
               ResultIntervalType::kExact);

  interval_inputs2.content_interval_info.clear();
  ExpectNullResult(matcher.Match(inputs));
}

TEST(FrameIntervalMatchersTest, VideoConferenceFixedInterval) {
  Settings settings;
  settings.interval_settings = BuildDefaultFixedIntervalSettings();
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/3u);
  VideoConferenceMatcher matcher;

  FrameIntervalInputs& interval_inputs1 = inputs.inputs_map[FrameSinkId(0, 1)];
  interval_inputs1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(32)});
  FrameIntervalInputs& interval_inputs2 = inputs.inputs_map[FrameSinkId(0, 2)];
  interval_inputs2.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(24)});
  ExpectResult(matcher.Match(inputs), base::Milliseconds(16),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, VideoConferenceDenseFixedInterval) {
  Settings settings;
  FixedIntervalSettings fixed_interval_settings =
      BuildDenseFixedIntervalSettings();
  settings.interval_settings = fixed_interval_settings;
  VideoConferenceMatcher matcher;

  base::TimeDelta input1_interval = base::Hertz(59.95);
  base::TimeDelta input2_interval = base::Hertz(59.99);
  // Assert that each input interval is within epsilon to each supported
  // interval.
  for (base::TimeDelta supported_interval :
       fixed_interval_settings.supported_intervals) {
    ASSERT_LE((supported_interval - input1_interval).magnitude(),
              settings.epsilon);
    ASSERT_LE((supported_interval - input2_interval).magnitude(),
              settings.epsilon);
  }

  // Verify that the closest supported interval is chosen when there are
  // multiple options within epsilon.
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/3u);
    FrameIntervalInputs& interval_inputs1 =
        inputs.inputs_map[FrameSinkId(0, 1)];
    interval_inputs1.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, input1_interval});
    FrameIntervalInputs& interval_inputs2 =
        inputs.inputs_map[FrameSinkId(0, 2)];
    interval_inputs2.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, input1_interval});
    ExpectResult(matcher.Match(inputs), base::Hertz(59.94),
                 ResultIntervalType::kExact);
  }

  // Verify the same when the input interval is closer to the other side.
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/3u);
    FrameIntervalInputs& interval_inputs1 =
        inputs.inputs_map[FrameSinkId(0, 1)];
    interval_inputs1.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, input2_interval});
    FrameIntervalInputs& interval_inputs2 =
        inputs.inputs_map[FrameSinkId(0, 2)];
    interval_inputs2.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, input2_interval});
    ExpectResult(matcher.Match(inputs), base::Hertz(60),
                 ResultIntervalType::kExact);
  }
}

TEST(FrameIntervalMatchersTest, VideoConferenceDuplicateCount) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/3u);
  VideoConferenceMatcher matcher;

  FrameIntervalInputs& interval_inputs1 = inputs.inputs_map[FrameSinkId(0, 1)];
  interval_inputs1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(32), 2u});
  ExpectResult(matcher.Match(inputs), base::Milliseconds(32),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, VideoConferenceIgnoreOldSinks) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/3u);
  VideoConferenceMatcher matcher;

  FrameIntervalInputs& interval_inputs1 = inputs.inputs_map[FrameSinkId(0, 1)];
  interval_inputs1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(32)});
  FrameIntervalInputs& interval_inputs2 = inputs.inputs_map[FrameSinkId(0, 2)];
  interval_inputs2.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(24)});
  ExpectResult(matcher.Match(inputs), base::Milliseconds(24),
               ResultIntervalType::kExact);

  interval_inputs2.frame_time = kNow - base::Seconds(1);
  ExpectNullResult(matcher.Match(inputs));
}

TEST(FrameIntervalMatchersTest, VideoConferenceContinuousRange) {
  Settings settings;
  settings.interval_settings = BuildContinuousRangeSettings();
  VideoConferenceMatcher matcher;

  // Verify the minimum content interval is chosen when it falls within the
  // supported range.
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/3u);
    FrameIntervalInputs& interval_inputs1 =
        inputs.inputs_map[FrameSinkId(0, 1)];
    interval_inputs1.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(60)});
    FrameIntervalInputs& interval_inputs2 =
        inputs.inputs_map[FrameSinkId(0, 2)];
    interval_inputs2.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(50)});
    ExpectResult(matcher.Match(inputs), base::Hertz(60),
                 ResultIntervalType::kExact);
  }

  // Verify the minimum possible interval is chosen when the minimum content
  // interval falls below the supported range.
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/3u);
    FrameIntervalInputs& interval_inputs1 =
        inputs.inputs_map[FrameSinkId(0, 1)];
    interval_inputs1.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(150)});
    FrameIntervalInputs& interval_inputs2 =
        inputs.inputs_map[FrameSinkId(0, 2)];
    interval_inputs2.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(50)});
    ExpectResult(matcher.Match(inputs), base::Hertz(120),
                 ResultIntervalType::kExact);
  }

  // Verify the maximum possible interval is chosen when the minimum content
  // interval falls above the supported range.
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/3u);
    FrameIntervalInputs& interval_inputs1 =
        inputs.inputs_map[FrameSinkId(0, 1)];
    interval_inputs1.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(30)});
    FrameIntervalInputs& interval_inputs2 =
        inputs.inputs_map[FrameSinkId(0, 2)];
    interval_inputs2.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(35)});
    ExpectResult(matcher.Match(inputs), base::Hertz(40),
                 ResultIntervalType::kExact);
  }
}

// Regression test for https://crbug.com/371227621.
TEST(FrameIntervalMatcherInputsTest, WriteIntoTrace) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
  FrameIntervalInputs& interval_inputs1 = inputs.inputs_map[FrameSinkId(0, 1)];
  interval_inputs1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(32)});

  EXPECT_EQ(
      perfetto::TracedValueToString(inputs),
      "{FrameSinkId(0, 1):"
      "{time_diff_us:0,has_input:false,only_content:false,"
      "content_info_0:{type:video,interval_us:32000,duplicate_count:0}}}");
}

TEST(FrameIntervalMatchersTest, UserInputBoostMatcher) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  UserInputBoostMatcher matcher;

  inputs.inputs_map[FrameSinkId(0, 1)].has_user_input = true;
  ExpectResult(matcher.Match(inputs), FrameIntervalClass::kBoost);

  inputs.inputs_map[FrameSinkId(0, 1)].has_user_input = false;
  ExpectNullResult(matcher.Match(inputs));
}

TEST(FrameIntervalMatchersTest, SlowScrollThrottleSlowSpeedThrottle) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
  std::vector<mojom::FrameRateVelocityPoint> velocity_points;
  velocity_points.emplace_back(60, 0);
  velocity_points.emplace_back(80, 125);
  velocity_points.emplace_back(120, 300);
  SlowScrollThrottleMatcher matcher(/*device_scale_factor=*/1.0f,
                                    std::move(velocity_points));

  FrameIntervalInputs& interval_input = inputs.inputs_map[FrameSinkId(0, 1)];
  interval_input.content_interval_info.push_back(
      {ContentFrameIntervalType::kCompositorScroll, base::TimeDelta()});
  interval_input.has_only_content_frame_interval_updates = true;

  interval_input.major_scroll_speed_in_pixels_per_second = 400.0f;
  ExpectResult(matcher.Match(inputs), base::Hertz(120),
               ResultIntervalType::kAtLeast);

  interval_input.major_scroll_speed_in_pixels_per_second = 200.0f;
  ExpectResult(matcher.Match(inputs), base::Hertz(80),
               ResultIntervalType::kAtLeast);

  interval_input.major_scroll_speed_in_pixels_per_second = 10.0f;
  ExpectResult(matcher.Match(inputs), base::Hertz(60),
               ResultIntervalType::kAtLeast);

  interval_input.major_scroll_speed_in_pixels_per_second = 0.0f;
  ExpectNullResult(matcher.Match(inputs));
}

TEST(FrameIntervalMatchersTest, SlowScrollThrottleContinuousRange) {
  Settings settings;
  settings.interval_settings =
      BuildContinuousRangeSettings(base::Hertz(100), base::Hertz(50));
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
  std::vector<mojom::FrameRateVelocityPoint> velocity_points;
  velocity_points.emplace_back(30, 0);
  velocity_points.emplace_back(80, 125);
  velocity_points.emplace_back(120, 300);
  SlowScrollThrottleMatcher matcher(/*device_scale_factor=*/1.0f,
                                    std::move(velocity_points));

  FrameIntervalInputs& interval_input = inputs.inputs_map[FrameSinkId(0, 1)];
  interval_input.content_interval_info.push_back(
      {ContentFrameIntervalType::kCompositorScroll, base::TimeDelta()});
  interval_input.has_only_content_frame_interval_updates = true;

  // Clamped to min_interval (100Hz).
  interval_input.major_scroll_speed_in_pixels_per_second = 400.0f;
  ExpectResult(matcher.Match(inputs), base::Hertz(100),
               ResultIntervalType::kAtLeast);

  // Within range (80Hz).
  interval_input.major_scroll_speed_in_pixels_per_second = 200.0f;
  ExpectResult(matcher.Match(inputs), base::Hertz(80),
               ResultIntervalType::kAtLeast);

  // Clamped to max_interval (50Hz).
  interval_input.major_scroll_speed_in_pixels_per_second = 10.0f;
  ExpectResult(matcher.Match(inputs), base::Hertz(50),
               ResultIntervalType::kAtLeast);
}

TEST(FrameIntervalMatchersTest, SlowScrollThrottleIgnoreOneOffUpdate) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
  std::vector<mojom::FrameRateVelocityPoint> velocity_points;
  velocity_points.emplace_back(60, 0);
  velocity_points.emplace_back(80, 125);
  velocity_points.emplace_back(120, 300);
  SlowScrollThrottleMatcher matcher(/*device_scale_factor=*/1.0f,
                                    std::move(velocity_points));

  FrameIntervalInputs& interval_input = inputs.inputs_map[FrameSinkId(0, 1)];
  interval_input.content_interval_info.push_back(
      {ContentFrameIntervalType::kCompositorScroll, base::TimeDelta()});
  interval_input.has_only_content_frame_interval_updates = true;
  interval_input.major_scroll_speed_in_pixels_per_second = 10.0f;

  inputs.frame_id = 10u;
  ExpectResult(matcher.Match(inputs), base::Hertz(60),
               ResultIntervalType::kAtLeast);

  inputs.frame_id = 11u;
  interval_input.has_only_content_frame_interval_updates = false;
  // Ignore one off non-scroll update.
  ExpectResult(matcher.Match(inputs), base::Hertz(60),
               ResultIntervalType::kAtLeast);

  // Continuous non-scroll update should not match.
  inputs.frame_id = 12u;
  interval_input.has_only_content_frame_interval_updates = false;
  ExpectNullResult(matcher.Match(inputs));
  inputs.frame_id = 13u;
  interval_input.has_only_content_frame_interval_updates = false;
  ExpectNullResult(matcher.Match(inputs));

  // Match if there are no non-scroll updates.
  inputs.frame_id = 14u;
  interval_input.has_only_content_frame_interval_updates = true;
  ExpectResult(matcher.Match(inputs), base::Hertz(60),
               ResultIntervalType::kAtLeast);
}

TEST(FrameIntervalMatchersTest, MixedFixedInterval_SingleStream_Monostate) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
  MixedFixedIntervalMatcher matcher;

  // Single video stream passes through directly.
  FrameIntervalInputs& interval_input = inputs.inputs_map[FrameSinkId(0, 1)];
  interval_input.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(24)});
  interval_input.has_only_content_frame_interval_updates = true;

  ExpectResult(matcher.Match(inputs), base::Hertz(24),
               ResultIntervalType::kExact);

  // Single stepped animation stream also passes through directly.
  interval_input.content_interval_info.clear();
  interval_input.content_interval_info.push_back(
      {ContentFrameIntervalType::kSteppedCompositorAnimation, base::Hertz(15)});
  ExpectResult(matcher.Match(inputs), base::Hertz(15),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest,
     MixedFixedInterval_MultiAnimation_Cadence_Monostate) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Sink 1: 10fps GIF
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kAnimatingImage, base::Hertz(10)});
  sink1.has_only_content_frame_interval_updates = true;

  // Sink 2: 15fps stepped glow
  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kSteppedCompositorAnimation, base::Hertz(15)});
  sink2.has_only_content_frame_interval_updates = true;

  // Common cadence(10, 15) = 30Hz
  ExpectResult(matcher.Match(inputs), base::Hertz(30),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest,
     MixedFixedInterval_VideoPlusSteppedAnimation_Cadence_Monostate) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Sink 1: 24fps video
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(24)});
  sink1.has_only_content_frame_interval_updates = true;

  // Sink 2: 15fps stepped glow
  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kSteppedCompositorAnimation, base::Hertz(15)});
  sink2.has_only_content_frame_interval_updates = true;

  // Common cadence(24, 15) = 120Hz
  ExpectResult(matcher.Match(inputs), base::Hertz(120),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest,
     MixedFixedInterval_DiscreteFixedSettings_24fpsPlus15fps) {
  Settings settings;
  FixedIntervalSettings fixed_settings;
  fixed_settings.supported_intervals.insert(base::Hertz(60));
  fixed_settings.supported_intervals.insert(base::Hertz(120));
  fixed_settings.default_interval = base::Hertz(120);
  settings.interval_settings = fixed_settings;

  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Sink 1: 24fps video
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(24)});
  sink1.has_only_content_frame_interval_updates = true;

  // Sink 2: 15fps stepped glow
  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kSteppedCompositorAnimation, base::Hertz(15)});
  sink2.has_only_content_frame_interval_updates = true;

  // 60Hz fails 24fps cadence (3:2 pulldown); 120Hz satisfies both 24fps (5:5)
  // and 15fps (8:8).
  ExpectResult(matcher.Match(inputs), base::Hertz(120),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest,
     MixedFixedInterval_DiscreteFixedSettings_30fpsPlus15fps) {
  Settings settings;
  FixedIntervalSettings fixed_settings;
  fixed_settings.supported_intervals.insert(base::Hertz(30));
  fixed_settings.supported_intervals.insert(base::Hertz(60));
  fixed_settings.supported_intervals.insert(base::Hertz(120));
  fixed_settings.default_interval = base::Hertz(60);
  settings.interval_settings = fixed_settings;

  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Sink 1: 30fps video
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(30)});
  sink1.has_only_content_frame_interval_updates = true;

  // Sink 2: 15fps stepped glow
  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kSteppedCompositorAnimation, base::Hertz(15)});
  sink2.has_only_content_frame_interval_updates = true;

  // 30Hz satisfies both 30fps (1:1) and 15fps (2:2) and is the slowest refresh
  // rate (most power savings).
  ExpectResult(matcher.Match(inputs), base::Hertz(30),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, MixedFixedInterval_ContinuousRangeSettings) {
  Settings settings;
  settings.interval_settings = BuildContinuousRangeSettings(
      /*min_interval=*/base::Hertz(144), /*max_interval=*/base::Hertz(40));

  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
  MixedFixedIntervalMatcher matcher;

  // 24fps video (41.67ms) is too slow for 40Hz (25ms); cadence multiplier 2
  // gives 48Hz (20.83ms).
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(24)});
  sink1.has_only_content_frame_interval_updates = true;

  ExpectResult(matcher.Match(inputs), base::Hertz(48),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, MixedFixedInterval_IgnoreStaleFrameSink) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Sink 1: active 24fps video
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(24)});
  sink1.has_only_content_frame_interval_updates = true;

  // Sink 2: stale 15fps stepped glow (older than ignore_frame_sink_timeout)
  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.frame_time = kNow - base::Seconds(1);
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kSteppedCompositorAnimation, base::Hertz(15)});
  sink2.has_only_content_frame_interval_updates = true;

  // Only Sink 1 is active -> 24Hz
  ExpectResult(matcher.Match(inputs), base::Hertz(24),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest,
     MixedFixedInterval_NonContentDamage_ReturnsNullopt) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
  MixedFixedIntervalMatcher matcher;

  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(24)});
  sink1.has_only_content_frame_interval_updates = false;

  ExpectNullResult(matcher.Match(inputs));
}

TEST(FrameIntervalMatchersTest,
     MixedFixedInterval_CompositorScroll_ReturnsNullopt) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
  MixedFixedIntervalMatcher matcher;

  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kCompositorScroll, base::TimeDelta()});
  sink1.has_only_content_frame_interval_updates = true;

  ExpectNullResult(matcher.Match(inputs));
}

TEST(FrameIntervalMatchersTest,
     MixedFixedInterval_InvalidInterval_ReturnsNullopt) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
  MixedFixedIntervalMatcher matcher;

  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Microseconds(-100)});
  sink1.has_only_content_frame_interval_updates = true;

  ExpectNullResult(matcher.Match(inputs));
}

TEST(FrameIntervalMatchersTest,
     MixedFixedInterval_HarmonicCadenceExceedsContinuousRange_ReturnsNullopt) {
  Settings settings;
  settings.interval_settings = BuildContinuousRangeSettings(
      /*min_interval=*/base::Hertz(144), /*max_interval=*/base::Hertz(40));
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Sink 1: 24fps video
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(24)});
  sink1.has_only_content_frame_interval_updates = true;

  // Sink 2: 53fps stepped animation (1272 Hz harmonic cadence exceeds 144Hz VRR
  // maximum)
  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kSteppedCompositorAnimation, base::Hertz(53)});
  sink2.has_only_content_frame_interval_updates = true;

  ExpectNullResult(matcher.Match(inputs));
}

TEST(FrameIntervalMatchersTest, MixedFixedInterval_HighHarmonicRate_Monostate) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Sink 1: 24fps video
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(24)});
  sink1.has_only_content_frame_interval_updates = true;

  // Sink 2: 53fps stepped animation (1272 Hz harmonic cadence)
  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kSteppedCompositorAnimation, base::Hertz(53)});
  sink2.has_only_content_frame_interval_updates = true;

  // Unconstrained mode computes the full harmonic rate.
  ExpectResult(matcher.Match(inputs), base::Hertz(1272),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, MixedFixedInterval_FractionalNTSCVideo) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Sink 1: 23.976fps NTSC video (~41708us)
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Microseconds(41708)});
  sink1.has_only_content_frame_interval_updates = true;

  // Sink 2: 60fps stepped animation
  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kSteppedCompositorAnimation, base::Hertz(60)});
  sink2.has_only_content_frame_interval_updates = true;

  // 23.976 matches 24fps within epsilon. Common cadence(24, 60) = 120Hz.
  ExpectResult(matcher.Match(inputs), base::Hertz(120),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, MixedFixedInterval_ScrollBarFadeOutAnimation) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Sink 1: 24fps video
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(24)});
  sink1.has_only_content_frame_interval_updates = true;

  // Sink 2: 30fps scrollbar fade-out animation
  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kScrollBarFadeOutAnimation, base::Hertz(30)});
  sink2.has_only_content_frame_interval_updates = true;

  // Common cadence(24, 30) = 120Hz
  ExpectResult(matcher.Match(inputs), base::Hertz(120),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest,
     MixedFixedInterval_DeduplicateIdenticalIntervals) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Sink 1: 24fps video
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(24)});
  sink1.has_only_content_frame_interval_updates = true;

  // Sink 2: another 24fps stream (e.g. video or 24fps GIF)
  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kAnimatingImage, base::Hertz(24)});
  sink2.has_only_content_frame_interval_updates = true;

  // Identical intervals deduplicate within epsilon -> single 24Hz stream.
  ExpectResult(matcher.Match(inputs), base::Hertz(24),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, MixedFixedInterval_SubHzAnimation_Monostate) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Sink 1: 0.5fps (2000ms) stepped CSS animation
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kSteppedCompositorAnimation,
       base::Milliseconds(2000)});
  sink1.has_only_content_frame_interval_updates = true;

  // Sink 2: 30fps video
  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(30)});
  sink2.has_only_content_frame_interval_updates = true;

  // 0.5fps step is an exact integer multiple (60x) of 30fps -> 30Hz
  ExpectResult(matcher.Match(inputs), base::Hertz(30),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest, MixedFixedInterval_SubHzAnimation_Discrete) {
  Settings settings;
  FixedIntervalSettings fixed_settings;
  fixed_settings.supported_intervals.insert(base::Hertz(30));
  fixed_settings.supported_intervals.insert(base::Hertz(60));
  fixed_settings.supported_intervals.insert(base::Hertz(120));
  fixed_settings.default_interval = base::Hertz(60);
  settings.interval_settings = fixed_settings;

  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Sink 1: 0.5fps (2000ms) stepped CSS animation
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kSteppedCompositorAnimation,
       base::Milliseconds(2000)});
  sink1.has_only_content_frame_interval_updates = true;

  // Sink 2: 30fps video
  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(30)});
  sink2.has_only_content_frame_interval_updates = true;

  // 30Hz satisfies both 0.5fps (60:1) and 30fps (1:1) and saves power
  ExpectResult(matcher.Match(inputs), base::Hertz(30),
               ResultIntervalType::kExact);
}

TEST(FrameIntervalMatchersTest,
     MixedFixedInterval_PathologicalIncommensurate_TerminatesPromptly) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/2u);
  MixedFixedIntervalMatcher matcher;

  // Pathological pair of slow/incommensurate prime frequencies with no simple
  // cadence within kMaxHarmonicMultiple=32 harmonics (e.g. 47 Hz and 53 Hz). In
  // monostate (min_supported_interval is 0), without kMaxHarmonicMultiple this
  // would iterate excessively (until n=47). With the cap, it terminates
  // promptly and returns nullopt.
  FrameIntervalInputs& sink1 = inputs.inputs_map[FrameSinkId(0, 1)];
  sink1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Hertz(47)});
  sink1.has_only_content_frame_interval_updates = true;

  FrameIntervalInputs& sink2 = inputs.inputs_map[FrameSinkId(0, 2)];
  sink2.content_interval_info.push_back(
      {ContentFrameIntervalType::kSteppedCompositorAnimation, base::Hertz(53)});
  sink2.has_only_content_frame_interval_updates = true;

  EXPECT_FALSE(matcher.Match(inputs).has_value());
}

}  // namespace
}  // namespace viz
