// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "chromecast/media/api/test/mock_cma_backend.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/pipeline/av_pipeline_client.h"
#include "chromecast/media/cma/pipeline/load_type.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"
#include "chromecast/media/cma/test/mock_frame_provider.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_transformation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromecast {
namespace media {
namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::Return;

// A frame provider that never delivers any frames and whose Flush completion
// can optionally be deferred indefinitely.
class StallingFrameProvider : public CodedFrameProvider {
 public:
  explicit StallingFrameProvider(bool defer_flush)
      : defer_flush_(defer_flush) {}

  StallingFrameProvider(const StallingFrameProvider&) = delete;
  StallingFrameProvider& operator=(const StallingFrameProvider&) = delete;

  ~StallingFrameProvider() override = default;

  void Read(ReadCB read_cb) override { pending_read_cb_ = std::move(read_cb); }

  void Flush(base::OnceClosure flush_cb) override {
    pending_read_cb_.Reset();
    if (defer_flush_) {
      pending_flush_cb_ = std::move(flush_cb);
    } else {
      std::move(flush_cb).Run();
    }
  }

 private:
  const bool defer_flush_;
  ReadCB pending_read_cb_;
  base::OnceClosure pending_flush_cb_;
};

TEST(MediaPipelineImplTest, DoesNotCrashOnFlushWhenBufferingIsDisabled) {
  base::test::TaskEnvironment task_environment;

  MediaPipelineImpl media_pipeline;
  MockCmaBackend::VideoDecoder video_decoder;
  auto backend = std::make_unique<MockCmaBackend>();
  auto frame_provider = std::make_unique<MockFrameProvider>();

  ON_CALL(video_decoder, SetConfig).WillByDefault(Return(true));
  EXPECT_CALL(*backend, CreateVideoDecoder).WillOnce(Return(&video_decoder));
  EXPECT_CALL(*backend, Initialize)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*backend, Start).Times(AtLeast(1)).WillRepeatedly(Return(true));

  media_pipeline.Initialize(LoadType::kLoadTypeMediaStream, std::move(backend),
                            /*is_buffering_enabled=*/false);
  media_pipeline.InitializeVideo(
      {::media::VideoDecoderConfig(
          ::media::VideoCodec::kH264,
          ::media::VideoCodecProfile::H264PROFILE_MAIN,
          ::media::VideoDecoderConfig::AlphaMode::kIsOpaque,
          ::media::VideoColorSpace(1, 1, 1, gfx::ColorSpace::RangeID::FULL),
          ::media::VideoTransformation(), gfx::Size(1920, 1080),
          gfx::Rect(0, 0, 1920, 1080), gfx::Size(1920, 1080), {},
          ::media::EncryptionScheme::kUnencrypted)},
      VideoPipelineClient(), std::move(frame_provider));
  media_pipeline.StartPlayingFrom(base::Seconds(0));
  media_pipeline.Flush(base::DoNothing());
}

// When both an audio and a video stream are present, the per-stream flushes
// can complete independently. StartPlayingFrom must not restart the backend
// or any AV pipeline until both have completed and the backend has been
// stopped.
TEST(MediaPipelineImplTest, StartPlayingFromIgnoredWhileFlushPending) {
  base::test::TaskEnvironment task_environment;

  NiceMock<MockCmaBackend::AudioDecoder> audio_decoder;
  NiceMock<MockCmaBackend::VideoDecoder> video_decoder;
  ON_CALL(audio_decoder, SetConfig).WillByDefault(Return(true));
  ON_CALL(video_decoder, SetConfig).WillByDefault(Return(true));

  auto backend = std::make_unique<NiceMock<MockCmaBackend>>();
  ON_CALL(*backend, Initialize).WillByDefault(Return(true));
  ON_CALL(*backend, Start).WillByDefault(Return(true));
  EXPECT_CALL(*backend, CreateAudioDecoder).WillOnce(Return(&audio_decoder));
  EXPECT_CALL(*backend, CreateVideoDecoder).WillOnce(Return(&video_decoder));
  EXPECT_CALL(*backend, Start).Times(1);
  EXPECT_CALL(*backend, Stop).Times(0);

  EXPECT_CALL(audio_decoder, PushBuffer(_)).Times(0);

  MediaPipelineImpl media_pipeline;
  media_pipeline.Initialize(LoadType::kLoadTypeMediaStream, std::move(backend),
                            /*is_buffering_enabled=*/false);

  ::media::AudioDecoderConfig audio_config(
      ::media::AudioCodec::kMP3, ::media::kSampleFormatS16,
      ::media::ChannelLayoutConfig::Stereo(), 44100, ::media::EmptyExtraData(),
      ::media::EncryptionScheme::kUnencrypted);
  ASSERT_EQ(::media::PIPELINE_OK,
            media_pipeline.InitializeAudio(
                audio_config, AvPipelineClient(),
                std::make_unique<StallingFrameProvider>(
                    /*defer_flush=*/false)));
  ASSERT_EQ(
      ::media::PIPELINE_OK,
      media_pipeline.InitializeVideo(
          {::media::VideoDecoderConfig(
              ::media::VideoCodec::kH264, ::media::H264PROFILE_MAIN,
              ::media::VideoDecoderConfig::AlphaMode::kIsOpaque,
              ::media::VideoColorSpace(), ::media::kNoTransformation,
              gfx::Size(640, 480), gfx::Rect(0, 0, 640, 480),
              gfx::Size(640, 480), ::media::EmptyExtraData(),
              ::media::EncryptionScheme::kUnencrypted)},
          VideoPipelineClient(),
          std::make_unique<StallingFrameProvider>(/*defer_flush=*/true)));

  media_pipeline.StartPlayingFrom(base::Seconds(0));

  // The audio pipeline's flush completes synchronously while the video
  // pipeline's flush remains outstanding, so the overall flush has not yet
  // stopped the backend.
  bool flush_done = false;
  media_pipeline.Flush(
      base::BindOnce([](bool* done) { *done = true; }, &flush_done));
  EXPECT_FALSE(flush_done);

  // A StartPlayingFrom that arrives in this state must be rejected without
  // restarting the backend or feeding any new audio buffers.
  media_pipeline.StartPlayingFrom(base::Seconds(1));
  task_environment.RunUntilIdle();
}

}  // namespace
}  // namespace media
}  // namespace chromecast
