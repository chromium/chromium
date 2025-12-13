// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/mime_utils.h"

#include <limits>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chromecast/media/base/media_codec_support.h"

namespace chromecast {
namespace media {

namespace {

constexpr auto kAudioCodecToMime =
    base::MakeFixedFlatMap<AudioCodec, std::string_view>({
        {kCodecAAC, R"-(audio/mp4; codecs="mp4a.40.5")-"},
        {kCodecMP3, R"-(audio/mp4; codecs="mp4a.69")-"},
        {kCodecPCM, R"-(audio/wav; codecs="1")-"},
        {kCodecPCM_S16BE, R"-(audio/wav; codecs="1")-"},
        {kCodecVorbis, R"-(audio/webm; codecs="vorbis")-"},
        {kCodecOpus, R"-(audio/webm; codecs="opus")-"},
        {kCodecEAC3, R"-(audio/mp4; codecs="ec-3")-"},
        {kCodecAC3, R"-(audio/mp4; codecs="ac-3")-"},
        {kCodecFLAC, R"-(audio/ogg; codecs="flac")-"},
    });

// Returns a MIME string for HEVC dolby vision.
//
// Returns an empty string if the MIME type is not supported by cast, or if it
// cannot be determined.
std::string GetHEVCDolbyVisionMimeType(VideoProfile profile, int32_t level) {
  if (level >= 10 || level <= 0) {
    // Note: there ARE valid dolby vision levels > 10, but they are not
    // officially supported by cast. See
    // https://developers.google.com/cast/docs/media and
    // https://professionalsupport.dolby.com/s/article/What-is-Dolby-Vision-Profile?language=en_US
    LOG(INFO) << "Unsupported dolby vision level: " << level;
    return "";
  }

  int profile_int = 0;

  // According to
  // https://professional.dolby.com/siteassets/content-creation/dolby-vision-for-content-creators/dolbyvisioninmpegdashspecification_v2_0_public_20190107.pdf,
  // only profiles 5 and 8 are supported for online streaming.
  switch (profile) {
    case kDolbyVisionProfile5:
      profile_int = 5;
      break;
    case kDolbyVisionProfile8:
      profile_int = 8;
      break;
    default:
      LOG(INFO) << "Unsupported dolby vision profile: " << profile;
      return "";
  }

  // 'level' is exactly one digit, due to the above check.
  return base::StringPrintf(R"-(video/mp4; codecs="dvhe.0%d.0%d")-",
                            profile_int, level);
}

// Returns a MIME string for H.264 for the given profile/level. Returns an empty
// string if a MIME type cannot be determined.
std::string GetH264MimeType(VideoProfile profile, int32_t level) {
  // Level should be two hexadecimal digits. We could probably narrow this down
  // further (looks like the valid levels are in [10, 62] in decimal).
  if (level <= 0 || level > 0x000000FF) {
    LOG(ERROR) << "Invalid H.264 level=" << level << ", profile=" << profile;
    return "";
  }

  // The string format values come from
  // https://developers.google.com/cast/docs/media and
  // https://source.chromium.org/chromium/chromium/src/+/main:content/test/data/media/canplaytype_test.js;l=1096;drc=1ec72b6410df5f90eaf4f24f33074f8682c7ffc5
  //
  // We're guessing the constraint_set_flags values (the third and fourth digits
  // after avc1).
  std::string codec_str;
  switch (profile) {
    case kH264Baseline:
      codec_str = base::StringPrintf("avc1.42E0%02X", level);
      break;
    case kH264Main:
      codec_str = base::StringPrintf("avc1.4D40%02X", level);
      break;
    case kH264High:
      codec_str = base::StringPrintf("avc1.6400%02X", level);
      break;
    case kH264High10:
      codec_str = base::StringPrintf("avc1.6E00%02X", level);
      break;
    case kH264Extended:
      codec_str = base::StringPrintf("avc1.5800%02X", level);
      break;
    default:
      LOG(ERROR) << "Unsupported H.264 profile=" << profile
                 << ", level=" << level;
      return "";
  }

  return base::StringPrintf(R"-(video/mp4; codecs="%s")-", codec_str.c_str());
}

// Returns a MIME string for HEVC for the given profile/level. Returns an empty
// string if a MIME type cannot be determined.
std::string GetHEVCMimeType(VideoProfile profile, int32_t level) {
  // Note that the level (format string after L) should be represented in
  // decimal, and should equal the level * 30 (e.g. level 5.1 is 153).
  std::string codec_str;
  switch (profile) {
    case kHEVCMain:
      codec_str = base::StringPrintf("hev1.1.6.L%d.B0", level);
      break;
    case kHEVCMain10:
      codec_str = base::StringPrintf("hev1.2.6.L%d.B0", level);
      break;
    default:
      LOG(ERROR) << "Unsupported HEVC profile=" << profile
                 << ", level=" << level;
      return "";
  }

  return base::StringPrintf(R"-(video/mp4; codecs="%s")-", codec_str.c_str());
}

// Returns a MIME string for VP9 for the given profile/level. Returns an empty
// string if a MIME type cannot be determined.
std::string GetVp9MimeType(VideoProfile profile, int32_t level) {
  // Level must be at most two digits.
  // https://source.chromium.org/chromium/chromium/src/+/main:media/base/video_codec_string_parsers.cc;l=103;drc=798b98d70313e6a55bcf9cc85bc7ca7d42ca6d23
  // has a list of currently-supported levels in chromium code, but for
  // simplicity (and potential future compatibility) we just support two digits
  // here. Since that code runs first, the level here should be one of the
  // levels specified there.
  if (level < 0 || level >= 100) {
    LOG(ERROR) << "Invalid VP9 level=" << level << ", profile=" << profile;
    return "";
  }

  // Note that the bit depth is guessed as 10 here, since that value is not
  // provided to GetMimeType. This appears to be a limitation introduced by
  // chromium code, not cast code:
  // https://source.chromium.org/chromium/chromium/src/+/main:media/base/mime_util_internal.cc;l=984;drc=798b98d70313e6a55bcf9cc85bc7ca7d42ca6d23
  std::string codec_str;
  switch (profile) {
    case kVP9Profile0:
      codec_str = base::StringPrintf("vp09.00.%02d.10", level);
      break;
    case kVP9Profile1:
      codec_str = base::StringPrintf("vp09.01.%02d.10", level);
      break;
    case kVP9Profile2:
      codec_str = base::StringPrintf("vp09.02.%02d.10", level);
      break;
    case kVP9Profile3:
      codec_str = base::StringPrintf("vp09.03.%02d.10", level);
      break;
    default:
      LOG(ERROR) << "Unsupported VP9 profile=" << profile
                 << ", level=" << level;
      return "";
  }

  return base::StringPrintf(R"-(video/webm; codecs="%s")-", codec_str.c_str());
}

// Returns a MIME string for AV1 for the given profile/level. Returns an empty
// string if a MIME type cannot be determined.
//
// Since color info, monochrome, etc. are not available to this function, we use
// the simple version of an AV1 MIME string (no optional info is included). We
// also guess a tier (M) and bit depth (08).
std::string GetAv1MimeType(VideoProfile profile, int32_t level) {
  if (level < 0 || level > 31) {
    LOG(ERROR) << "Invalid AV1 level: " << level;
    return "";
  }

  int profile_int = 0;
  switch (profile) {
    case kAV1ProfileMain:
      profile_int = 0;
      break;
    case kAV1ProfileHigh:
      profile_int = 1;
      break;
    case kAV1ProfilePro:
      profile_int = 2;
      break;
    case kVideoProfileUnknown:
      // This can happen for progressive playback.
      LOG(WARNING) << "Unknown AV1 profile. Guessing main profile";
      profile_int = 0;
      break;
    default:
      LOG(ERROR) << "Unsupported AV1 profile: " << profile;
      return "";
  }

  // Note: here we assume tier M and bit depth 08.
  return base::StringPrintf(R"-(video/mp4; codecs="av01.%d.%02dM.08")-",
                            profile_int, level);
}

}  // namespace

std::string GetMimeType(VideoCodec codec, VideoProfile profile, int32_t level) {
  switch (codec) {
    case kCodecDolbyVisionHEVC:
      return GetHEVCDolbyVisionMimeType(profile, level);
    case kCodecH264:
      return GetH264MimeType(profile, level);
    case kCodecHEVC:
      return GetHEVCMimeType(profile, level);
    case kCodecVP9:
      return GetVp9MimeType(profile, level);
    case kCodecVP8:
      return R"-(video/webm; codecs="vp8")-";
    case kCodecAV1:
      return GetAv1MimeType(profile, level);
    default:
      LOG(ERROR) << "Unsupported video codec=" << codec;
      return "";
  }
}

std::string GetMimeType(::media::VideoCodec codec,
                        ::media::VideoCodecProfile profile,
                        uint32_t level) {
  // Ensure that `level` can be converted to int32_t safely.
  if (int64_t{level} > int64_t{std::numeric_limits<int32_t>::max()}) {
    LOG(ERROR) << "Invalid codec level: " << level;
    return "";
  }
  return GetMimeType(ToCastVideoCodec(codec, profile),
                     ToCastVideoProfile(profile), static_cast<int32_t>(level));
}

std::string GetMimeType(AudioCodec codec) {
  if (auto it = kAudioCodecToMime.find(codec); it != kAudioCodecToMime.end()) {
    return std::string(it->second);
  }
  return "";
}

std::string GetMimeType(::media::AudioCodec codec) {
  return GetMimeType(ToCastAudioCodec(codec));
}

}  // namespace media
}  // namespace chromecast
