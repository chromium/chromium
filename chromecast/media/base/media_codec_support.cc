// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_codec_support.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"

namespace chromecast {
namespace media {

AudioCodec ToCastAudioCodec(const ::media::AudioCodec codec) {
  switch (codec) {
    case ::media::AudioCodec::kAAC:
      return kCodecAAC;
    case ::media::AudioCodec::kMP3:
      return kCodecMP3;
    case ::media::AudioCodec::kPCM:
      return kCodecPCM;
    case ::media::AudioCodec::kPCM_S16BE:
      return kCodecPCM_S16BE;
    case ::media::AudioCodec::kVorbis:
      return kCodecVorbis;
    case ::media::AudioCodec::kOpus:
      return kCodecOpus;
    case ::media::AudioCodec::kEAC3:
      return kCodecEAC3;
    case ::media::AudioCodec::kAC3:
      return kCodecAC3;
    case ::media::AudioCodec::kDTS:
      return kCodecDTS;
    case ::media::AudioCodec::kDTSXP2:
      return kCodecDTSXP2;
    case ::media::AudioCodec::kDTSE:
      return kCodecDTSE;
    case ::media::AudioCodec::kFLAC:
      return kCodecFLAC;
    case ::media::AudioCodec::kMpegHAudio:
      return kCodecMpegHAudio;
    default:
      LOG(ERROR) << "Unsupported audio codec " << codec;
  }
  return kAudioCodecUnknown;
}

VideoCodec ToCastVideoCodec(const ::media::VideoCodec video_codec,
                            const ::media::VideoCodecProfile codec_profile) {
  switch (video_codec) {
    case ::media::VideoCodec::kH264:
      return kCodecH264;
    case ::media::VideoCodec::kVP8:
      return kCodecVP8;
    case ::media::VideoCodec::kVP9:
      return kCodecVP9;
    case ::media::VideoCodec::kHEVC:
      return kCodecHEVC;
    case ::media::VideoCodec::kDolbyVision:
      if (codec_profile == ::media::DOLBYVISION_PROFILE0 ||
          codec_profile == ::media::DOLBYVISION_PROFILE9) {
        return kCodecDolbyVisionH264;
      } else if (codec_profile == ::media::DOLBYVISION_PROFILE5 ||
                 codec_profile == ::media::DOLBYVISION_PROFILE7 ||
                 codec_profile == ::media::DOLBYVISION_PROFILE8) {
        return kCodecDolbyVisionHEVC;
      }
      LOG(ERROR) << "Unsupported video codec profile " << codec_profile;
      break;
    case ::media::VideoCodec::kAV1:
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
    case ::media::HEVCPROFILE_REXT:
      return kHEVCRext;
    case ::media::HEVCPROFILE_HIGH_THROUGHPUT:
      return kHEVCHighThroughput;
    case ::media::HEVCPROFILE_MULTIVIEW_MAIN:
      return kHEVCMultiviewMain;
    case ::media::HEVCPROFILE_SCALABLE_MAIN:
      return kHEVCScalableMain;
    case ::media::HEVCPROFILE_3D_MAIN:
      return kHEVC3dMain;
    case ::media::HEVCPROFILE_SCREEN_EXTENDED:
      return kHEVCScreenExtended;
    case ::media::HEVCPROFILE_SCALABLE_REXT:
      return kHEVCScalableRext;
    case ::media::HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED:
      return kHEVCHighThroughputScreenExtended;
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
      return kDolbyVisionProfile0;
    case ::media::DOLBYVISION_PROFILE5:
      return kDolbyVisionProfile5;
    case ::media::DOLBYVISION_PROFILE7:
      return kDolbyVisionProfile7;
    case ::media::DOLBYVISION_PROFILE8:
      return kDolbyVisionProfile8;
    case ::media::DOLBYVISION_PROFILE9:
      return kDolbyVisionProfile9;
    case ::media::AV1PROFILE_PROFILE_MAIN:
      return kAV1ProfileMain;
    case ::media::AV1PROFILE_PROFILE_HIGH:
      return kAV1ProfileHigh;
    case ::media::AV1PROFILE_PROFILE_PRO:
      return kAV1ProfilePro;
    case ::media::VVCPROFILE_MAIN10:
      return kVVCProfileMain10;
    case ::media::VVCPROFILE_MAIN12:
      return kVVCProfileMain12;
    case ::media::VVCPROFILE_MAIN12_INTRA:
      return kVVCProfileMain12Intra;
    case ::media::VVCPROIFLE_MULTILAYER_MAIN10:
      return kVVCProfileMultilayerMain10;
    case ::media::VVCPROFILE_MAIN10_444:
      return kVVCProfileMain10444;
    case ::media::VVCPROFILE_MAIN12_444:
      return kVVCProfileMain12444;
    case ::media::VVCPROFILE_MAIN16_444:
      return kVVCProfileMain16444;
    case ::media::VVCPROFILE_MAIN12_444_INTRA:
      return kVVCProfileMain12444Intra;
    case ::media::VVCPROFILE_MAIN16_444_INTRA:
      return kVVCProfileMain16444Intra;
    case ::media::VVCPROFILE_MULTILAYER_MAIN10_444:
      return kVVCProfileMain10444;
    case ::media::VVCPROFILE_MAIN10_STILL_PICTURE:
      return kVVCProfileMain10Still;
    case ::media::VVCPROFILE_MAIN12_STILL_PICTURE:
      return kVVCProfileMain12Still;
    case ::media::VVCPROFILE_MAIN10_444_STILL_PICTURE:
      return kVVCProfileMain10444Still;
    case ::media::VVCPROFILE_MAIN12_444_STILL_PICTURE:
      return kVVCProfileMain12444Still;
    case ::media::VVCPROFILE_MAIN16_444_STILL_PICTURE:
      return kVVCProfileMain16444Still;
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
