// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "chromecast/media/api/test/mock_cma_backend.h"
#include "chromecast/media/cma/pipeline/load_type.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"
#include "chromecast/media/cma/test/mock_frame_provider.h"
#include "media/base/encryption_scheme.h"
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

using ::testing::AtLeast;
using ::testing::Return;

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

}  // namespace
}  // namespace media
}  // namespace chromecast
