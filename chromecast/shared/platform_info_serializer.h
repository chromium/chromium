// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHARED_PLATFORM_INFO_SERIALIZER_H_
#define CHROMECAST_SHARED_PLATFORM_INFO_SERIALIZER_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "chromecast/public/media/decoder_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cast_core/public/src/proto/bindings/media_capabilities.pb.h"

namespace chromecast {

// This class is responsible for serialization and deserialization of JSON
// messages used for specifying the media capabilities of a Cast receiver.
class PlatformInfoSerializer {
 public:
  // Information about a supported audio codec.
  struct AudioCodecInfo {
    media::AudioCodec codec = media::AudioCodec::kAudioCodecUnknown;
    media::SampleFormat sample_format =
        media::SampleFormat::kUnknownSampleFormat;
    int max_samples_per_second = 0;
    int max_audio_channels = 0;
  };

  // Information about a supported video codec.
  struct VideoCodecInfo {
    media::VideoCodec codec = media::VideoCodec::kVideoCodecUnknown;
    media::VideoProfile profile = media::VideoProfile::kVideoProfileUnknown;
  };

  PlatformInfoSerializer();
  PlatformInfoSerializer(PlatformInfoSerializer&& other);
  ~PlatformInfoSerializer();
  PlatformInfoSerializer& operator=(PlatformInfoSerializer&& other);

  cast::bindings::MediaCapabilitiesMessage* platform_info();

  std::string Serialize() const;
  static absl::optional<PlatformInfoSerializer> Deserialize(
      base::StringPiece base64);

  // Setters for known valid properties.
  void SetMaxWidth(int max_width);
  void SetMaxHeight(int max_height);
  void SetMaxFrameRate(int max_frame_rate);
  void SetSupportedCryptoBlockFormat(const std::string& format);
  void SetMaxChannels(int max_channels);
  void SetPcmSurroundSoundSupported(bool is_supported);
  void SetPlatformDolbyVisionEnabled(bool is_enabled);
  void SetDolbyVisionSupported(bool is_supported);
  void SetDolbyVision4kP60Supported(bool is_supported);
  void SetDolbyVisionSupportedByCurrentHdmiMode(bool is_supported);
  void SetHdmiVideoModeSwitchEnabled(bool is_enabled);
  void SetPlatformHevcEnabled(bool is_enabled);
  void SetHdmiModeHdrCheckEnforced(bool is_enforced);
  void SetHdrSupportedByCurrentHdmiMode(bool is_supported);
  void SetSmpteSt2084Supported(bool is_supported);
  void SetHlgSupported(bool is_supported);
  void SetHdrFeatureEnabled(bool is_enabled);
  void SetHdcpVersion(int hdcp_version);
  void SetSpatialRenderingSupportMask(int mask);
  void SetMaxFillRate(int max_fill_rate);
  void SetSupportedAudioCodecs(std::vector<AudioCodecInfo> codec_infos);
  void SetSupportedVideoCodecs(std::vector<VideoCodecInfo> codec_infos);

  // Getters for the same properties. Returns absl::nullopt if no such value is
  // set, and the set value in all other cases.
  absl::optional<int> MaxWidth() const;
  absl::optional<int> MaxHeight() const;
  absl::optional<int> MaxFrameRate() const;
  absl::optional<std::string> SupportedCryptoBlockFormat() const;
  absl::optional<int> MaxChannels() const;
  absl::optional<bool> PcmSurroundSoundSupported() const;
  absl::optional<bool> IsPlatformDolbyVisionEnabled() const;
  absl::optional<bool> IsDolbyVisionSupported() const;
  absl::optional<bool> IsDolbyVision4kP60Supported() const;
  absl::optional<bool> IsDolbyVisionSupportedByCurrentHdmiMode() const;
  absl::optional<bool> IsHdmiVideoModeSwitchEnabled() const;
  absl::optional<bool> IsPlatformHevcEnabled() const;
  absl::optional<bool> IsHdmiModeHdrCheckEnforced() const;
  absl::optional<bool> IsHdrSupportedByCurrentHdmiMode() const;
  absl::optional<bool> IsSmpteSt2084Supported() const;
  absl::optional<bool> IsHlgSupported() const;
  absl::optional<bool> IsHdrFeatureEnabled() const;
  absl::optional<int> HdcpVersion() const;
  absl::optional<int> SpatialRenderingSupportMask() const;
  absl::optional<int> MaxFillRate() const;
  absl::optional<std::vector<AudioCodecInfo>> SupportedAudioCodecs() const;
  absl::optional<std::vector<VideoCodecInfo>> SupportedVideoCodecs() const;

  // Deprecated fields.
  void SetSupportedLegacyVp9Levels(std::vector<int> levels);
  absl::optional<std::vector<int>> SupportedLegacyVp9Levels() const;

 private:
  // All currently produced values.
  cast::bindings::MediaCapabilitiesMessage platform_info_;
};

bool operator==(const PlatformInfoSerializer::AudioCodecInfo& first,
                const PlatformInfoSerializer::AudioCodecInfo& second);

bool operator==(const PlatformInfoSerializer::VideoCodecInfo& first,
                const PlatformInfoSerializer::VideoCodecInfo& second);

}  // namespace chromecast

#endif  // CHROMECAST_SHARED_PLATFORM_INFO_SERIALIZER_H_
