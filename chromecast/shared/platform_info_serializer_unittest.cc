// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shared/platform_info_serializer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

PlatformInfoSerializer ConvertAndValidate(
    const PlatformInfoSerializer& parser) {
  std::string json = parser.ToJson();
  absl::optional<PlatformInfoSerializer> deserialized_parser =
      PlatformInfoSerializer::TryParse(json);
  EXPECT_TRUE(deserialized_parser.has_value());
  EXPECT_EQ(parser, deserialized_parser.value());
  return std::move(deserialized_parser.value());
}

class PlatformInfoSerializerTest : public testing::Test {
 public:
  PlatformInfoSerializerTest() = default;
  ~PlatformInfoSerializerTest() override = default;

 protected:
  PlatformInfoSerializer serializer_;
};

TEST_F(PlatformInfoSerializerTest, MaxWidth) {
  serializer_.SetMaxWidth(42);
  EXPECT_EQ(42, ConvertAndValidate(serializer_).MaxWidth().value());

  serializer_.SetMaxWidth(12);
  EXPECT_EQ(12, ConvertAndValidate(serializer_).MaxWidth().value());

  serializer_.SetMaxWidth(7);
  EXPECT_EQ(7, ConvertAndValidate(serializer_).MaxWidth().value());
}

TEST_F(PlatformInfoSerializerTest, MaxHeight) {
  serializer_.SetMaxHeight(42);
  EXPECT_EQ(42, ConvertAndValidate(serializer_).MaxHeight().value());

  serializer_.SetMaxHeight(12);
  EXPECT_EQ(12, ConvertAndValidate(serializer_).MaxHeight().value());

  serializer_.SetMaxHeight(7);
  EXPECT_EQ(7, ConvertAndValidate(serializer_).MaxHeight().value());
}

TEST_F(PlatformInfoSerializerTest, MaxFrameRate) {
  serializer_.SetMaxFrameRate(42);
  EXPECT_EQ(42, ConvertAndValidate(serializer_).MaxFrameRate().value());

  serializer_.SetMaxFrameRate(12);
  EXPECT_EQ(12, ConvertAndValidate(serializer_).MaxFrameRate().value());

  serializer_.SetMaxFrameRate(7);
  EXPECT_EQ(7, ConvertAndValidate(serializer_).MaxFrameRate().value());
}

TEST_F(PlatformInfoSerializerTest, SupportedCryptoBlockFormat) {
  serializer_.SetSupportedCryptoBlockFormat("foo");
  EXPECT_EQ(
      "foo",
      ConvertAndValidate(serializer_).SupportedCryptoBlockFormat().value());

  serializer_.SetSupportedCryptoBlockFormat("bar");
  EXPECT_EQ(
      "bar",
      ConvertAndValidate(serializer_).SupportedCryptoBlockFormat().value());

  serializer_.SetSupportedCryptoBlockFormat("");
  EXPECT_EQ(
      "", ConvertAndValidate(serializer_).SupportedCryptoBlockFormat().value());
}

TEST_F(PlatformInfoSerializerTest, MaxChannels) {
  serializer_.SetMaxChannels(42);
  EXPECT_EQ(42, ConvertAndValidate(serializer_).MaxChannels().value());

  serializer_.SetMaxChannels(12);
  EXPECT_EQ(12, ConvertAndValidate(serializer_).MaxChannels().value());

  serializer_.SetMaxChannels(7);
  EXPECT_EQ(7, ConvertAndValidate(serializer_).MaxChannels().value());
}

TEST_F(PlatformInfoSerializerTest, PcmSurroundSoundSupported) {
  serializer_.SetPcmSurroundSoundSupported(true);
  EXPECT_TRUE(
      ConvertAndValidate(serializer_).PcmSurroundSoundSupported().value());

  serializer_.SetPcmSurroundSoundSupported(false);
  EXPECT_FALSE(
      ConvertAndValidate(serializer_).PcmSurroundSoundSupported().value());
}

TEST_F(PlatformInfoSerializerTest, PlatformDobleVisionEnabled) {
  serializer_.SetPlatformDobleVisionEnabled(true);
  EXPECT_TRUE(
      ConvertAndValidate(serializer_).IsPlatformDobleVisionEnabled().value());

  serializer_.SetPlatformDobleVisionEnabled(false);
  EXPECT_FALSE(
      ConvertAndValidate(serializer_).IsPlatformDobleVisionEnabled().value());
}

TEST_F(PlatformInfoSerializerTest, DolbyVisionSupported) {
  serializer_.SetDolbyVisionSupported(true);
  EXPECT_TRUE(ConvertAndValidate(serializer_).IsDolbyVisionSupported().value());

  serializer_.SetDolbyVisionSupported(false);
  EXPECT_FALSE(
      ConvertAndValidate(serializer_).IsDolbyVisionSupported().value());
}

TEST_F(PlatformInfoSerializerTest, DolbyVision4kP60Supported) {
  serializer_.SetDolbyVision4kP60Supported(true);
  EXPECT_TRUE(
      ConvertAndValidate(serializer_).IsDolbyVision4kP60Supported().value());

  serializer_.SetDolbyVision4kP60Supported(false);
  EXPECT_FALSE(
      ConvertAndValidate(serializer_).IsDolbyVision4kP60Supported().value());
}

TEST_F(PlatformInfoSerializerTest, DolbyVisionSupportedByCurrentHdmiMode) {
  serializer_.SetDolbyVisionSupportedByCurrentHdmiMode(true);
  EXPECT_TRUE(ConvertAndValidate(serializer_)
                  .IsDolbyVisionSupportedByCurrentHdmiMode()
                  .value());

  serializer_.SetDolbyVisionSupportedByCurrentHdmiMode(false);
  EXPECT_FALSE(ConvertAndValidate(serializer_)
                   .IsDolbyVisionSupportedByCurrentHdmiMode()
                   .value());
}

TEST_F(PlatformInfoSerializerTest, HdmiVideoModeSwitchEnabled) {
  serializer_.SetHdmiVideoModeSwitchEnabled(true);
  EXPECT_TRUE(
      ConvertAndValidate(serializer_).IsHdmiVideoModeSwitchEnabled().value());

  serializer_.SetHdmiVideoModeSwitchEnabled(false);
  EXPECT_FALSE(
      ConvertAndValidate(serializer_).IsHdmiVideoModeSwitchEnabled().value());
}

TEST_F(PlatformInfoSerializerTest, PlatformHevcEnabled) {
  serializer_.SetPlatformHevcEnabled(true);
  EXPECT_TRUE(ConvertAndValidate(serializer_).IsPlatformHevcEnabled().value());

  serializer_.SetPlatformHevcEnabled(false);
  EXPECT_FALSE(ConvertAndValidate(serializer_).IsPlatformHevcEnabled().value());
}

TEST_F(PlatformInfoSerializerTest, HdmiModeHdrCheckEnforced) {
  serializer_.SetHdmiModeHdrCheckEnforced(true);
  EXPECT_TRUE(
      ConvertAndValidate(serializer_).IsHdmiModeHdrCheckEnforced().value());

  serializer_.SetHdmiModeHdrCheckEnforced(false);
  EXPECT_FALSE(
      ConvertAndValidate(serializer_).IsHdmiModeHdrCheckEnforced().value());
}

TEST_F(PlatformInfoSerializerTest, HdrSupportedByCurrentHdmiMode) {
  serializer_.SetHdrSupportedByCurrentHdmiMode(true);
  EXPECT_TRUE(ConvertAndValidate(serializer_)
                  .IsHdrSupportedByCurrentHdmiMode()
                  .value());

  serializer_.SetHdrSupportedByCurrentHdmiMode(false);
  EXPECT_FALSE(ConvertAndValidate(serializer_)
                   .IsHdrSupportedByCurrentHdmiMode()
                   .value());
}

TEST_F(PlatformInfoSerializerTest, SmpteSt2084Supported) {
  serializer_.SetSmpteSt2084Supported(true);
  EXPECT_TRUE(ConvertAndValidate(serializer_).IsSmpteSt2084Supported().value());

  serializer_.SetSmpteSt2084Supported(false);
  EXPECT_FALSE(
      ConvertAndValidate(serializer_).IsSmpteSt2084Supported().value());
}

TEST_F(PlatformInfoSerializerTest, HglSupported) {
  serializer_.SetHglSupported(true);
  EXPECT_TRUE(ConvertAndValidate(serializer_).IsHglSupported().value());

  serializer_.SetHglSupported(false);
  EXPECT_FALSE(ConvertAndValidate(serializer_).IsHglSupported().value());
}

TEST_F(PlatformInfoSerializerTest, HdrFeatureEnabled) {
  serializer_.SetHdrFeatureEnabled(true);
  EXPECT_TRUE(ConvertAndValidate(serializer_).IsHdrFeatureEnabled().value());

  serializer_.SetHdrFeatureEnabled(false);
  EXPECT_FALSE(ConvertAndValidate(serializer_).IsHdrFeatureEnabled().value());
}

TEST_F(PlatformInfoSerializerTest, SupportedLegacyVp9Levels) {
  std::vector<int> source{42, 24, 7};
  serializer_.SetSupportedLegacyVp9Levels(source);
  auto converted =
      ConvertAndValidate(serializer_).SupportedLegacyVp9Levels().value();

  ASSERT_EQ(source.size(), converted.size());
  for (int i = 0; i < static_cast<int>(source.size()); i++) {
    EXPECT_EQ(source[i], converted[i]);
  }
}

TEST_F(PlatformInfoSerializerTest, HdcpVersion) {
  serializer_.SetHdcpVersion(42);
  EXPECT_EQ(42, ConvertAndValidate(serializer_).HdcpVersion().value());

  serializer_.SetHdcpVersion(12);
  EXPECT_EQ(12, ConvertAndValidate(serializer_).HdcpVersion().value());

  serializer_.SetHdcpVersion(7);
  EXPECT_EQ(7, ConvertAndValidate(serializer_).HdcpVersion().value());
}

TEST_F(PlatformInfoSerializerTest, SpatialRenderingSupportMask) {
  serializer_.SetSpatialRenderingSupportMask(42);
  EXPECT_EQ(
      42,
      ConvertAndValidate(serializer_).SpatialRenderingSupportMask().value());

  serializer_.SetSpatialRenderingSupportMask(12);
  EXPECT_EQ(
      12,
      ConvertAndValidate(serializer_).SpatialRenderingSupportMask().value());

  serializer_.SetSpatialRenderingSupportMask(7);
  EXPECT_EQ(
      7, ConvertAndValidate(serializer_).SpatialRenderingSupportMask().value());
}

TEST_F(PlatformInfoSerializerTest, MaxFillRate) {
  serializer_.SetMaxFillRate(42);
  EXPECT_EQ(42, ConvertAndValidate(serializer_).MaxFillRate().value());

  serializer_.SetMaxFillRate(12);
  EXPECT_EQ(12, ConvertAndValidate(serializer_).MaxFillRate().value());

  serializer_.SetMaxFillRate(7);
  EXPECT_EQ(7, ConvertAndValidate(serializer_).MaxFillRate().value());
}

TEST_F(PlatformInfoSerializerTest, IsValid) {
  serializer_.SetMaxWidth(0);
  EXPECT_FALSE(serializer_.IsValid());

  serializer_.SetMaxHeight(0);
  EXPECT_FALSE(serializer_.IsValid());

  serializer_.SetMaxFrameRate(0);
  EXPECT_FALSE(serializer_.IsValid());

  serializer_.SetSupportedCryptoBlockFormat("");
  EXPECT_FALSE(serializer_.IsValid());

  serializer_.SetMaxChannels(0);
  EXPECT_FALSE(serializer_.IsValid());

  serializer_.SetPcmSurroundSoundSupported(false);
  EXPECT_TRUE(serializer_.IsValid());
}

TEST_F(PlatformInfoSerializerTest, AudioCodecInfo) {
  PlatformInfoSerializer::AudioCodecInfo first{
      media::AudioCodec::kCodecAAC, media::SampleFormat::kSampleFormatU8, 42,
      16};
  PlatformInfoSerializer::AudioCodecInfo second{
      media::AudioCodec::kCodecOpus, media::SampleFormat::kSampleFormatS32, 18,
      34};
  PlatformInfoSerializer::AudioCodecInfo third{
      media::AudioCodec::kCodecAC3, media::SampleFormat::kSampleFormatPlanarS16,
      9, 99};

  std::vector<PlatformInfoSerializer::AudioCodecInfo> codec_infos{first, second,
                                                                  third};
  serializer_.SetSupportedAudioCodecs(codec_infos);

  auto converted = ConvertAndValidate(serializer_).SupportedAudioCodecs();
  ASSERT_TRUE(converted.has_value());
  ASSERT_EQ(codec_infos.size(), converted.value().size());
  for (int i = 0; i < static_cast<int>(codec_infos.size()); i++) {
    EXPECT_EQ(codec_infos[i], converted.value()[i]);
  }
}

TEST_F(PlatformInfoSerializerTest, VideoCodecInfo) {
  PlatformInfoSerializer::VideoCodecInfo first{
      media::VideoCodec::kCodecVP8, media::VideoProfile::kVP8ProfileAny};
  PlatformInfoSerializer::VideoCodecInfo second{
      media::VideoCodec::kCodecVP9, media::VideoProfile::kVP9Profile2};
  PlatformInfoSerializer::VideoCodecInfo third{
      media::VideoCodec::kCodecH264, media::VideoProfile::kH264Extended};

  std::vector<PlatformInfoSerializer::VideoCodecInfo> codec_infos{first, second,
                                                                  third};
  serializer_.SetSupportedVideoCodecs(codec_infos);

  auto converted = ConvertAndValidate(serializer_).SupportedVideoCodecs();
  ASSERT_TRUE(converted.has_value());
  ASSERT_EQ(codec_infos.size(), converted.value().size());
  for (int i = 0; i < static_cast<int>(codec_infos.size()); i++) {
    EXPECT_EQ(codec_infos[i], converted.value()[i]);
  }
}

}  // namespace chromecast
