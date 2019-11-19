// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/rtp_stream.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/sender/audio_sender.h"
#include "media/cast/sender/video_sender.h"
#include "media/cast/test/mock_cast_transport.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using ::testing::_;
using media::cast::TestAudioBusFactory;

namespace mirroring {

namespace {

class DummyClient final : public RtpStreamClient {
 public:
  DummyClient() {}
  ~DummyClient() override {}

  // RtpStreamClient implementation.
  void OnError(const std::string& message) override {}
  void RequestRefreshFrame() override {}
  void CreateVideoEncodeAccelerator(
      const media::cast::ReceiveVideoEncodeAcceleratorCallback& callback)
      override {}
  void CreateVideoEncodeMemory(
      size_t size,
      const media::cast::ReceiveVideoEncodeMemoryCallback& callback) override {}

  base::WeakPtr<RtpStreamClient> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<DummyClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DummyClient);
};

}  // namespace

class RtpStreamTest : public ::testing::Test {
 public:
  RtpStreamTest()
      : cast_environment_(new media::cast::CastEnvironment(
            &testing_clock_,
            task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMainThreadTaskRunner())) {
    testing_clock_.Advance(base::TimeTicks::Now() - base::TimeTicks());
  }

  ~RtpStreamTest() override { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::SimpleTestTickClock testing_clock_;
  const scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  DummyClient client_;
  media::cast::MockCastTransport transport_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RtpStreamTest);
};

// Test the video streaming pipeline.
TEST_F(RtpStreamTest, VideoStreaming) {
  // Create one video frame.
  gfx::Size size(64, 32);
  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, size, gfx::Rect(size), size, base::TimeDelta());
  media::cast::PopulateVideoFrame(video_frame.get(), 1);
  video_frame->metadata()->SetTimeTicks(
      media::VideoFrameMetadata::REFERENCE_TIME, testing_clock_.NowTicks());

  auto video_sender = std::make_unique<media::cast::VideoSender>(
      cast_environment_, media::cast::GetDefaultVideoSenderConfig(),
      base::DoNothing(), base::DoNothing(), base::DoNothing(), &transport_,
      base::DoNothing());
  VideoRtpStream video_stream(std::move(video_sender), client_.GetWeakPtr());
  {
    base::RunLoop run_loop;
    // Expect the video frame is sent to video sender for encoding, and the
    // encoded frame is sent to the transport.
    EXPECT_CALL(transport_, InsertFrame(_, _))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    video_stream.InsertVideoFrame(std::move(video_frame));
    run_loop.Run();
  }

  task_environment_.RunUntilIdle();
}

// Test the audio streaming pipeline.
TEST_F(RtpStreamTest, AudioStreaming) {
  // Create audio data.
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(10);
  media::cast::FrameSenderConfig audio_config =
      media::cast::GetDefaultAudioSenderConfig();
  std::unique_ptr<media::AudioBus> audio_bus =
      TestAudioBusFactory(audio_config.channels, audio_config.rtp_timebase,
                          TestAudioBusFactory::kMiddleANoteFreq, 0.5f)
          .NextAudioBus(kDuration);
  auto audio_sender = std::make_unique<media::cast::AudioSender>(
      cast_environment_, audio_config, base::DoNothing(), &transport_);
  AudioRtpStream audio_stream(std::move(audio_sender), client_.GetWeakPtr());
  {
    base::RunLoop run_loop;
    // Expect the audio data is sent to audio sender for encoding, and the
    // encoded frame is sent to the transport.
    EXPECT_CALL(transport_, InsertFrame(_, _))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    audio_stream.InsertAudio(std::move(audio_bus), testing_clock_.NowTicks());
    run_loop.Run();
  }

  task_environment_.RunUntilIdle();
}

}  // namespace mirroring
