// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/starboard_buffering_tracker.h"

#include <functional>
#include <string>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/base/metrics/mock_cast_metrics_helper.h"
#include "chromecast/starboard/media/media/mock_starboard_api_wrapper.h"
#include "media/base/mock_filters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::chromecast::metrics::CastMetricsHelper;
using ::chromecast::metrics::MockCastMetricsHelper;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::WithArg;

// A test fixture is used to handle the creation and lifetime of a
// TaskEnvironment, and to construct various mocks.
class StarboardBufferingTrackerTest : public testing::Test {
 protected:
  // This must be destructed last.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockCastMetricsHelper metrics_helper_;
};

// Returns the current media time. This can be bound to a local variable's
// address to precisely modify the media time during a test.
base::TimeDelta GetCurrentMediaTime(base::TimeDelta* current_media_time) {
  CHECK(current_media_time != nullptr);
  return *current_media_time;
}

TEST_F(StarboardBufferingTrackerTest,
       ComputesBufferingTimeBeforePlaybackStarts) {
  constexpr auto kInitialBufferingTime = base::Seconds(5);
  base::TimeDelta current_media_time = base::Seconds(0);

  EXPECT_CALL(
      metrics_helper_,
      LogTimeToBufferAv(CastMetricsHelper::BufferingType::kInitialBuffering,
                        kInitialBufferingTime))
      .Times(1);

  StarboardBufferingTracker tracker(
      base::BindRepeating(&GetCurrentMediaTime, &current_media_time),
      &metrics_helper_);
  tracker.OnPlayerStatus(StarboardPlayerState::kStarboardPlayerStatePrerolling);
  task_environment_.FastForwardBy(kInitialBufferingTime);
  tracker.OnPlayerStatus(StarboardPlayerState::kStarboardPlayerStatePresenting);
}

TEST_F(StarboardBufferingTrackerTest, ComputesBufferingTimeAfterUnderrun) {
  // To test the "buffering after underrun" scenario, we put the SbPlayer in the
  // "presenting" state and push three buffers. Playback starts at some point
  // after the second buffer is pushed; since the clock will be greater than
  // kMediaTime, it means that there was some period of time when content was
  // not playing (meaning playback was stuck buffering).
  //
  // To give a concrete example: suppose 10 real seconds pass, but the media
  // time only increased by 3 seconds. That means that 7 seconds were spent
  // buffering (this assumes that the playback speed is accurate. A separate
  // test covers the case where playback speed != 1).

  constexpr auto kInitialMediaTime = base::Seconds(150);
  constexpr auto kFinalMediaTime = base::Seconds(160);
  constexpr auto kUnderrunBufferingTime = base::Seconds(3);

  constexpr auto kTotalElapsedTime =
      kFinalMediaTime - kInitialMediaTime + kUnderrunBufferingTime;

  base::TimeDelta current_media_time = kInitialMediaTime;

  EXPECT_CALL(
      metrics_helper_,
      LogTimeToBufferAv(CastMetricsHelper::BufferingType::kInitialBuffering, _))
      .Times(1);
  EXPECT_CALL(metrics_helper_,
              LogTimeToBufferAv(
                  CastMetricsHelper::BufferingType::kBufferingAfterUnderrun,
                  kUnderrunBufferingTime))
      .Times(1);
  // Ignore metrics that are unrelated to this test.
  EXPECT_CALL(metrics_helper_, RecordApplicationEventWithValue(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(metrics_helper_, RecordApplicationEventWithValue(
                                   StrEq("Cast.Platform.AutoPauseTime"),
                                   kUnderrunBufferingTime.InMilliseconds()))
      .Times(1);
  EXPECT_CALL(metrics_helper_,
              RecordApplicationEventWithValue(
                  StrEq("Cast.Platform.PlayTimeBeforeAutoPause"),
                  kTotalElapsedTime.InMilliseconds()));

  StarboardBufferingTracker tracker(
      base::BindRepeating(&GetCurrentMediaTime, &current_media_time),
      &metrics_helper_);

  // Simulate prerolling and playback starting.
  tracker.OnPlayerStatus(StarboardPlayerState::kStarboardPlayerStatePrerolling);
  task_environment_.FastForwardBy(base::Seconds(1));
  tracker.OnPlayerStatus(StarboardPlayerState::kStarboardPlayerStatePresenting);

  // Simulate three buffers being pushed. Realistically there would be a delay
  // between the first two buffers, but it should not matter for the sake of the
  // logic being tested. What matters is the time reported by the clock when the
  // media time changes (when the third buffer is pushed and GetPlayerInfo is
  // called for the third time).
  tracker.OnBufferPush();
  tracker.OnBufferPush();

  current_media_time = kFinalMediaTime;
  task_environment_.FastForwardBy(kFinalMediaTime - kInitialMediaTime +
                                  kUnderrunBufferingTime);
  tracker.OnBufferPush();
}

TEST_F(StarboardBufferingTrackerTest,
       ComputesBufferingTimeAfterUnderrunForPlaybackRateBelow1) {
  // Same as ComputesBufferingTimeAfterUnderrun, but with a slower playback
  // rate.
  constexpr double kPlaybackRate = 0.5;

  constexpr auto kInitialMediaTime = base::Seconds(150);
  constexpr auto kFinalMediaTime = base::Seconds(160);
  constexpr auto kUnderrunBufferingTime = base::Seconds(4);

  constexpr auto kTotalElapsedTime =
      (kFinalMediaTime - kInitialMediaTime) / kPlaybackRate +
      kUnderrunBufferingTime;

  base::TimeDelta current_media_time = kInitialMediaTime;

  EXPECT_CALL(
      metrics_helper_,
      LogTimeToBufferAv(CastMetricsHelper::BufferingType::kInitialBuffering, _))
      .Times(1);
  EXPECT_CALL(metrics_helper_,
              LogTimeToBufferAv(
                  CastMetricsHelper::BufferingType::kBufferingAfterUnderrun,
                  kUnderrunBufferingTime))
      .Times(1);
  // Ignore metrics that are unrelated to this test.
  EXPECT_CALL(metrics_helper_, RecordApplicationEventWithValue(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(metrics_helper_, RecordApplicationEventWithValue(
                                   StrEq("Cast.Platform.AutoPauseTime"),
                                   kUnderrunBufferingTime.InMilliseconds()))
      .Times(1);
  EXPECT_CALL(metrics_helper_,
              RecordApplicationEventWithValue(
                  StrEq("Cast.Platform.PlayTimeBeforeAutoPause"),
                  kTotalElapsedTime.InMilliseconds()));

  StarboardBufferingTracker tracker(
      base::BindRepeating(&GetCurrentMediaTime, &current_media_time),
      &metrics_helper_);
  tracker.SetPlaybackRate(kPlaybackRate);

  // Simulate prerolling and playback starting.
  tracker.OnPlayerStatus(StarboardPlayerState::kStarboardPlayerStatePrerolling);
  task_environment_.FastForwardBy(base::Seconds(1));
  tracker.OnPlayerStatus(StarboardPlayerState::kStarboardPlayerStatePresenting);

  // Simulate three buffers being pushed.
  tracker.OnBufferPush();
  tracker.OnBufferPush();

  current_media_time = kFinalMediaTime;
  task_environment_.FastForwardBy((kFinalMediaTime - kInitialMediaTime) /
                                      kPlaybackRate +
                                  kUnderrunBufferingTime);
  tracker.OnBufferPush();
}

TEST_F(StarboardBufferingTrackerTest,
       ComputesBufferingTimeAfterUnderrunForPlaybackRateAbove1) {
  // Same as ComputesBufferingTimeAfterUnderrun, but with a faster playback
  // rate.
  constexpr double kPlaybackRate = 1.5;

  constexpr auto kInitialMediaTime = base::Seconds(150);
  constexpr auto kFinalMediaTime = base::Seconds(160);
  constexpr auto kUnderrunBufferingTime = base::Seconds(2);

  constexpr auto kTotalElapsedTime =
      (kFinalMediaTime - kInitialMediaTime) / kPlaybackRate +
      kUnderrunBufferingTime;

  base::TimeDelta current_media_time = kInitialMediaTime;

  EXPECT_CALL(
      metrics_helper_,
      LogTimeToBufferAv(CastMetricsHelper::BufferingType::kInitialBuffering, _))
      .Times(1);
  EXPECT_CALL(metrics_helper_,
              LogTimeToBufferAv(
                  CastMetricsHelper::BufferingType::kBufferingAfterUnderrun,
                  kUnderrunBufferingTime))
      .Times(1);
  // Ignore metrics that are unrelated to this test.
  EXPECT_CALL(metrics_helper_, RecordApplicationEventWithValue(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(metrics_helper_, RecordApplicationEventWithValue(
                                   StrEq("Cast.Platform.AutoPauseTime"),
                                   kUnderrunBufferingTime.InMilliseconds()))
      .Times(1);
  EXPECT_CALL(metrics_helper_,
              RecordApplicationEventWithValue(
                  StrEq("Cast.Platform.PlayTimeBeforeAutoPause"),
                  kTotalElapsedTime.InMilliseconds()));

  StarboardBufferingTracker tracker(
      base::BindRepeating(&GetCurrentMediaTime, &current_media_time),
      &metrics_helper_);
  tracker.SetPlaybackRate(kPlaybackRate);

  // Simulate prerolling and playback starting.
  tracker.OnPlayerStatus(StarboardPlayerState::kStarboardPlayerStatePrerolling);
  task_environment_.FastForwardBy(base::Seconds(1));
  tracker.OnPlayerStatus(StarboardPlayerState::kStarboardPlayerStatePresenting);

  // Simulate three buffers being pushed.
  tracker.OnBufferPush();
  tracker.OnBufferPush();

  current_media_time = kFinalMediaTime;
  task_environment_.FastForwardBy((kFinalMediaTime - kInitialMediaTime) /
                                      kPlaybackRate +
                                  kUnderrunBufferingTime);
  tracker.OnBufferPush();
}

TEST_F(StarboardBufferingTrackerTest, DoesNotCountPausedTimeAsBuffering) {
  // A playback rate of 0 means that playback is paused.
  constexpr auto kInitialMediaTime = base::Seconds(100);
  constexpr auto kFinalMediaTime = base::Seconds(105);

  base::TimeDelta current_media_time = kInitialMediaTime;

  // Prerolling still counts as "initial buffering".
  EXPECT_CALL(
      metrics_helper_,
      LogTimeToBufferAv(CastMetricsHelper::BufferingType::kInitialBuffering, _))
      .Times(1);

  // This scenario should NOT count as buffering, since the playback rate will
  // be set to 0.
  EXPECT_CALL(metrics_helper_,
              LogTimeToBufferAv(
                  CastMetricsHelper::BufferingType::kBufferingAfterUnderrun, _))
      .Times(0);
  // Ignore metrics that are unrelated to this test.
  EXPECT_CALL(metrics_helper_, RecordApplicationEventWithValue(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(metrics_helper_, RecordApplicationEventWithValue(
                                   StrEq("Cast.Platform.AutoPauseTime"), _))
      .Times(0);
  EXPECT_CALL(metrics_helper_,
              RecordApplicationEventWithValue(
                  StrEq("Cast.Platform.PlayTimeBeforeAutoPause"), _))
      .Times(0);

  StarboardBufferingTracker tracker(
      base::BindRepeating(&GetCurrentMediaTime, &current_media_time),
      &metrics_helper_);

  // Content is paused.
  tracker.SetPlaybackRate(0.0);

  // Simulate prerolling and playback starting.
  tracker.OnPlayerStatus(StarboardPlayerState::kStarboardPlayerStatePrerolling);
  task_environment_.FastForwardBy(base::Seconds(1));
  tracker.OnPlayerStatus(StarboardPlayerState::kStarboardPlayerStatePresenting);

  // Simulate three buffers being pushed. Realistically there would be a delay
  // between the first two buffers, but it should not matter for the sake of the
  // logic being tested. What matters is the time reported by the clock when the
  // media time changes (when the third buffer is pushed and GetPlayerInfo is
  // called for the third time).
  tracker.OnBufferPush();
  tracker.OnBufferPush();

  // Simulate the user unpausing 20s later.
  task_environment_.FastForwardBy(base::Seconds(20));
  tracker.SetPlaybackRate(0.0);

  // Simulate some time passing and the next buffer being pushed. Note that the
  // media time and the "real" (mock) time are equal.
  current_media_time = kFinalMediaTime;
  task_environment_.FastForwardBy(kFinalMediaTime - kInitialMediaTime);
  tracker.OnBufferPush();
}

TEST_F(StarboardBufferingTrackerTest, ComputesInitialBufferingDuration) {
  constexpr auto kInitialMediaTime = base::Seconds(0);
  constexpr auto kInitialBufferingDuration = base::Seconds(2);
  base::TimeDelta current_media_time = kInitialMediaTime;

  EXPECT_CALL(
      metrics_helper_,
      LogTimeToBufferAv(CastMetricsHelper::BufferingType::kInitialBuffering, _))
      .Times(1);
  EXPECT_CALL(metrics_helper_, RecordApplicationEventWithValue(
                                   StrEq("Cast.Platform.InitialBufferingTime"),
                                   kInitialBufferingDuration.InMilliseconds()))
      .Times(1);

  StarboardBufferingTracker tracker(
      base::BindRepeating(&GetCurrentMediaTime, &current_media_time),
      &metrics_helper_);

  // Simulate prerolling and playback starting.
  tracker.OnPlayerStatus(StarboardPlayerState::kStarboardPlayerStatePrerolling);
  task_environment_.FastForwardBy(kInitialBufferingDuration);
  tracker.OnPlayerStatus(StarboardPlayerState::kStarboardPlayerStatePresenting);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
