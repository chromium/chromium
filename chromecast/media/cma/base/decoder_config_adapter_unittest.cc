// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/decoder_config_adapter.h"

#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/stream_id.h"
#include "media/base/media_util.h"
#include "media/base/video_color_space.h"
#include "media/base/video_decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

// Returns an initialized ::media::VideoDecoderConfig.
::media::VideoDecoderConfig GetChromiumVideoConfig() {
  gfx::Size coded_size(640, 480);
  gfx::Rect visible_rect(640, 480);
  gfx::Size natural_size(640, 480);
  return ::media::VideoDecoderConfig(
      ::media::VideoCodec::kH264, ::media::VIDEO_CODEC_PROFILE_UNKNOWN,
      ::media::VideoDecoderConfig::AlphaMode::kIsOpaque,
      ::media::VideoColorSpace(), ::media::kNoTransformation, coded_size,
      visible_rect, natural_size, ::media::EmptyExtraData(),
      ::media::EncryptionScheme::kUnencrypted);
}

TEST(DecoderConfigAdapterTest, PopulatesVideoCodecLevel) {
  constexpr uint32_t kCodecLevel = 30ul;

  ::media::VideoDecoderConfig chromium_config = GetChromiumVideoConfig();
  chromium_config.set_level(kCodecLevel);

  VideoConfig cast_config = DecoderConfigAdapter::ToCastVideoConfig(
      StreamId::kPrimary, chromium_config);

  EXPECT_EQ(cast_config.codec_profile_level, kCodecLevel);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
