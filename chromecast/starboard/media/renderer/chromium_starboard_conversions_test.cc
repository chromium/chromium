// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/chromium_starboard_conversions.h"

#include <cstring>
#include <optional>
#include <vector>

#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/test_matchers.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/encryption_scheme.h"
#include "media/base/sample_format.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_transformation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromecast {
namespace media {
namespace {

using ::media::AudioCodec;
using ::media::AudioDecoderConfig;
using ::media::ChannelLayout;
using ::media::EncryptionScheme;
using ::media::SampleFormat;
using ::media::VideoCodec;
using ::media::VideoCodecProfile;
using ::media::VideoColorSpace;
using ::media::VideoDecoderConfig;
using ::media::VideoTransformation;
using ::testing::Optional;

TEST(StarboardConversionsTest, ConvertsValidAudioConfigToStarboardConfig) {
  EXPECT_THAT(
      ToStarboardAudioSampleInfo(AudioDecoderConfig(
          AudioCodec::kAAC, SampleFormat::kSampleFormatS32,
          ChannelLayout::CHANNEL_LAYOUT_5_1, /*samples_per_second=*/48000,
          /*extra_data=*/{}, EncryptionScheme::kCenc)),
      Optional(MatchesAudioSampleInfo({
          .codec = StarboardAudioCodec::kStarboardAudioCodecAac,
          .mime = R"-(audio/mp4; codecs="mp4a.40.5")-",
          .format_tag = 0,
          .number_of_channels = 6,
          .samples_per_second = 48000,
          .average_bytes_per_second = 48000 * 4 * 6,
          .block_alignment = 4,
          .bits_per_sample = 32,
          .audio_specific_config_size = 0,
          .audio_specific_config = nullptr,
      })));
}

TEST(StarboardConversionsTest, ReturnsNulloptForInvalidAudioConfig) {
  // DTS is not supported in starboard.
  EXPECT_EQ(ToStarboardAudioSampleInfo(AudioDecoderConfig(
                AudioCodec::kDTS, SampleFormat::kSampleFormatS32,
                ChannelLayout::CHANNEL_LAYOUT_5_1, /*samples_per_second=*/48000,
                /*extra_data=*/{}, EncryptionScheme::kCenc)),
            std::nullopt);
}

TEST(StarboardConversionsTest, ConvertsValidVideoConfigToStarboardConfig) {
  EXPECT_THAT(
      ToStarboardVideoSampleInfo(VideoDecoderConfig(
          VideoCodec::kHEVC, VideoCodecProfile::HEVCPROFILE_MAIN10,
          VideoDecoderConfig::AlphaMode::kIsOpaque,
          VideoColorSpace(1, 1, 1, gfx::ColorSpace::RangeID::LIMITED),
          VideoTransformation(), gfx::Size(1920, 1080), gfx::Rect(1920, 1080),
          gfx::Size(1920, 1080), /*extra_data=*/{}, EncryptionScheme::kCenc)),
      Optional(MatchesVideoSampleInfo({
          .codec = StarboardVideoCodec::kStarboardVideoCodecH265,
          .mime = R"-(video/mp4; codecs="hev1.2.6.L0.B0")-",
          .max_video_capabilities = "",
          .is_key_frame = false,
          .frame_width = 1920,
          .frame_height = 1080,
          .color_metadata =
              StarboardColorMetadata{
                  .bits_per_channel = 0,               // unknown
                  .chroma_subsampling_horizontal = 0,  // unknown
                  .chroma_subsampling_vertical = 0,    // unknown
                  .cb_subsampling_horizontal = 0,      // unknown
                  .cb_subsampling_vertical = 0,        // unknown
                  .chroma_siting_horizontal = 0,       // unknown
                  .chroma_siting_vertical = 0,         // unknown
                  .mastering_metadata = {},
                  .max_cll = 0,
                  .max_fall = 0,
                  .primaries = 1,
                  .transfer = 1,
                  .matrix = 1,
                  .range = 1,
              },
      })));
}

TEST(StarboardConversionsTest,
     ConvertsValidVideoConfigWithHdrMetadataToStarboardConfig) {
  VideoDecoderConfig chromium_config(
      VideoCodec::kHEVC, VideoCodecProfile::HEVCPROFILE_MAIN10,
      VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(9, 16, 9, gfx::ColorSpace::RangeID::LIMITED),
      VideoTransformation(), gfx::Size(3840, 2160), gfx::Rect(3840, 2160),
      gfx::Size(3840, 2160), /*extra_data=*/{}, EncryptionScheme::kUnencrypted);
  gfx::HDRMetadata hdr_metadata;
  gfx::HdrMetadataSmpteSt2086 smpte;
  smpte.luminance_max = 0.9;
  smpte.primaries.fRX = 0.1;
  smpte.primaries.fRY = 0.2;
  smpte.primaries.fGX = 0.3;
  smpte.primaries.fGY = 0.4;
  smpte.primaries.fBX = 0.5;
  smpte.primaries.fBY = 0.6;
  smpte.primaries.fWX = 0.7;
  smpte.primaries.fWY = 0.8;
  smpte.luminance_max = 1.1;
  smpte.luminance_min = 0.01;

  gfx::HdrMetadataCta861_3 cta;
  cta.max_content_light_level = 100;
  cta.max_frame_average_light_level = 1000;

  hdr_metadata.smpte_st_2086 = smpte;
  hdr_metadata.cta_861_3 = cta;
  chromium_config.set_hdr_metadata(hdr_metadata);

  EXPECT_THAT(ToStarboardVideoSampleInfo(chromium_config),
              Optional(MatchesVideoSampleInfo({
                  .codec = StarboardVideoCodec::kStarboardVideoCodecH265,
                  .mime = R"-(video/mp4; codecs="hev1.2.6.L0.B0")-",
                  .max_video_capabilities = "",
                  .is_key_frame = false,
                  .frame_width = 3840,
                  .frame_height = 2160,
                  .color_metadata =
                      {
                          .bits_per_channel = 0,               // unknown
                          .chroma_subsampling_horizontal = 0,  // unknown
                          .chroma_subsampling_vertical = 0,    // unknown
                          .cb_subsampling_horizontal = 0,      // unknown
                          .cb_subsampling_vertical = 0,        // unknown
                          .chroma_siting_horizontal = 0,       // unknown
                          .chroma_siting_vertical = 0,         // unknown
                          .mastering_metadata =
                              {
                                  .primary_r_chromaticity_x = 0.1,
                                  .primary_r_chromaticity_y = 0.2,
                                  .primary_g_chromaticity_x = 0.3,
                                  .primary_g_chromaticity_y = 0.4,
                                  .primary_b_chromaticity_x = 0.5,
                                  .primary_b_chromaticity_y = 0.6,
                                  .white_point_chromaticity_x = 0.7,
                                  .white_point_chromaticity_y = 0.8,
                                  .luminance_max = 1.1,
                                  .luminance_min = 0.01,
                              },
                          .max_cll = 100,
                          .max_fall = 1000,
                          .primaries = 9,
                          .transfer = 16,
                          .matrix = 9,
                          .range = 1,
                      },
              })));
}

TEST(StarboardConversionsTest, ReturnsNulloptForInvalidVideoConfig) {
  // Dolby Vision is paired with a bad profile.
  EXPECT_EQ(
      ToStarboardVideoSampleInfo(VideoDecoderConfig(
          VideoCodec::kDolbyVision, VideoCodecProfile::VVCPROFILE_MAIN10,
          VideoDecoderConfig::AlphaMode::kIsOpaque,
          VideoColorSpace(1, 1, 1, gfx::ColorSpace::RangeID::LIMITED),
          VideoTransformation(), gfx::Size(1920, 1080), gfx::Rect(1920, 1080),
          gfx::Size(1920, 1080), /*extra_data=*/{}, EncryptionScheme::kCenc)),
      std::nullopt);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
