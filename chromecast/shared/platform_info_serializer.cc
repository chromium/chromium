// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shared/platform_info_serializer.h"

#include "base/callback.h"
#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"

namespace chromecast {

namespace {

// Keep synced with media_capabilities.js
constexpr char kMaxWidthProperty[] = "maxWidth";
constexpr char kMaxHeightProperty[] = "maxHeight";
constexpr char kMaxFrameRateProperty[] = "maxFrameRate";
constexpr char kSupportedCryptoBlockFormatProperty[] =
    "supportedCryptoblockFormat";
constexpr char kMaxChannelsProperty[] = "maxChannels";
constexpr char kPcmSurroundSoundSupportedProperty[] =
    "pcmSurroundSoundSupported";
constexpr char kIsPlatformDolbyVisionEnabledProperty[] =
    "isPlatformDolbyVisionEnabled";
constexpr char kIsDolbyVisionSupportedProperty[] = "isDolbyVisionSupported";
constexpr char kIsDolbyVision4kP60SupportedProperty[] =
    "isDolbyVision4kP60Supported";
constexpr char kIsDolbyVisionSupportedByCurrentHdmiModeProperty[] =
    "isDolbyVisionSupportedByCurrentHdmiMode";
constexpr char kIsHdmiVideoModeSwitchEnabledProperty[] =
    "isHdmiVideoModeSwitchEnabled";
constexpr char kIsPlatformHevcEnabledProperty[] = "isPlatformHevcEnabled";
constexpr char kIsHdmiModeHdrCheckEnforcedProperty[] =
    "isHdmiModeHdrCheckEnforced";
constexpr char kIsHdrSupportedByCurrentHdmiModeProperty[] =
    "isHdrSupportedByCurrentHdmiMode";
constexpr char kIsSmpteSt2084SupportedProperty[] = "isSmpteSt2084Supported";
constexpr char kIsHlgSupportedProperty[] = "isHlgSupported";
constexpr char kIsHdrFeatureEnabledProperty[] = "isHdrFeatureEnabled";
constexpr char kSupportedLegacyVp9LevelsProperty[] = "supportedLegacyVp9Levels";
constexpr char kHdcpVersionProperty[] = "hdcpVersion";
constexpr char kSpatialRenderingSupportMaskProperty[] =
    "spatialRenderingSupportMask";
constexpr char kMaxFillrateProperty[] = "maxFillrate";

// Audio Codec information
constexpr char kSupportedAudioCodecsProperty[] = "supportedAudioCodecs";
constexpr char kAudioCodecInfoCodecKey[] = "codec";
constexpr char kAudioCodecInfoSampleFormat[] = "sampleFormat";
constexpr char kAudioCodecInfoSamplesPerSecond[] = "samplesPerSecond";
constexpr char kAudioCodecInfoMaxAudioChannelsKey[] = "maxAudioChannels";

// Video Codec information
constexpr char kSupportedVideoCodecsProperty[] = "supportedVideoCodecs";
constexpr char kVideoCodecInfoCodecKey[] = "codec";
constexpr char kVideoCodecInfoProfileKey[] = "profile";

// Attempts to parse |value| as the given type, returning absl::nullopt on
// failure.
template <typename T>
struct Parser {
  static absl::optional<T> Parse(const base::Value& value);
};

template <typename T>
struct Parser<std::vector<T>> {
  static absl::optional<std::vector<T>> Parse(const base::Value& value);
};

// Tries to populate values of the given type |T| from |value| into
// |destination|, returning whether the operation succeeded.
template <typename T>
bool TryPopulateValue(const base::Value* value, T* destination) {
  DCHECK(destination);

  if (!value) {
    return false;
  }

  auto parsed = Parser<T>::Parse(*value);
  if (!parsed.has_value()) {
    return false;
  }

  *destination = std::move(parsed.value());
  return true;
}

// Parses an enum value. To be used as a helper when implementing the above.
template <typename T>
absl::optional<T> ParseEnum(const base::Value& value) {
  absl::optional<int> parsed_int = Parser<int>::Parse(value);
  if (!parsed_int.has_value()) {
    return absl::nullopt;
  }
  return static_cast<T>(parsed_int.value());
}

template <>
inline absl::optional<media::AudioCodec> Parser<media::AudioCodec>::Parse(
    const base::Value& value) {
  return ParseEnum<media::AudioCodec>(value);
}

template <>
inline absl::optional<media::SampleFormat> Parser<media::SampleFormat>::Parse(
    const base::Value& value) {
  return ParseEnum<media::SampleFormat>(value);
}

template <>
inline absl::optional<media::VideoCodec> Parser<media::VideoCodec>::Parse(
    const base::Value& value) {
  return ParseEnum<media::VideoCodec>(value);
}

template <>
inline absl::optional<media::VideoProfile> Parser<media::VideoProfile>::Parse(
    const base::Value& value) {
  return ParseEnum<media::VideoProfile>(value);
}

template <>
absl::optional<bool> Parser<bool>::Parse(const base::Value& value) {
  if (!value.is_bool()) {
    return absl::nullopt;
  }
  return value.GetBool();
}

template <>
absl::optional<int> Parser<int>::Parse(const base::Value& value) {
  if (!value.is_int()) {
    return absl::nullopt;
  }
  return value.GetInt();
}

template <>
absl::optional<std::string> Parser<std::string>::Parse(
    const base::Value& value) {
  if (!value.is_string()) {
    return absl::nullopt;
  }
  return value.GetString();
}

template <>
absl::optional<PlatformInfoSerializer::AudioCodecInfo>
Parser<PlatformInfoSerializer::AudioCodecInfo>::Parse(
    const base::Value& value) {
  if (!value.is_dict()) {
    return absl::nullopt;
  }

  const base::Value* codec_value = value.FindKey(kAudioCodecInfoCodecKey);
  const base::Value* sample_format_value =
      value.FindKey(kAudioCodecInfoSampleFormat);
  const base::Value* samples_per_second_value =
      value.FindKey(kAudioCodecInfoSamplesPerSecond);
  const base::Value* max_audio_channels_value =
      value.FindKey(kAudioCodecInfoMaxAudioChannelsKey);

  PlatformInfoSerializer::AudioCodecInfo audio_codec_info;
  if (!TryPopulateValue(codec_value, &audio_codec_info.codec)) {
    return absl::nullopt;
  }

  TryPopulateValue(sample_format_value, &audio_codec_info.sample_format);
  TryPopulateValue(samples_per_second_value,
                   &audio_codec_info.max_samples_per_second);
  TryPopulateValue(max_audio_channels_value,
                   &audio_codec_info.max_audio_channels);

  return audio_codec_info;
}

template <>
absl::optional<PlatformInfoSerializer::VideoCodecInfo>
Parser<PlatformInfoSerializer::VideoCodecInfo>::Parse(
    const base::Value& value) {
  if (!value.is_dict()) {
    return absl::nullopt;
  }

  const base::Value* codec_value = value.FindKey(kVideoCodecInfoCodecKey);
  const base::Value* profiles_value = value.FindKey(kVideoCodecInfoProfileKey);

  PlatformInfoSerializer::VideoCodecInfo video_codec_info;
  if (!TryPopulateValue(codec_value, &video_codec_info.codec)) {
    return absl::nullopt;
  }

  TryPopulateValue(profiles_value, &video_codec_info.profile);

  return video_codec_info;
}

template <typename T>
absl::optional<std::vector<T>> Parser<std::vector<T>>::Parse(
    const base::Value& value) {
  if (!value.is_list()) {
    return absl::nullopt;
  }
  base::Value::ConstListView list = value.GetList();
  std::vector<T> results;
  results.reserve(list.size());
  for (const auto& element : list) {
    auto parsed_element = Parser<T>::Parse(element);
    if (!parsed_element.has_value()) {
      return absl::nullopt;
    }
    results.push_back(parsed_element.value());
  }
  return results;
}

template <typename T>
absl::optional<T> ParseWithKey(const base::DictionaryValue& dictionary,
                               base::StringPiece key) {
  const base::Value* value = dictionary.FindKey(key);
  if (!value) {
    return absl::nullopt;
  }

  return Parser<T>::Parse(*value);
}

void SetWithKey(base::DictionaryValue* dictionary,
                std::vector<PlatformInfoSerializer::AudioCodecInfo> elements) {
  DCHECK(dictionary);
  base::ListValue list;
  for (const auto& element : elements) {
    base::DictionaryValue audio_codec_value;
    audio_codec_value.SetIntKey(kAudioCodecInfoCodecKey, element.codec);
    audio_codec_value.SetIntKey(kAudioCodecInfoSampleFormat,
                                element.sample_format);
    audio_codec_value.SetIntKey(kAudioCodecInfoSamplesPerSecond,
                                element.max_samples_per_second);
    audio_codec_value.SetIntKey(kAudioCodecInfoMaxAudioChannelsKey,
                                element.max_audio_channels);
    list.Append(std::move(audio_codec_value));
  }
  dictionary->SetKey(kSupportedAudioCodecsProperty, std::move(list));
}

void SetWithKey(base::DictionaryValue* dictionary,
                std::vector<PlatformInfoSerializer::VideoCodecInfo> elements) {
  DCHECK(dictionary);
  base::ListValue list;
  for (auto& element : elements) {
    base::DictionaryValue video_codec_value;
    video_codec_value.SetIntKey(kVideoCodecInfoCodecKey, element.codec);
    video_codec_value.SetIntKey(kVideoCodecInfoProfileKey, element.profile);
    list.Append(std::move(video_codec_value));
  }
  dictionary->SetKey(kSupportedVideoCodecsProperty, std::move(list));
}

}  // namespace

PlatformInfoSerializer::PlatformInfoSerializer() = default;

PlatformInfoSerializer::PlatformInfoSerializer(PlatformInfoSerializer&& other) =
    default;

PlatformInfoSerializer::~PlatformInfoSerializer() = default;

PlatformInfoSerializer& PlatformInfoSerializer::operator=(
    PlatformInfoSerializer&& other) = default;

bool PlatformInfoSerializer::operator==(
    const PlatformInfoSerializer& other) const {
  return platform_info_ == other.platform_info_;
}

bool PlatformInfoSerializer::operator!=(
    const PlatformInfoSerializer& other) const {
  return !(*this == other);
}

// static
absl::optional<PlatformInfoSerializer> PlatformInfoSerializer::TryParse(
    base::StringPiece json) {
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  if (!value || !value.value().is_dict()) {
    return absl::nullopt;
  }

  PlatformInfoSerializer parser;

  // NOTE: values in value.value() take precedence in the case of intersecting
  // keys. So because there is no way to remove keys from the underlying
  // Dictionary, all values will be overwritten.
  parser.platform_info_.MergeDictionary(&value.value());
  return parser;
}

void PlatformInfoSerializer::SetMaxWidth(int max_width) {
  platform_info_.SetIntKey(kMaxWidthProperty, max_width);
}

void PlatformInfoSerializer::SetMaxHeight(int max_height) {
  platform_info_.SetIntKey(kMaxHeightProperty, max_height);
}

void PlatformInfoSerializer::SetMaxFrameRate(int max_frame_rate) {
  platform_info_.SetIntKey(kMaxFrameRateProperty, max_frame_rate);
}

void PlatformInfoSerializer::SetSupportedCryptoBlockFormat(std::string format) {
  platform_info_.SetStringKey(kSupportedCryptoBlockFormatProperty,
                              std::move(format));
}

void PlatformInfoSerializer::SetMaxChannels(int max_channels) {
  platform_info_.SetIntKey(kMaxChannelsProperty, max_channels);
}

void PlatformInfoSerializer::SetPcmSurroundSoundSupported(bool is_supported) {
  platform_info_.SetBoolKey(kPcmSurroundSoundSupportedProperty, is_supported);
}

void PlatformInfoSerializer::SetPlatformDobleVisionEnabled(bool is_enabled) {
  platform_info_.SetBoolKey(kIsPlatformDolbyVisionEnabledProperty, is_enabled);
}

void PlatformInfoSerializer::SetDolbyVisionSupported(bool is_supported) {
  platform_info_.SetBoolKey(kIsDolbyVisionSupportedProperty, is_supported);
}

void PlatformInfoSerializer::SetDolbyVision4kP60Supported(bool is_supported) {
  platform_info_.SetBoolKey(kIsDolbyVision4kP60SupportedProperty, is_supported);
}

void PlatformInfoSerializer::SetDolbyVisionSupportedByCurrentHdmiMode(
    bool is_supported) {
  platform_info_.SetBoolKey(kIsDolbyVisionSupportedByCurrentHdmiModeProperty,
                            is_supported);
}

void PlatformInfoSerializer::SetHdmiVideoModeSwitchEnabled(bool is_enabled) {
  platform_info_.SetBoolKey(kIsHdmiVideoModeSwitchEnabledProperty, is_enabled);
}

void PlatformInfoSerializer::SetPlatformHevcEnabled(bool is_enabled) {
  platform_info_.SetBoolKey(kIsPlatformHevcEnabledProperty, is_enabled);
}

void PlatformInfoSerializer::SetHdmiModeHdrCheckEnforced(bool is_enforced) {
  platform_info_.SetBoolKey(kIsHdmiModeHdrCheckEnforcedProperty, is_enforced);
}

void PlatformInfoSerializer::SetHdrSupportedByCurrentHdmiMode(
    bool is_supported) {
  platform_info_.SetBoolKey(kIsHdrSupportedByCurrentHdmiModeProperty,
                            is_supported);
}

void PlatformInfoSerializer::SetSmpteSt2084Supported(bool is_supported) {
  platform_info_.SetBoolKey(kIsSmpteSt2084SupportedProperty, is_supported);
}

void PlatformInfoSerializer::SetHglSupported(bool is_supported) {
  platform_info_.SetBoolKey(kIsHlgSupportedProperty, is_supported);
}

void PlatformInfoSerializer::SetHdrFeatureEnabled(bool is_enabled) {
  platform_info_.SetBoolKey(kIsHdrFeatureEnabledProperty, is_enabled);
}

void PlatformInfoSerializer::SetSupportedLegacyVp9Levels(
    std::vector<int> levels) {
  std::vector<base::Value> values;
  values.reserve(levels.size());
  for (int level : levels) {
    values.emplace_back(level);
  }

  platform_info_.SetKey(kSupportedLegacyVp9LevelsProperty,
                        base::Value(std::move(values)));
}

void PlatformInfoSerializer::SetHdcpVersion(int hdcp_version) {
  platform_info_.SetIntKey(kHdcpVersionProperty, hdcp_version);
}

void PlatformInfoSerializer::SetSpatialRenderingSupportMask(int mask) {
  platform_info_.SetIntKey(kSpatialRenderingSupportMaskProperty, mask);
}

void PlatformInfoSerializer::SetMaxFillRate(int max_fill_rate) {
  platform_info_.SetIntKey(kMaxFillrateProperty, max_fill_rate);
}

void PlatformInfoSerializer::SetSupportedAudioCodecs(
    std::vector<AudioCodecInfo> codec_infos) {
  SetWithKey(&platform_info_, std::move(codec_infos));
}

void PlatformInfoSerializer::SetSupportedVideoCodecs(
    std::vector<VideoCodecInfo> codec_infos) {
  SetWithKey(&platform_info_, std::move(codec_infos));
}

absl::optional<int> PlatformInfoSerializer::MaxWidth() const {
  return ParseWithKey<int>(platform_info_, kMaxWidthProperty);
}

absl::optional<int> PlatformInfoSerializer::MaxHeight() const {
  return ParseWithKey<int>(platform_info_, kMaxHeightProperty);
}

absl::optional<int> PlatformInfoSerializer::MaxFrameRate() const {
  return ParseWithKey<int>(platform_info_, kMaxFrameRateProperty);
}

absl::optional<std::string> PlatformInfoSerializer::SupportedCryptoBlockFormat()
    const {
  return ParseWithKey<std::string>(platform_info_,
                                   kSupportedCryptoBlockFormatProperty);
}

absl::optional<int> PlatformInfoSerializer::MaxChannels() const {
  return ParseWithKey<int>(platform_info_, kMaxChannelsProperty);
}

absl::optional<bool> PlatformInfoSerializer::PcmSurroundSoundSupported() const {
  return ParseWithKey<bool>(platform_info_, kPcmSurroundSoundSupportedProperty);
}

absl::optional<bool> PlatformInfoSerializer::IsPlatformDobleVisionEnabled()
    const {
  return ParseWithKey<bool>(platform_info_,
                            kIsPlatformDolbyVisionEnabledProperty);
}

absl::optional<bool> PlatformInfoSerializer::IsDolbyVisionSupported() const {
  return ParseWithKey<bool>(platform_info_, kIsDolbyVisionSupportedProperty);
}

absl::optional<bool> PlatformInfoSerializer::IsDolbyVision4kP60Supported()
    const {
  return ParseWithKey<bool>(platform_info_,
                            kIsDolbyVision4kP60SupportedProperty);
}

absl::optional<bool>
PlatformInfoSerializer::IsDolbyVisionSupportedByCurrentHdmiMode() const {
  return ParseWithKey<bool>(platform_info_,
                            kIsDolbyVisionSupportedByCurrentHdmiModeProperty);
}

absl::optional<bool> PlatformInfoSerializer::IsHdmiVideoModeSwitchEnabled()
    const {
  return ParseWithKey<bool>(platform_info_,
                            kIsHdmiVideoModeSwitchEnabledProperty);
}

absl::optional<bool> PlatformInfoSerializer::IsPlatformHevcEnabled() const {
  return ParseWithKey<bool>(platform_info_, kIsPlatformHevcEnabledProperty);
}

absl::optional<bool> PlatformInfoSerializer::IsHdmiModeHdrCheckEnforced()
    const {
  return ParseWithKey<bool>(platform_info_,
                            kIsHdmiModeHdrCheckEnforcedProperty);
}

absl::optional<bool> PlatformInfoSerializer::IsHdrSupportedByCurrentHdmiMode()
    const {
  return ParseWithKey<bool>(platform_info_,
                            kIsHdrSupportedByCurrentHdmiModeProperty);
}

absl::optional<bool> PlatformInfoSerializer::IsSmpteSt2084Supported() const {
  return ParseWithKey<bool>(platform_info_, kIsSmpteSt2084SupportedProperty);
}

absl::optional<bool> PlatformInfoSerializer::IsHglSupported() const {
  return ParseWithKey<bool>(platform_info_, kIsHlgSupportedProperty);
}

absl::optional<bool> PlatformInfoSerializer::IsHdrFeatureEnabled() const {
  return ParseWithKey<bool>(platform_info_, kIsHdrFeatureEnabledProperty);
}

absl::optional<std::vector<int>>
PlatformInfoSerializer::SupportedLegacyVp9Levels() const {
  return ParseWithKey<std::vector<int>>(platform_info_,
                                        kSupportedLegacyVp9LevelsProperty);
}

absl::optional<int> PlatformInfoSerializer::HdcpVersion() const {
  return ParseWithKey<int>(platform_info_, kHdcpVersionProperty);
}

absl::optional<int> PlatformInfoSerializer::SpatialRenderingSupportMask()
    const {
  return ParseWithKey<int>(platform_info_,
                           kSpatialRenderingSupportMaskProperty);
}

absl::optional<int> PlatformInfoSerializer::MaxFillRate() const {
  return ParseWithKey<int>(platform_info_, kMaxFillrateProperty);
}

absl::optional<std::vector<PlatformInfoSerializer::AudioCodecInfo>>
PlatformInfoSerializer::SupportedAudioCodecs() const {
  return ParseWithKey<std::vector<AudioCodecInfo>>(
      platform_info_, kSupportedAudioCodecsProperty);
}

absl::optional<std::vector<PlatformInfoSerializer::VideoCodecInfo>>
PlatformInfoSerializer::SupportedVideoCodecs() const {
  return ParseWithKey<std::vector<VideoCodecInfo>>(
      platform_info_, kSupportedVideoCodecsProperty);
}

std::string PlatformInfoSerializer::ToJson() const {
  std::string json;
  bool success = base::JSONWriter::Write(platform_info_, &json);
  DCHECK(success);
  return json;
}

bool PlatformInfoSerializer::IsValid() const {
  return MaxWidth() && MaxHeight() && MaxFrameRate() &&
         SupportedCryptoBlockFormat() && MaxChannels() &&
         PcmSurroundSoundSupported();
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
