// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/decoder_config_adapter.h"

#include <vector>

#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/stream_id.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
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

TEST(DecoderConfigAdapterTest, ToCastAudioConfigSurround71) {
  ::media::AudioDecoderConfig chromium_config(
      ::media::AudioCodec::kEAC3, ::media::SampleFormat::kSampleFormatS16,
      ::media::ChannelLayoutConfig::FromLayout<
          ::media::CHANNEL_LAYOUT_7_1>(),
      48000, /*extra_data=*/std::vector<uint8_t>(),
      ::media::EncryptionScheme::kUnencrypted);

  AudioConfig cast_config = DecoderConfigAdapter::ToCastAudioConfig(
      StreamId::kPrimary, chromium_config);

  EXPECT_EQ(cast_config.channel_layout, ChannelLayout::DISCRETE);
  EXPECT_EQ(cast_config.channel_number, 8);
}

TEST(DecoderConfigAdapterTest, ChannelLayoutConversions) {
  EXPECT_EQ(DecoderConfigAdapter::ToChannelLayout(
                ::media::ChannelLayout::CHANNEL_LAYOUT_NONE),
            ChannelLayout::UNSUPPORTED);
  EXPECT_EQ(DecoderConfigAdapter::ToChannelLayout(
                ::media::ChannelLayout::CHANNEL_LAYOUT_UNSUPPORTED),
            ChannelLayout::UNSUPPORTED);
  EXPECT_EQ(DecoderConfigAdapter::ToChannelLayout(
                ::media::ChannelLayout::CHANNEL_LAYOUT_MONO),
            ChannelLayout::MONO);
  EXPECT_EQ(DecoderConfigAdapter::ToChannelLayout(
                ::media::ChannelLayout::CHANNEL_LAYOUT_STEREO),
            ChannelLayout::STEREO);
  EXPECT_EQ(DecoderConfigAdapter::ToChannelLayout(
                ::media::ChannelLayout::CHANNEL_LAYOUT_STEREO_DOWNMIX),
            ChannelLayout::STEREO);
  EXPECT_EQ(DecoderConfigAdapter::ToChannelLayout(
                ::media::ChannelLayout::CHANNEL_LAYOUT_5_1),
            ChannelLayout::SURROUND_5_1);
  EXPECT_EQ(DecoderConfigAdapter::ToChannelLayout(
                ::media::ChannelLayout::CHANNEL_LAYOUT_5_1_BACK),
            ChannelLayout::SURROUND_5_1);
  EXPECT_EQ(DecoderConfigAdapter::ToChannelLayout(
                ::media::ChannelLayout::CHANNEL_LAYOUT_BITSTREAM),
            ChannelLayout::BITSTREAM);
  EXPECT_EQ(DecoderConfigAdapter::ToChannelLayout(
                ::media::ChannelLayout::CHANNEL_LAYOUT_DISCRETE),
            ChannelLayout::DISCRETE);
  EXPECT_EQ(DecoderConfigAdapter::ToChannelLayout(
                ::media::ChannelLayout::CHANNEL_LAYOUT_7_1),
            ChannelLayout::DISCRETE);
  EXPECT_EQ(DecoderConfigAdapter::ToChannelLayout(
                ::media::ChannelLayout::CHANNEL_LAYOUT_QUAD),
            ChannelLayout::DISCRETE);

  EXPECT_EQ(DecoderConfigAdapter::ToMediaChannelLayout(
                ChannelLayout::UNSUPPORTED),
            ::media::ChannelLayout::CHANNEL_LAYOUT_UNSUPPORTED);
  EXPECT_EQ(DecoderConfigAdapter::ToMediaChannelLayout(ChannelLayout::MONO),
            ::media::ChannelLayout::CHANNEL_LAYOUT_MONO);
  EXPECT_EQ(DecoderConfigAdapter::ToMediaChannelLayout(ChannelLayout::STEREO),
            ::media::ChannelLayout::CHANNEL_LAYOUT_STEREO);
  EXPECT_EQ(DecoderConfigAdapter::ToMediaChannelLayout(
                ChannelLayout::SURROUND_5_1),
            ::media::ChannelLayout::CHANNEL_LAYOUT_5_1);
  EXPECT_EQ(DecoderConfigAdapter::ToMediaChannelLayout(
                ChannelLayout::BITSTREAM),
            ::media::ChannelLayout::CHANNEL_LAYOUT_BITSTREAM);
  EXPECT_EQ(DecoderConfigAdapter::ToMediaChannelLayout(
                ChannelLayout::DISCRETE),
            ::media::ChannelLayout::CHANNEL_LAYOUT_DISCRETE);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
