// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/session_logger.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "components/mirroring/service/value_util.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/frame_id.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/stats_event_subscriber.h"
#include "media/cast/test/fake_receiver_time_offset_estimator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::media::FakeSingleThreadTaskRunner;
using ::media::cast::AUDIO_EVENT;
using ::media::cast::CastEnvironment;
using ::media::cast::EventMediaType;
using ::media::cast::FRAME_CAPTURE_BEGIN;
using ::media::cast::FRAME_CAPTURE_END;
using ::media::cast::FRAME_ENCODED;
using ::media::cast::FrameEvent;
using ::media::cast::FrameId;
using ::media::cast::RtpTimeDelta;
using ::media::cast::RtpTimeTicks;
using ::media::cast::StatsEventSubscriber;
using ::media::cast::VIDEO_EVENT;
using ::media::cast::test::FakeReceiverTimeOffsetEstimator;

namespace mirroring {

namespace {
const int kReceiverOffsetSecs = 100;
const size_t kMaxFrameInfoMapSize = 100UL;
}  // namespace

class SessionLoggerTest : public ::testing::Test {
 public:
  SessionLoggerTest()
      : task_runner_(new FakeSingleThreadTaskRunner(&sender_clock_)),
        cast_environment_(new CastEnvironment(&sender_clock_,
                                              task_runner_,
                                              task_runner_,
                                              task_runner_)),
        fake_offset_estimator_(
            std::make_unique<FakeReceiverTimeOffsetEstimator>(
                base::Seconds(kReceiverOffsetSecs))) {
    receiver_clock_.Advance(base::Seconds(kReceiverOffsetSecs));
  }

  SessionLoggerTest(const SessionLoggerTest&) = delete;
  SessionLoggerTest& operator=(const SessionLoggerTest&) = delete;

  ~SessionLoggerTest() override = default;

 protected:
  void AdvanceClocks(base::TimeDelta delta) {
    task_runner_->Sleep(delta);
    receiver_clock_.Advance(delta);
  }

  void Init() {
    DCHECK(!session_logger_.get());
    session_logger_ = std::make_unique<SessionLogger>(
        cast_environment_, std::move(fake_offset_estimator_));
  }

  void DispatchFrameEvent(std::unique_ptr<FrameEvent> frame_event) {
    cast_environment_->logger()->DispatchFrameEvent(std::move(frame_event));
  }

  std::unique_ptr<FrameEvent> ConstructCaptureBeginEvent(
      EventMediaType event_media_type,
      RtpTimeTicks rtp_timestamp) {
    std::unique_ptr<FrameEvent> capture_begin_event =
        std::make_unique<FrameEvent>();
    capture_begin_event->timestamp = sender_clock_.NowTicks();
    capture_begin_event->type = FRAME_CAPTURE_BEGIN;
    capture_begin_event->media_type = event_media_type;
    capture_begin_event->rtp_timestamp = rtp_timestamp;

    return capture_begin_event;
  }

  std::unique_ptr<FrameEvent> ConstructCaptureEndEvent(
      EventMediaType event_media_type,
      RtpTimeTicks rtp_timestamp) {
    std::unique_ptr<FrameEvent> capture_end_event =
        std::make_unique<FrameEvent>();
    capture_end_event->timestamp = sender_clock_.NowTicks();
    capture_end_event->type = FRAME_CAPTURE_END;
    capture_end_event->media_type = event_media_type;
    capture_end_event->rtp_timestamp = rtp_timestamp;

    return capture_end_event;
  }

  std::unique_ptr<FrameEvent> ConstructEncodeEvent(
      EventMediaType event_media_type,
      RtpTimeTicks rtp_timestamp,
      FrameId frame_id) {
    std::unique_ptr<FrameEvent> encode_event = std::make_unique<FrameEvent>();
    encode_event->timestamp = sender_clock_.NowTicks();
    encode_event->type = FRAME_ENCODED;
    encode_event->media_type = event_media_type;
    encode_event->rtp_timestamp = rtp_timestamp;
    encode_event->frame_id = frame_id;
    encode_event->size = 1024;
    encode_event->key_frame = true;
    encode_event->target_bitrate = 5678;
    encode_event->encoder_cpu_utilization = 9.10;
    encode_event->idealized_bitrate_utilization = 11.12;

    return encode_event;
  }

  base::SimpleTestTickClock sender_clock_;
  base::SimpleTestTickClock receiver_clock_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  std::unique_ptr<FakeReceiverTimeOffsetEstimator> fake_offset_estimator_;
  std::unique_ptr<SessionLogger> session_logger_;
};

TEST_F(SessionLoggerTest, CaptureEncode) {
  // Using RTP events, test that the StatEventSubscribers are parsing and
  // storing statistics.
  Init();

  RtpTimeTicks rtp_timestamp;
  FrameId frame_id = FrameId::first();

  const int num_frames = kMaxFrameInfoMapSize + 50;

  // Drop half the frames during the encode step.
  for (int i = 0; i < num_frames; i++) {
    DispatchFrameEvent(ConstructCaptureBeginEvent(VIDEO_EVENT, rtp_timestamp));
    DispatchFrameEvent(ConstructCaptureBeginEvent(AUDIO_EVENT, rtp_timestamp));

    AdvanceClocks(base::Microseconds(10));

    DispatchFrameEvent(ConstructCaptureEndEvent(VIDEO_EVENT, rtp_timestamp));
    DispatchFrameEvent(ConstructCaptureEndEvent(AUDIO_EVENT, rtp_timestamp));

    if (i % 2 == 0) {
      AdvanceClocks(base::Microseconds(10));

      DispatchFrameEvent(
          ConstructEncodeEvent(VIDEO_EVENT, rtp_timestamp, frame_id));
      DispatchFrameEvent(
          ConstructEncodeEvent(AUDIO_EVENT, rtp_timestamp, frame_id));
    }
    AdvanceClocks(base::Microseconds(34567));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
    frame_id++;
  }

  auto stats_dict = session_logger_->GetStats();

  // Check that the GetStats() dict has been populated with audio stats.
  const base::Value::Dict* audio_dict =
      stats_dict.FindDict(StatsEventSubscriber::kAudioStatsDictKey);
  EXPECT_TRUE(audio_dict);
  EXPECT_TRUE(audio_dict->contains("CAPTURE_FPS"));
  EXPECT_TRUE(audio_dict->FindDouble("CAPTURE_FPS").has_value());

  EXPECT_TRUE(audio_dict->contains("NUM_FRAMES_CAPTURED"));
  EXPECT_TRUE(audio_dict->FindDouble("NUM_FRAMES_CAPTURED").has_value());

  EXPECT_TRUE(audio_dict->contains("NUM_FRAMES_DROPPED_BY_ENCODER"));
  EXPECT_TRUE(
      audio_dict->FindDouble("NUM_FRAMES_DROPPED_BY_ENCODER").has_value());

  EXPECT_TRUE(audio_dict->contains("AVG_CAPTURE_LATENCY_MS"));
  EXPECT_TRUE(audio_dict->FindDouble("AVG_CAPTURE_LATENCY_MS").has_value());

  // Check that the GetStats() dict has been populated with video stats.
  const base::Value::Dict* video_dict =
      stats_dict.FindDict(StatsEventSubscriber::kVideoStatsDictKey);
  EXPECT_TRUE(video_dict);
  EXPECT_TRUE(video_dict->contains("CAPTURE_FPS"));
  EXPECT_TRUE(video_dict->FindDouble("CAPTURE_FPS").has_value());

  EXPECT_TRUE(video_dict->contains("NUM_FRAMES_CAPTURED"));
  EXPECT_TRUE(video_dict->FindDouble("NUM_FRAMES_CAPTURED").has_value());

  EXPECT_TRUE(video_dict->contains("NUM_FRAMES_DROPPED_BY_ENCODER"));
  EXPECT_TRUE(
      video_dict->FindDouble("NUM_FRAMES_DROPPED_BY_ENCODER").has_value());

  EXPECT_TRUE(video_dict->contains("AVG_CAPTURE_LATENCY_MS"));
  EXPECT_TRUE(video_dict->FindDouble("AVG_CAPTURE_LATENCY_MS").has_value());
}

}  // namespace mirroring
