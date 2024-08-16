// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_interval_matchers.h"

#include <optional>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

using FrameIntervalClass = FrameIntervalMatcher::FrameIntervalClass;
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
  ASSERT_TRUE(absl::holds_alternative<FrameIntervalClass>(result));
  EXPECT_EQ(frame_interval_class, absl::get<FrameIntervalClass>(result));
}

void ExpectResult(const std::optional<Result> result_opt,
                  base::TimeDelta interval) {
  ASSERT_TRUE(result_opt.has_value());
  const Result& result = result_opt.value();
  ASSERT_TRUE(absl::holds_alternative<base::TimeDelta>(result));
  EXPECT_EQ(interval, absl::get<base::TimeDelta>(result));
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
  Inputs inputs(settings);

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
  ExpectResult(matcher.Match(inputs), base::Milliseconds(8));

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

  ExpectResult(matcher.Match(inputs), base::Milliseconds(32));

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

  ExpectResult(matcher.Match(inputs), base::Milliseconds(8));
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
  ExpectResult(matcher.Match(inputs), base::Milliseconds(16));
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
  ExpectResult(matcher.Match(inputs), base::Milliseconds(32));
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

    ExpectResult(matcher.Match(inputs), base::Hertz(60));
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

    ExpectResult(matcher.Match(inputs), base::Hertz(70));
  }

  // Verify the same (where the expected cadence = 3).
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
    FrameIntervalInputs& frame_interval_inputs =
        inputs.inputs_map[FrameSinkId(0, 1)];
    frame_interval_inputs.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(15)});
    frame_interval_inputs.has_only_content_frame_interval_updates = true;

    ExpectResult(matcher.Match(inputs), base::Hertz(45));
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

    ExpectResult(matcher.Match(inputs), base::Hertz(80));
  }

  // Verify the same (where the expected cadence = 4).
  {
    Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/1u);
    FrameIntervalInputs& frame_interval_inputs =
        inputs.inputs_map[FrameSinkId(0, 1)];
    frame_interval_inputs.content_interval_info.push_back(
        {ContentFrameIntervalType::kVideo, base::Hertz(400)});
    frame_interval_inputs.has_only_content_frame_interval_updates = true;

    ExpectResult(matcher.Match(inputs), base::Hertz(100));
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

    ExpectResult(matcher.Match(inputs), base::Hertz(48));
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

    ExpectResult(matcher.Match(inputs), base::Hertz(60));
  }
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
  ExpectResult(matcher.Match(inputs), base::Milliseconds(24));

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
  ExpectResult(matcher.Match(inputs), base::Milliseconds(16));
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
    ExpectResult(matcher.Match(inputs), base::Hertz(59.94));
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
    ExpectResult(matcher.Match(inputs), base::Hertz(60));
  }
}

TEST(FrameIntervalMatchersTest, VideoConferenceDuplicateCount) {
  Settings settings;
  Inputs inputs = BuildDefaultInputs(settings, /*num_sinks=*/3u);
  VideoConferenceMatcher matcher;

  FrameIntervalInputs& interval_inputs1 = inputs.inputs_map[FrameSinkId(0, 1)];
  interval_inputs1.content_interval_info.push_back(
      {ContentFrameIntervalType::kVideo, base::Milliseconds(32), 2u});
  ExpectResult(matcher.Match(inputs), base::Milliseconds(32));
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
  ExpectResult(matcher.Match(inputs), base::Milliseconds(24));

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
    ExpectResult(matcher.Match(inputs), base::Hertz(60));
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
    ExpectResult(matcher.Match(inputs), base::Hertz(120));
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
    ExpectResult(matcher.Match(inputs), base::Hertz(40));
  }
}

}  // namespace
}  // namespace viz
