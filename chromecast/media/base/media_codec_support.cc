// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_codec_support.h"

#include "base/bind.h"
#include "base/strings/string_util.h"

namespace chromecast {
namespace media {

AudioCodec ToCastAudioCodec(const ::media::AudioCodec codec) {
  switch (codec) {
    case ::media::kCodecAAC:
      return kCodecAAC;
    case ::media::kCodecMP3:
      return kCodecMP3;
    case ::media::kCodecPCM:
      return kCodecPCM;
    case ::media::kCodecPCM_S16BE:
      return kCodecPCM_S16BE;
    case ::media::kCodecVorbis:
      return kCodecVorbis;
    case ::media::kCodecOpus:
      return kCodecOpus;
    case ::media::kCodecEAC3:
      return kCodecEAC3;
    case ::media::kCodecAC3:
      return kCodecAC3;
    case ::media::kCodecFLAC:
      return kCodecFLAC;
    case ::media::kCodecMpegHAudio:
      return kCodecMpegHAudio;
    default:
      LOG(ERROR) << "Unsupported audio codec " << codec;
  }
  return kAudioCodecUnknown;
}

VideoCodec ToCastVideoCodec(const ::media::VideoCodec video_codec,
                            const ::media::VideoCodecProfile codec_profile) {
  switch (video_codec) {
    case ::media::kCodecH264:
      return kCodecH264;
    case ::media::kCodecVP8:
      return kCodecVP8;
    case ::media::kCodecVP9:
      return kCodecVP9;
    case ::media::kCodecHEVC:
      return kCodecHEVC;
    case ::media::kCodecDolbyVision:
      if (codec_profile == ::media::DOLBYVISION_PROFILE0 ||
          codec_profile == ::media::DOLBYVISION_PROFILE9) {
        return kCodecDolbyVisionH264;
      } else if (codec_profile == ::media::DOLBYVISION_PROFILE4 ||
                 codec_profile == ::media::DOLBYVISION_PROFILE5 ||
                 codec_profile == ::media::DOLBYVISION_PROFILE7 ||
                 codec_profile == ::media::DOLBYVISION_PROFILE8) {
        return kCodecDolbyVisionHEVC;
      }
      LOG(ERROR) << "Unsupported video codec profile " << codec_profile;
      break;
    case ::media::kCodecAV1:
      return kCodecAV1;
    default:
      LOG(ERROR) << "Unsupported video codec " << video_codec;
  }
  return kVideoCodecUnknown;
}

VideoProfile ToCastVideoProfile(
    const ::media::VideoCodecProfile codec_profile) {
  switch (codec_profile) {
    case ::media::H264PROFILE_BASELINE:
      return kH264Baseline;
    case ::media::H264PROFILE_MAIN:
      return kH264Main;
    case ::media::H264PROFILE_EXTENDED:
      return kH264Extended;
    case ::media::H264PROFILE_HIGH:
      return kH264High;
    case ::media::H264PROFILE_HIGH10PROFILE:
      return kH264High10;
    case ::media::H264PROFILE_HIGH422PROFILE:
      return kH264High422;
    case ::media::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return kH264High444Predictive;
    case ::media::H264PROFILE_SCALABLEBASELINE:
      return kH264ScalableBaseline;
    case ::media::H264PROFILE_SCALABLEHIGH:
      return kH264ScalableHigh;
    case ::media::H264PROFILE_STEREOHIGH:
      return kH264StereoHigh;
    case ::media::H264PROFILE_MULTIVIEWHIGH:
      return kH264MultiviewHigh;
    case ::media::HEVCPROFILE_MAIN:
      return kHEVCMain;
    case ::media::HEVCPROFILE_MAIN10:
      return kHEVCMain10;
    case ::media::HEVCPROFILE_MAIN_STILL_PICTURE:
      return kHEVCMainStillPicture;
    case ::media::VP8PROFILE_ANY:
      return kVP8ProfileAny;
    case ::media::VP9PROFILE_PROFILE0:
      return kVP9Profile0;
    case ::media::VP9PROFILE_PROFILE1:
      return kVP9Profile1;
    case ::media::VP9PROFILE_PROFILE2:
      return kVP9Profile2;
    case ::media::VP9PROFILE_PROFILE3:
      return kVP9Profile3;
    case ::media::DOLBYVISION_PROFILE0:
      return kDolbyVisionCompatible_EL_MD;
    case ::media::DOLBYVISION_PROFILE4:
      return kDolbyVisionCompatible_EL_MD;
    case ::media::DOLBYVISION_PROFILE5:
      return kDolbyVisionNonCompatible_BL_MD;
    case ::media::DOLBYVISION_PROFILE7:
      return kDolbyVisionNonCompatible_BL_EL_MD;
    case ::media::AV1PROFILE_PROFILE_MAIN:
      return kAV1ProfileMain;
    case ::media::AV1PROFILE_PROFILE_HIGH:
      return kAV1ProfileHigh;
    case ::media::AV1PROFILE_PROFILE_PRO:
      return kAV1ProfilePro;
    default:
      LOG(INFO) << "Unsupported video codec profile " << codec_profile;
  }
  return kVideoProfileUnknown;
}

CodecProfileLevel ToCastCodecProfileLevel(
    const ::media::CodecProfileLevel& codec_profile_level) {
  CodecProfileLevel result;
  result.codec =
      ToCastVideoCodec(codec_profile_level.codec, codec_profile_level.profile);
  result.profile = ToCastVideoProfile(codec_profile_level.profile);
  result.level = codec_profile_level.level;
  return result;
}

}  // namespace media
}  // namespace chromecast
