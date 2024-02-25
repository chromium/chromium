// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shared/platform_info_serializer.h"

#include <string_view>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromecast/bindings/shared/proto_serializer.h"

namespace chromecast {
namespace {
bool IsOutOfRange(int value, int min, int max) {
  return min > value || max < value;
}
}  // namespace

PlatformInfoSerializer::PlatformInfoSerializer() = default;

PlatformInfoSerializer::PlatformInfoSerializer(PlatformInfoSerializer&& other) =
    default;

PlatformInfoSerializer::~PlatformInfoSerializer() = default;

PlatformInfoSerializer& PlatformInfoSerializer::operator=(
    PlatformInfoSerializer&& other) = default;

// static
std::optional<PlatformInfoSerializer> PlatformInfoSerializer::Deserialize(
    std::string_view base64) {
  std::optional<cast::bindings::MediaCapabilitiesMessage> proto =
      chromecast::bindings::ProtoSerializer<
          cast::bindings::MediaCapabilitiesMessage>::Deserialize(base64);
  if (!proto) {
    return std::nullopt;
  }

  PlatformInfoSerializer parser;
  parser.platform_info_ = std::move(*proto);
  return parser;
}

std::string PlatformInfoSerializer::Serialize() const {
  return chromecast::bindings::ProtoSerializer<
      cast::bindings::MediaCapabilitiesMessage>::Serialize(platform_info_);
}

void PlatformInfoSerializer::SetMaxWidth(int max_width) {
  platform_info_.set_max_width(max_width);
}

void PlatformInfoSerializer::SetMaxHeight(int max_height) {
  platform_info_.set_max_height(max_height);
}

void PlatformInfoSerializer::SetMaxFrameRate(int max_frame_rate) {
  platform_info_.set_max_frame_rate(max_frame_rate);
}

void PlatformInfoSerializer::SetSupportedCryptoBlockFormat(
    const std::string& format) {
  platform_info_.set_supported_cryptoblock_format(format);
}

void PlatformInfoSerializer::SetMaxChannels(int max_channels) {
  platform_info_.set_max_channels(max_channels);
}

void PlatformInfoSerializer::SetPcmSurroundSoundSupported(bool is_supported) {
  platform_info_.set_is_pcm_surround_sound_supported(is_supported);
}

void PlatformInfoSerializer::SetPlatformDolbyVisionEnabled(bool is_enabled) {
  platform_info_.set_is_platform_dolby_vision_enabled(is_enabled);
}

void PlatformInfoSerializer::SetDolbyVisionSupported(bool is_supported) {
  platform_info_.set_is_dolby_vision_supported(is_supported);
}

void PlatformInfoSerializer::SetDolbyVision4kP60Supported(bool is_supported) {
  platform_info_.set_is_dolby_vision4k_p60_supported(is_supported);
}

void PlatformInfoSerializer::SetDolbyVisionSupportedByCurrentHdmiMode(
    bool is_supported) {
  platform_info_.set_is_dolby_vision_supported_by_current_hdmi_mode(
      is_supported);
}

void PlatformInfoSerializer::SetHdmiVideoModeSwitchEnabled(bool is_enabled) {
  platform_info_.set_is_hdmi_video_mode_switch_enabled(is_enabled);
}

void PlatformInfoSerializer::SetPlatformHevcEnabled(bool is_enabled) {
  platform_info_.set_is_platform_hevc_enabled(is_enabled);
}

void PlatformInfoSerializer::SetHdmiModeHdrCheckEnforced(bool is_enforced) {
  platform_info_.set_is_hdmi_mode_hdr_check_enforced(is_enforced);
}

void PlatformInfoSerializer::SetHdrSupportedByCurrentHdmiMode(
    bool is_supported) {
  platform_info_.set_is_hdr_supported_by_current_hdmi_mode(is_supported);
}

void PlatformInfoSerializer::SetSmpteSt2084Supported(bool is_supported) {
  platform_info_.set_is_smpte_st2084_supported(is_supported);
}

void PlatformInfoSerializer::SetHlgSupported(bool is_supported) {
  platform_info_.set_is_hlg_supported(is_supported);
}

void PlatformInfoSerializer::SetHdrFeatureEnabled(bool is_enabled) {
  platform_info_.set_is_hdr_feature_enabled(is_enabled);
}

void PlatformInfoSerializer::SetSupportedLegacyVp9Levels(
    std::vector<int> levels) {
  platform_info_.clear_supported_legacy_vp9_levels();
  for (int level : levels) {
    platform_info_.add_supported_legacy_vp9_levels(level);
  }
}

void PlatformInfoSerializer::SetHdcpVersion(int hdcp_version) {
  platform_info_.set_hdcp_version(hdcp_version);
}

void PlatformInfoSerializer::SetSpatialRenderingSupportMask(int mask) {
  platform_info_.set_spatial_rendering_support_mask(mask);
}

void PlatformInfoSerializer::SetMaxFillRate(int max_fill_rate) {
  platform_info_.set_max_fill_rate(max_fill_rate);
}

void PlatformInfoSerializer::SetSupportedAudioCodecs(
    std::vector<AudioCodecInfo> codec_infos) {
  platform_info_.clear_supported_audio_codecs();
  for (const auto& element : codec_infos) {
    cast::bindings::AudioCodecInfo info;
    DCHECK(cast::bindings::AudioCodecInfo::AudioCodec_IsValid(element.codec));
    DCHECK(cast::bindings::AudioCodecInfo::SampleFormat_IsValid(
        element.sample_format));
    info.set_codec(
        cast::bindings::AudioCodecInfo::AudioCodec_IsValid(element.codec)
            ? static_cast<cast::bindings::AudioCodecInfo::AudioCodec>(
                  element.codec)
            : cast::bindings::AudioCodecInfo::AUDIO_CODEC_UNKNOWN);
    info.set_sample_format(
        cast::bindings::AudioCodecInfo::SampleFormat_IsValid(
            element.sample_format)
            ? static_cast<cast::bindings::AudioCodecInfo::SampleFormat>(
                  element.sample_format)
            : cast::bindings::AudioCodecInfo::SAMPLE_FORMAT_UNKNOWN);
    info.set_max_samples_per_second(element.max_samples_per_second);
    info.set_max_audio_channels(element.max_audio_channels);
    *platform_info_.add_supported_audio_codecs() = std::move(info);
  }
}

void PlatformInfoSerializer::SetSupportedVideoCodecs(
    std::vector<VideoCodecInfo> codec_infos) {
  for (auto& element : codec_infos) {
    cast::bindings::VideoCodecInfo info;
    DCHECK(cast::bindings::VideoCodecInfo::VideoCodec_IsValid(element.codec));
    DCHECK(
        cast::bindings::VideoCodecInfo::VideoProfile_IsValid(element.profile));
    info.set_codec(
        cast::bindings::VideoCodecInfo::VideoCodec_IsValid(element.codec)
            ? static_cast<cast::bindings::VideoCodecInfo::VideoCodec>(
                  element.codec)
            : cast::bindings::VideoCodecInfo::VIDEO_CODEC_UNKNOWN);
    info.set_profile(
        cast::bindings::VideoCodecInfo::VideoProfile_IsValid(element.profile)
            ? static_cast<cast::bindings::VideoCodecInfo::VideoProfile>(
                  element.profile)
            : cast::bindings::VideoCodecInfo::VIDEO_PROFILE_UNKNOWN);
    *platform_info_.add_supported_video_codecs() = std::move(info);
  }
}

std::optional<int> PlatformInfoSerializer::MaxWidth() const {
  return platform_info_.max_width();
}

std::optional<int> PlatformInfoSerializer::MaxHeight() const {
  return platform_info_.max_height();
}

std::optional<int> PlatformInfoSerializer::MaxFrameRate() const {
  return platform_info_.max_frame_rate();
}

std::optional<std::string> PlatformInfoSerializer::SupportedCryptoBlockFormat()
    const {
  return platform_info_.supported_cryptoblock_format();
}

std::optional<int> PlatformInfoSerializer::MaxChannels() const {
  return platform_info_.max_channels();
}

std::optional<bool> PlatformInfoSerializer::PcmSurroundSoundSupported() const {
  return platform_info_.is_pcm_surround_sound_supported();
}

std::optional<bool> PlatformInfoSerializer::IsPlatformDolbyVisionEnabled()
    const {
  return platform_info_.is_platform_dolby_vision_enabled();
}

std::optional<bool> PlatformInfoSerializer::IsDolbyVisionSupported() const {
  return platform_info_.is_dolby_vision_supported();
}

std::optional<bool> PlatformInfoSerializer::IsDolbyVision4kP60Supported()
    const {
  return platform_info_.is_dolby_vision4k_p60_supported();
}

std::optional<bool>
PlatformInfoSerializer::IsDolbyVisionSupportedByCurrentHdmiMode() const {
  return platform_info_.is_dolby_vision_supported_by_current_hdmi_mode();
}

std::optional<bool> PlatformInfoSerializer::IsHdmiVideoModeSwitchEnabled()
    const {
  return platform_info_.is_hdmi_video_mode_switch_enabled();
}

std::optional<bool> PlatformInfoSerializer::IsPlatformHevcEnabled() const {
  return platform_info_.is_platform_hevc_enabled();
}

std::optional<bool> PlatformInfoSerializer::IsHdmiModeHdrCheckEnforced() const {
  return platform_info_.is_hdmi_mode_hdr_check_enforced();
}

std::optional<bool> PlatformInfoSerializer::IsHdrSupportedByCurrentHdmiMode()
    const {
  return platform_info_.is_hdr_supported_by_current_hdmi_mode();
}

std::optional<bool> PlatformInfoSerializer::IsSmpteSt2084Supported() const {
  return platform_info_.is_smpte_st2084_supported();
}

std::optional<bool> PlatformInfoSerializer::IsHlgSupported() const {
  return platform_info_.is_hlg_supported();
}

std::optional<bool> PlatformInfoSerializer::IsHdrFeatureEnabled() const {
  return platform_info_.is_hdr_feature_enabled();
}

std::optional<std::vector<int>>
PlatformInfoSerializer::SupportedLegacyVp9Levels() const {
  std::vector<int> levels;
  levels.reserve(platform_info_.supported_legacy_vp9_levels_size());
  for (const auto& level : platform_info_.supported_legacy_vp9_levels()) {
    levels.push_back(level);
  }

  return levels;
}

std::optional<int> PlatformInfoSerializer::HdcpVersion() const {
  return platform_info_.hdcp_version();
}

std::optional<int> PlatformInfoSerializer::SpatialRenderingSupportMask() const {
  return platform_info_.spatial_rendering_support_mask();
}

std::optional<int> PlatformInfoSerializer::MaxFillRate() const {
  return platform_info_.max_fill_rate();
}

std::optional<std::vector<PlatformInfoSerializer::AudioCodecInfo>>
PlatformInfoSerializer::SupportedAudioCodecs() const {
  std::vector<AudioCodecInfo> infos;
  infos.reserve(platform_info_.supported_audio_codecs_size());
  for (const auto& info : platform_info_.supported_audio_codecs()) {
    if (IsOutOfRange(info.codec(), media::AudioCodec::kAudioCodecMin,
                     media::AudioCodec::kAudioCodecMax)) {
      LOG(WARNING) << "Unrecognized AudioCodec: " << info.codec();
      continue;
    }

    if (IsOutOfRange(info.sample_format(),
                     media::SampleFormat::kSampleFormatMin,
                     media::SampleFormat::kSampleFormatMax)) {
      LOG(WARNING) << "Unrecognized SampleFormat: " << info.sample_format();
      continue;
    }

    AudioCodecInfo parsed;
    parsed.codec = static_cast<media::AudioCodec>(info.codec());
    parsed.sample_format =
        static_cast<media::SampleFormat>(info.sample_format());
    parsed.max_samples_per_second = info.max_samples_per_second();
    parsed.max_audio_channels = info.max_audio_channels();
    infos.push_back(std::move(parsed));
  }

  return infos.empty()
             ? std::nullopt
             : std::make_optional<
                   std::vector<PlatformInfoSerializer::AudioCodecInfo>>(infos);
}

std::optional<std::vector<PlatformInfoSerializer::VideoCodecInfo>>
PlatformInfoSerializer::SupportedVideoCodecs() const {
  std::vector<VideoCodecInfo> infos;
  infos.reserve(platform_info_.supported_video_codecs_size());
  for (const auto& info : platform_info_.supported_video_codecs()) {
    if (IsOutOfRange(info.codec(), media::VideoCodec::kVideoCodecMin,
                     media::VideoCodec::kVideoCodecMax)) {
      LOG(WARNING) << "Unrecognized VideoCodec: " << info.codec();
      continue;
    }

    if (IsOutOfRange(info.profile(), media::VideoProfile::kVideoProfileMin,
                     media::VideoProfile::kVideoProfileMax)) {
      LOG(WARNING) << "Unrecognized VideoProfile: " << info.profile();
      continue;
    }

    VideoCodecInfo parsed;
    parsed.codec = static_cast<media::VideoCodec>(info.codec());
    parsed.profile = static_cast<media::VideoProfile>(info.profile());
    infos.push_back(std::move(parsed));
  }

  return infos.empty()
             ? std::nullopt
             : std::make_optional<
                   std::vector<PlatformInfoSerializer::VideoCodecInfo>>(infos);
}

bool operator==(const PlatformInfoSerializer::AudioCodecInfo& first,
                const PlatformInfoSerializer::AudioCodecInfo& second) {
  return first.codec == second.codec &&
         first.sample_format == second.sample_format &&
         first.max_samples_per_second == second.max_samples_per_second &&
         first.max_audio_channels == second.max_audio_channels;
}

bool operator==(const PlatformInfoSerializer::VideoCodecInfo& first,
                const PlatformInfoSerializer::VideoCodecInfo& second) {
  return first.codec == second.codec && first.profile == second.profile;
}

}  // namespace chromecast
