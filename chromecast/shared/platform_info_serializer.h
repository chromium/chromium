// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHARED_PLATFORM_INFO_SERIALIZER_H_
#define CHROMECAST_SHARED_PLATFORM_INFO_SERIALIZER_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromecast {

// This class is responsible for serialization and deserialization of JSON
// messages used for specifying the media capabilities of a Cast receiver.
class PlatformInfoSerializer {
 public:
  PlatformInfoSerializer();
  PlatformInfoSerializer(PlatformInfoSerializer&& other);

  ~PlatformInfoSerializer();

  PlatformInfoSerializer& operator=(PlatformInfoSerializer&& other);
  bool operator==(const PlatformInfoSerializer& other) const;
  bool operator!=(const PlatformInfoSerializer& other) const;

  // Tries to parse the provided json, returning base::nullopt on failure.
  static absl::optional<PlatformInfoSerializer> TryParse(
      base::StringPiece json);

  // Serializes |platform_info_| into json.
  std::string ToJson() const;

  // Setters for known properties.
  void SetMaxWidth(int max_width);
  void SetMaxHeight(int max_height);
  void SetMaxFrameRate(int max_frame_rate);
  void SetSupportedCryptoBlockFormat(std::string format);
  void SetMaxChannels(int max_channels);
  void SetPcmSurroundSoundSupported(bool is_supported);
  void SetPlatformDobleVisionEnabled(bool is_enabled);
  void SetDolbyVisionSupported(bool is_supported);
  void SetDolbyVision4kP60Supported(bool is_supported);
  void SetDolbyVisionSupportedByCurrentHdmiMode(bool is_supported);
  void SetHdmiVideoModeSwitchEnabled(bool is_enabled);
  void SetPlatformHevcEnabled(bool is_enabled);
  void SetHdmiModeHdrCheckEnforced(bool is_enforced);
  void SetHdrSupportedByCurrentHdmiMode(bool is_supported);
  void SetSmpteSt2084Supported(bool is_supported);
  void SetHglSupported(bool is_supported);
  void SetHdrFeatureEnabled(bool is_enabled);
  void SetSupportedLegacyVp9Levels(std::vector<int> levels);
  void SetHdcpVersion(int hdcp_version);
  void SetSpatialRenderingSupportMask(int mask);
  void SetMaxFillRate(int max_fill_rate);

  // Getters for the same properties. Returns absl::nullopt if no such value is
  // set, and the set value in all other cases.
  absl::optional<int> MaxWidth() const;
  absl::optional<int> MaxHeight() const;
  absl::optional<int> MaxFrameRate() const;
  absl::optional<std::string> SupportedCryptoBlockFormat() const;
  absl::optional<int> MaxChannels() const;
  absl::optional<bool> PcmSurroundSoundSupported() const;
  absl::optional<bool> IsPlatformDobleVisionEnabled() const;
  absl::optional<bool> IsDolbyVisionSupported() const;
  absl::optional<bool> IsDolbyVision4kP60Supported() const;
  absl::optional<bool> IsDolbyVisionSupportedByCurrentHdmiMode() const;
  absl::optional<bool> IsHdmiVideoModeSwitchEnabled() const;
  absl::optional<bool> IsPlatformHevcEnabled() const;
  absl::optional<bool> IsHdmiModeHdrCheckEnforced() const;
  absl::optional<bool> IsHdrSupportedByCurrentHdmiMode() const;
  absl::optional<bool> IsSmpteSt2084Supported() const;
  absl::optional<bool> IsHglSupported() const;
  absl::optional<bool> IsHdrFeatureEnabled() const;
  absl::optional<std::vector<int>> SupportedLegacyVp9Levels() const;
  absl::optional<int> HdcpVersion() const;
  absl::optional<int> SpatialRenderingSupportMask() const;
  absl::optional<int> MaxFillRate() const;

 private:
  // All currently produced values.
  base::DictionaryValue platform_info_;
};

}  // namespace chromecast

#endif  // CHROMECAST_SHARED_PLATFORM_INFO_SERIALIZER_H_
