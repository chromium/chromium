// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/rtp_stream.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/mock_filters.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/sender/audio_sender.h"
#include "media/cast/sender/video_sender.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::cast::TestAudioBusFactory;
using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;

namespace mirroring {

namespace {

class StreamClient final : public RtpStreamClient {
 public:
  explicit StreamClient(base::SimpleTestTickClock* clock) : clock_(clock) {}

  StreamClient(const StreamClient&) = delete;
  StreamClient& operator=(const StreamClient&) = delete;

  ~StreamClient() override = default;

  void SetVideoRtpStream(VideoRtpStream* stream) { video_stream_ = stream; }
  void SetShouldDeliverFrames(bool should_deliver) {
    should_deliver_frames_ = should_deliver;
  }

  // RtpStreamClient implementation.
  void OnError(const std::string& message) override {}
  void RequestRefreshFrame() override {
    if (video_stream_ && should_deliver_frames_) {
      video_stream_->InsertVideoFrame(CreateVideoFrame());
    }
  }
  void CreateVideoEncodeAccelerator(
      media::cast::ReceiveVideoEncodeAcceleratorCallback callback) override {}

  scoped_refptr<media::VideoFrame> CreateVideoFrame() {
    constexpr gfx::Size kFrameSize(640, 480);

    base::TimeDelta frame_timestamp;
    if (first_frame_time_.is_null()) {
      first_frame_time_ = clock_->NowTicks();
      frame_timestamp = base::TimeDelta();
    } else {
      clock_->Advance(base::Milliseconds(10));
      frame_timestamp = clock_->NowTicks() - first_frame_time_;
    }

    auto frame = media::VideoFrame::CreateFrame(
        media::PIXEL_FORMAT_I420, kFrameSize, gfx::Rect(kFrameSize), kFrameSize,
        frame_timestamp);
    media::cast::PopulateVideoFrame(frame.get(), 1);
    frame->metadata().reference_time = clock_->NowTicks();
    return frame;
  }

  base::WeakPtr<RtpStreamClient> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<VideoRtpStream> video_stream_;
  bool should_deliver_frames_ = true;
  base::TimeTicks first_frame_time_;
  const raw_ptr<base::SimpleTestTickClock> clock_;
  base::WeakPtrFactory<StreamClient> weak_factory_{this};
};

class MockVideoSender : public media::cast::VideoSender {
 public:
  MockVideoSender() = default;
  MOCK_METHOD(void,
              InsertRawVideoFrame,
              (scoped_refptr<media::VideoFrame>, base::TimeTicks),
              (override));
};

class MockAudioSender : public media::cast::AudioSender {
 public:
  MockAudioSender() = default;
  MOCK_METHOD(void,
              InsertAudio,
              (std::unique_ptr<media::AudioBus>, base::TimeTicks),
              (override));
};

}  // namespace

class RtpStreamTest : public ::testing::Test {
 public:
  RtpStreamTest()
      : cast_environment_(new media::cast::CastEnvironment(
            &testing_clock_,
            task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMainThreadTaskRunner())),
        client_(&testing_clock_) {
    testing_clock_.Advance(base::TimeTicks::Now() - base::TimeTicks());
  }

  RtpStreamTest(const RtpStreamTest&) = delete;
  RtpStreamTest& operator=(const RtpStreamTest&) = delete;

  ~RtpStreamTest() override { task_environment_.RunUntilIdle(); }

 protected:
  void ExpectTimerRunning(const VideoRtpStream& video_stream) {
    EXPECT_TRUE(video_stream.refresh_timer_.IsRunning());
  }

  void ExpectTimerNotRunning(const VideoRtpStream& video_stream) {
    EXPECT_FALSE(video_stream.refresh_timer_.IsRunning());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::SimpleTestTickClock testing_clock_;
  const scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  StreamClient client_;
};

// Test the video streaming pipeline.
TEST_F(RtpStreamTest, VideoStreaming) {
  auto video_sender = std::make_unique<MockVideoSender>();
  EXPECT_CALL(*video_sender, InsertRawVideoFrame(_, _)).Times(1);
  VideoRtpStream video_stream(std::move(video_sender), client_.GetWeakPtr(),
                              base::Milliseconds(1));
  client_.SetVideoRtpStream(&video_stream);
  video_stream.InsertVideoFrame(client_.CreateVideoFrame());
  ExpectTimerRunning(video_stream);
  client_.SetVideoRtpStream(nullptr);
}

TEST_F(RtpStreamTest, VideoStreamEmitsFramesWhenNoUpdates) {
  auto video_sender = std::make_unique<MockVideoSender>();
  EXPECT_CALL(*video_sender, InsertRawVideoFrame(_, _)).Times(6);
  VideoRtpStream video_stream(std::move(video_sender), client_.GetWeakPtr(),
                              base::Milliseconds(1));
  client_.SetVideoRtpStream(&video_stream);
  video_stream.InsertVideoFrame(client_.CreateVideoFrame());
  task_environment_.FastForwardBy(base::Milliseconds(5));
  ExpectTimerRunning(video_stream);
  client_.SetVideoRtpStream(nullptr);
}

TEST_F(RtpStreamTest, VideoStreamDoesNotRefreshWithZeroInterval) {
  auto video_sender = std::make_unique<MockVideoSender>();
  EXPECT_CALL(*video_sender, InsertRawVideoFrame(_, _)).Times(1);
  VideoRtpStream video_stream(std::move(video_sender), client_.GetWeakPtr(),
                              base::TimeDelta());
  client_.SetVideoRtpStream(&video_stream);
  video_stream.InsertVideoFrame(client_.CreateVideoFrame());
  task_environment_.FastForwardBy(base::Milliseconds(5));
  ExpectTimerNotRunning(video_stream);
  client_.SetVideoRtpStream(nullptr);
}

TEST_F(RtpStreamTest, VideoStreamTimerNotRunningWhenNoFramesDelivered) {
  auto video_sender = std::make_unique<MockVideoSender>();
  EXPECT_CALL(*video_sender, InsertRawVideoFrame(_, _)).Times(1);
  VideoRtpStream video_stream(std::move(video_sender), client_.GetWeakPtr(),
                              base::Milliseconds(1));
  client_.SetVideoRtpStream(&video_stream);
  client_.SetShouldDeliverFrames(false);
  video_stream.InsertVideoFrame(client_.CreateVideoFrame());
  // Fast forward by enough time for the refresh_timer_ to fire 2 times.
  task_environment_.FastForwardBy(base::Milliseconds(5));
  ExpectTimerNotRunning(video_stream);
  client_.SetVideoRtpStream(nullptr);
}

TEST_F(RtpStreamTest, VideoStreamTimerRestartsWhenFramesDeliveredAgain) {
  auto video_sender = std::make_unique<MockVideoSender>();
  EXPECT_CALL(*video_sender, InsertRawVideoFrame(_, _)).Times(7);
  VideoRtpStream video_stream(std::move(video_sender), client_.GetWeakPtr(),
                              base::Milliseconds(1));
  client_.SetVideoRtpStream(&video_stream);
  client_.SetShouldDeliverFrames(false);
  video_stream.InsertVideoFrame(client_.CreateVideoFrame());
  // Fast forward by enough time for the refresh_timer_ to fire 2 times.
  task_environment_.FastForwardBy(base::Milliseconds(5));

  ExpectTimerNotRunning(video_stream);
  client_.SetShouldDeliverFrames(true);

  // When the video stream receives a frame, it restarts the timer.
  video_stream.InsertVideoFrame(client_.CreateVideoFrame());
  task_environment_.FastForwardBy(base::Milliseconds(5));

  ExpectTimerRunning(video_stream);
  client_.SetVideoRtpStream(nullptr);
}

// Test the audio streaming pipeline.
TEST_F(RtpStreamTest, AudioStreaming) {
  // Create audio data.
  const base::TimeDelta kDuration = base::Milliseconds(10);
  media::cast::FrameSenderConfig audio_config =
      media::cast::GetDefaultAudioSenderConfig();
  std::unique_ptr<media::AudioBus> audio_bus =
      TestAudioBusFactory(audio_config.channels, audio_config.rtp_timebase,
                          TestAudioBusFactory::kMiddleANoteFreq, 0.5f)
          .NextAudioBus(kDuration);

  auto audio_sender = std::make_unique<MockAudioSender>();
  EXPECT_CALL(*audio_sender, InsertAudio(_, _)).Times(1);
  AudioRtpStream audio_stream(std::move(audio_sender), client_.GetWeakPtr());
  audio_stream.InsertAudio(std::move(audio_bus), testing_clock_.NowTicks());
}

}  // namespace mirroring
