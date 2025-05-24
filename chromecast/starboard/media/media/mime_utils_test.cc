// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/mime_utils.h"

#include <string>
#include <vector>

#include "chromecast/public/media/decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::testing::StrEq;

// Holds video codec info and the corresponding MIME type.
struct VideoCodecInfoAndMime {
  CodecProfileLevel codec_profile_level;
  std::string mime;
};

// Holds audio codec info and the corresponding MIME type.
struct AudioCodecInfoAndMime {
  AudioCodec codec;
  std::string mime;
};

// Returns a list of supported video codecs and the corresponding MIME strings.
// See https://developers.google.com/cast/docs/media for supported codecs,
// though that list is likely not meant to be exhaustive.
std::vector<VideoCodecInfoAndMime> GetVideoCodecMimeValues() {
  std::vector<VideoCodecInfoAndMime> out;

  // h.264 baseline profile.
  out.push_back({.codec_profile_level = {.codec = kCodecH264,
                                         .profile = kH264Baseline,
                                         .level = 30},
                 .mime = R"-(video/mp4; codecs="avc1.42E01E")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecH264,
                                         .profile = kH264Baseline,
                                         .level = 31},
                 .mime = R"-(video/mp4; codecs="avc1.42E01F")-"});

  // h.264 main profile.
  out.push_back({.codec_profile_level = {.codec = kCodecH264,
                                         .profile = kH264Main,
                                         .level = 31},
                 .mime = R"-(video/mp4; codecs="avc1.4D401F")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecH264,
                                         .profile = kH264Main,
                                         .level = 40},
                 .mime = R"-(video/mp4; codecs="avc1.4D4028")-"});

  // h.264 high profile.
  out.push_back({.codec_profile_level = {.codec = kCodecH264,
                                         .profile = kH264High,
                                         .level = 40},
                 .mime = R"-(video/mp4; codecs="avc1.640028")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecH264,
                                         .profile = kH264High,
                                         .level = 41},
                 .mime = R"-(video/mp4; codecs="avc1.640029")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecH264,
                                         .profile = kH264High,
                                         .level = 42},
                 .mime = R"-(video/mp4; codecs="avc1.64002A")-"});

  // HEVC main profile.
  out.push_back({.codec_profile_level = {.codec = kCodecHEVC,
                                         .profile = kHEVCMain,
                                         .level = 150},
                 .mime = R"-(video/mp4; codecs="hev1.1.6.L150.B0")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecHEVC,
                                         .profile = kHEVCMain,
                                         .level = 153},
                 .mime = R"-(video/mp4; codecs="hev1.1.6.L153.B0")-"});

  // HEVC main10 profile.
  out.push_back({.codec_profile_level = {.codec = kCodecHEVC,
                                         .profile = kHEVCMain10,
                                         .level = 150},
                 .mime = R"-(video/mp4; codecs="hev1.2.6.L150.B0")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecHEVC,
                                         .profile = kHEVCMain10,
                                         .level = 153},
                 .mime = R"-(video/mp4; codecs="hev1.2.6.L153.B0")-"});

  // VP8.
  out.push_back({.codec_profile_level = {.codec = kCodecVP8,
                                         .profile = kVP8ProfileAny,
                                         .level = 0},
                 .mime = R"-(video/webm; codecs="vp8")-"});

  // VP9. Note that these assume bit depth 10, since the chromium code does not
  // include bit depth when checking decoder support for the codec.
  out.push_back({.codec_profile_level = {.codec = kCodecVP9,
                                         .profile = kVP9Profile0,
                                         .level = 10},
                 .mime = R"-(video/webm; codecs="vp09.00.10.10")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecVP9,
                                         .profile = kVP9Profile1,
                                         .level = 10},
                 .mime = R"-(video/webm; codecs="vp09.01.10.10")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecVP9,
                                         .profile = kVP9Profile2,
                                         .level = 10},
                 .mime = R"-(video/webm; codecs="vp09.02.10.10")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecVP9,
                                         .profile = kVP9Profile3,
                                         .level = 10},
                 .mime = R"-(video/webm; codecs="vp09.03.10.10")-"});

  // Check VP9 with a different level. There are many supported levels, so we do
  // not include all possible values.
  out.push_back({.codec_profile_level = {.codec = kCodecVP9,
                                         .profile = kVP9Profile0,
                                         .level = 62},
                 .mime = R"-(video/webm; codecs="vp09.00.62.10")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecVP9,
                                         .profile = kVP9Profile1,
                                         .level = 62},
                 .mime = R"-(video/webm; codecs="vp09.01.62.10")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecVP9,
                                         .profile = kVP9Profile2,
                                         .level = 62},
                 .mime = R"-(video/webm; codecs="vp09.02.62.10")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecVP9,
                                         .profile = kVP9Profile3,
                                         .level = 62},
                 .mime = R"-(video/webm; codecs="vp09.03.62.10")-"});

  // Dolby Vision profile 5.
  out.push_back({.codec_profile_level = {.codec = kCodecDolbyVisionHEVC,
                                         .profile = kDolbyVisionProfile5,
                                         .level = 6},
                 .mime = R"-(video/mp4; codecs="dvhe.05.06")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecDolbyVisionHEVC,
                                         .profile = kDolbyVisionProfile5,
                                         .level = 7},
                 .mime = R"-(video/mp4; codecs="dvhe.05.07")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecDolbyVisionHEVC,
                                         .profile = kDolbyVisionProfile5,
                                         .level = 9},
                 .mime = R"-(video/mp4; codecs="dvhe.05.09")-"});

  // Dolby Vision profile 8.
  out.push_back({.codec_profile_level = {.codec = kCodecDolbyVisionHEVC,
                                         .profile = kDolbyVisionProfile8,
                                         .level = 6},
                 .mime = R"-(video/mp4; codecs="dvhe.08.06")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecDolbyVisionHEVC,
                                         .profile = kDolbyVisionProfile8,
                                         .level = 7},
                 .mime = R"-(video/mp4; codecs="dvhe.08.07")-"});
  out.push_back({.codec_profile_level = {.codec = kCodecDolbyVisionHEVC,
                                         .profile = kDolbyVisionProfile8,
                                         .level = 9},
                 .mime = R"-(video/mp4; codecs="dvhe.08.09")-"});

  // Unsupported cases should return an empty string for the MIME type.
  out.push_back({.codec_profile_level = {.codec = kCodecDolbyVisionHEVC,
                                         .profile = kDolbyVisionProfile5,
                                         .level = 11},
                 .mime = ""});
  out.push_back({.codec_profile_level = {.codec = kCodecDolbyVisionHEVC,
                                         .profile = kDolbyVisionProfile8,
                                         .level = 11},
                 .mime = ""});

  // Invalid codec/profile/level combinations should return an empty string.

  // Mixed h.264 profile with HEVC codec.
  out.push_back({.codec_profile_level = {.codec = kCodecHEVC,
                                         .profile = kH264Main,
                                         .level = 150},
                 .mime = ""});

  // Mixed VP9 with HEVC profile.
  out.push_back({.codec_profile_level = {.codec = kCodecVP9,
                                         .profile = kH264Main,
                                         .level = 62},
                 .mime = ""});

  return out;
}

// Returns a list of supported audio codecs and the corresponding MIME strings.
// See https://developers.google.com/cast/docs/media for supported codecs,
// though that list is likely not meant to be exhaustive.
std::vector<AudioCodecInfoAndMime> GetAudioCodecMimeValues() {
  std::vector<AudioCodecInfoAndMime> out;

  // AAC.
  out.push_back(
      {.codec = kCodecAAC, .mime = R"-(audio/mp4; codecs="mp4a.40.5")-"});

  // MP3
  out.push_back(
      {.codec = kCodecMP3, .mime = R"-(audio/mp4; codecs="mp4a.69")-"});

  // PCM.
  out.push_back({.codec = kCodecPCM, .mime = R"-(audio/wav; codecs="1")-"});
  out.push_back(
      {.codec = kCodecPCM_S16BE, .mime = R"-(audio/wav; codecs="1")-"});

  // Vorbis.
  out.push_back(
      {.codec = kCodecVorbis, .mime = R"-(audio/webm; codecs="vorbis")-"});

  // Opus.
  out.push_back(
      {.codec = kCodecOpus, .mime = R"-(audio/webm; codecs="opus")-"});

  // E-AC-3.
  out.push_back({.codec = kCodecEAC3, .mime = R"-(audio/mp4; codecs="ec-3")-"});

  // AC-3.
  out.push_back({.codec = kCodecAC3, .mime = R"-(audio/mp4; codecs="ac-3")-"});

  // FLAC.
  out.push_back({.codec = kCodecFLAC, .mime = R"-(audio/ogg; codecs="flac")-"});

  // Unsupported codecs should return an empty string.
  out.push_back({.codec = kCodecDTS, .mime = ""});
  out.push_back({.codec = kCodecMpegHAudio, .mime = ""});
  out.push_back({.codec = kCodecDTSXP2, .mime = ""});
  out.push_back({.codec = kCodecDTSE, .mime = ""});

  return out;
}

// The parameter is a video codec/profile/level and the corresponding MIME type.
using MimeUtilsVideoCodecTest = ::testing::TestWithParam<VideoCodecInfoAndMime>;

TEST_P(MimeUtilsVideoCodecTest, ConvertsToMimeType) {
  const CodecProfileLevel codec_profile_level = GetParam().codec_profile_level;
  const std::string expected_mime = GetParam().mime;

  EXPECT_THAT(
      GetMimeType(codec_profile_level.codec, codec_profile_level.profile,
                  codec_profile_level.level),
      StrEq(expected_mime));
}

INSTANTIATE_TEST_SUITE_P(VideoMimeTypes,
                         MimeUtilsVideoCodecTest,
                         ::testing::ValuesIn(GetVideoCodecMimeValues()));

// The parameter is an audio codec and the corresponding MIME type.
using MimeUtilsAudioCodecTest = ::testing::TestWithParam<AudioCodecInfoAndMime>;

TEST_P(MimeUtilsAudioCodecTest, ConvertsToMimeType) {
  EXPECT_THAT(GetMimeType(GetParam().codec), StrEq(GetParam().mime));
}

INSTANTIATE_TEST_SUITE_P(AudioMimeTypes,
                         MimeUtilsAudioCodecTest,
                         ::testing::ValuesIn(GetAudioCodecMimeValues()));

}  // namespace
}  // namespace media
}  // namespace chromecast
