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

// Holds codec info and the corresponding mime type.
struct CodecInfoAndMime {
  CodecProfileLevel codec_profile_level;
  std::string mime;
};

// Returns a list of supported codecs and the corresponding MIME strings. See
// https://developers.google.com/cast/docs/media for supported codecs.
//
// TODO(b/275430044): expand this to cover all supported codecs.
std::vector<CodecInfoAndMime> GetCodecMimeValues() {
  std::vector<CodecInfoAndMime> out;
  // Profile 5.
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

  // Profile 8.
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
  return out;
}

// The parameter is a codec/profile/level and the corresponding MIME type.
using MimeUtilsTest = ::testing::TestWithParam<CodecInfoAndMime>;

TEST_P(MimeUtilsTest, ConvertsToMimeType) {
  const CodecProfileLevel codec_profile_level = GetParam().codec_profile_level;
  const std::string expected_mime = GetParam().mime;

  EXPECT_THAT(
      GetMimeType(codec_profile_level.codec, codec_profile_level.profile,
                  codec_profile_level.level),
      StrEq(expected_mime));
}

INSTANTIATE_TEST_SUITE_P(MimeTypes,
                         MimeUtilsTest,
                         ::testing::ValuesIn(GetCodecMimeValues()));

}  // namespace
}  // namespace media
}  // namespace chromecast
