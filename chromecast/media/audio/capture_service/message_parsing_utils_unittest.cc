// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/message_parsing_utils.h"

#include <vector>

#include "base/big_endian.h"
#include "chromecast/media/audio/capture_service/constants.h"
#include "chromecast/media/audio/capture_service/packet_header.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace capture_service {
namespace {

constexpr size_t kFrames = 10;
constexpr size_t kChannels = 2;
constexpr StreamInfo kStreamInfo =
    StreamInfo{StreamType::kSoftwareEchoCancelled,
               AudioCodec::kPcm,
               kChannels,
               SampleFormat::PLANAR_FLOAT,
               16000,
               kFrames};

class PacketHeaderTest
    : public testing::TestWithParam<
          std::tuple<StreamType, AudioCodec, int, SampleFormat, int, int>> {
 protected:
  StreamInfo GetStreamInfo() {
    StreamInfo info;
    info.stream_type = std::get<0>(GetParam());
    info.audio_codec = std::get<1>(GetParam());
    info.num_channels = std::get<2>(GetParam());
    info.sample_format = std::get<3>(GetParam());
    info.sample_rate = std::get<4>(GetParam());
    info.frames_per_buffer = std::get<5>(GetParam());
    return info;
  }
};

TEST_P(PacketHeaderTest, HandshakeMessage) {
  std::vector<char> data(sizeof(HandshakePacket), 0);
  StreamInfo stream_info = GetStreamInfo();
  PopulateHandshakeMessage(data.data(), data.size(), stream_info);

  StreamInfo info_out;
  bool success =
      ReadHandshakeMessage(data.data() + sizeof(uint16_t),
                           data.size() - sizeof(uint16_t), &info_out);
  EXPECT_TRUE(success);
  EXPECT_EQ(info_out.stream_type, stream_info.stream_type);
  EXPECT_EQ(info_out.audio_codec, stream_info.audio_codec);
  EXPECT_EQ(info_out.sample_format, stream_info.sample_format);
  EXPECT_EQ(info_out.num_channels, stream_info.num_channels);
  EXPECT_EQ(info_out.sample_rate, stream_info.sample_rate);
  EXPECT_EQ(info_out.frames_per_buffer, stream_info.frames_per_buffer);
}

TEST(MessageParsingUtilsTest, PcmAudioMessage) {
  size_t data_size = sizeof(PcmPacketHeader) / sizeof(float);
  std::vector<float> data(data_size, 1.0f);
  int64_t timestamp_us = 100;
  PopulatePcmAudioHeader(reinterpret_cast<char*>(data.data()),
                         data.size() * sizeof(float), kStreamInfo.stream_type,
                         timestamp_us);

  int64_t timestamp_out = 0;
  bool success = ReadPcmAudioHeader(
      reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(float) - sizeof(uint16_t), kStreamInfo,
      &timestamp_out);
  EXPECT_TRUE(success);
  EXPECT_EQ(timestamp_out, timestamp_us);
}

TEST(MessageParsingUtilsTest, ValidPlanarFloat) {
  size_t data_size =
      sizeof(PcmPacketHeader) / sizeof(float) + kFrames * kChannels;
  std::vector<float> data(data_size, .0f);
  PopulatePcmAudioHeader(reinterpret_cast<char*>(data.data()),
                         data.size() * sizeof(float), kStreamInfo.stream_type,
                         0);
  // Fill the last k frames, i.e., the second channel, with 0.5f.
  for (size_t i = data_size - kFrames; i < data_size; i++) {
    data[i] = .5f;
  }

  auto audio_bus = ::media::AudioBus::Create(kChannels, kFrames);
  bool success = ReadDataToAudioBus(
      kStreamInfo, reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(float) - sizeof(uint16_t), audio_bus.get());
  EXPECT_TRUE(success);
  for (size_t f = 0; f < kFrames; f++) {
    EXPECT_FLOAT_EQ(audio_bus->channel(0)[f], .0f);
    EXPECT_FLOAT_EQ(audio_bus->channel(1)[f], .5f);
  }
}

TEST(MessageParsingUtilsTest, ValidInterleavedInt16) {
  size_t data_size =
      sizeof(PcmPacketHeader) / sizeof(int16_t) + kFrames * kChannels;
  std::vector<int16_t> data(data_size, std::numeric_limits<int16_t>::max());
  PopulatePcmAudioHeader(reinterpret_cast<char*>(data.data()),
                         data.size() * sizeof(int16_t), kStreamInfo.stream_type,
                         0);
  // Fill the second channel with min().
  for (size_t i = sizeof(PcmPacketHeader) / sizeof(int16_t) + 1; i < data_size;
       i += 2) {
    data[i] = std::numeric_limits<int16_t>::min();
  }

  auto audio_bus = ::media::AudioBus::Create(kChannels, kFrames);
  StreamInfo stream_info = kStreamInfo;
  stream_info.sample_format = SampleFormat::INTERLEAVED_INT16;
  bool success = ReadDataToAudioBus(
      stream_info, reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(int16_t) - sizeof(uint16_t), audio_bus.get());
  EXPECT_TRUE(success);
  for (size_t f = 0; f < kFrames; f++) {
    EXPECT_FLOAT_EQ(audio_bus->channel(0)[f], 1.0f);
    EXPECT_FLOAT_EQ(audio_bus->channel(1)[f], -1.0f);
  }
}

TEST(MessageParsingUtilsTest, ValidInterleavedInt32) {
  size_t data_size =
      sizeof(PcmPacketHeader) / sizeof(int32_t) + kFrames * kChannels;
  std::vector<int32_t> data(data_size, std::numeric_limits<int32_t>::min());
  PopulatePcmAudioHeader(reinterpret_cast<char*>(data.data()),
                         data.size() * sizeof(int32_t), kStreamInfo.stream_type,
                         0);
  // Fill the second channel with max().
  for (size_t i = sizeof(PcmPacketHeader) / sizeof(int32_t) + 1; i < data_size;
       i += 2) {
    data[i] = std::numeric_limits<int32_t>::max();
  }

  auto audio_bus = ::media::AudioBus::Create(kChannels, kFrames);
  StreamInfo stream_info = kStreamInfo;
  stream_info.sample_format = SampleFormat::INTERLEAVED_INT32;
  bool success = ReadDataToAudioBus(
      stream_info, reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(int32_t) - sizeof(uint16_t), audio_bus.get());
  EXPECT_TRUE(success);
  for (size_t f = 0; f < kFrames; f++) {
    EXPECT_FLOAT_EQ(audio_bus->channel(0)[f], -1.0f);
    EXPECT_FLOAT_EQ(audio_bus->channel(1)[f], 1.0f);
  }
}

TEST(MessageParsingUtilsTest, InvalidTypeHandshake) {
  std::vector<char> data(sizeof(HandshakePacket), 0);
  StreamInfo stream_info = kStreamInfo;
  PopulateHandshakeMessage(data.data(), data.size(), stream_info);
  *(reinterpret_cast<uint8_t*>(data.data()) +
    offsetof(struct HandshakePacket, stream_type)) =
      static_cast<uint8_t>(StreamType::kLastType) + 1;
  bool success =
      ReadHandshakeMessage(data.data() + sizeof(uint16_t),
                           data.size() - sizeof(uint16_t), &stream_info);
  EXPECT_FALSE(success);
}

TEST(MessageParsingUtilsTest, InvalidTypePcmAudio) {
  size_t data_size = sizeof(PcmPacketHeader) / sizeof(float);
  std::vector<float> data(data_size, 1.0f);
  PopulatePcmAudioHeader(reinterpret_cast<char*>(data.data()),
                         data.size() * sizeof(float), kStreamInfo.stream_type,
                         0);
  *(reinterpret_cast<uint8_t*>(data.data()) +
    offsetof(struct PcmPacketHeader, stream_type)) =
      static_cast<uint8_t>(StreamType::kLastType) + 1;
  int64_t timestamp_us;
  bool success = ReadPcmAudioHeader(
      reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(float) - sizeof(uint16_t), kStreamInfo, &timestamp_us);
  EXPECT_FALSE(success);
}

TEST(MessageParsingUtilsTest, InvalidCodec) {
  std::vector<char> data(sizeof(HandshakePacket), 0);
  StreamInfo stream_info = kStreamInfo;
  PopulateHandshakeMessage(data.data(), data.size(), stream_info);
  *(reinterpret_cast<uint8_t*>(data.data()) +
    offsetof(struct HandshakePacket, audio_codec)) =
      static_cast<uint8_t>(AudioCodec::kLastCodec) + 1;
  bool success =
      ReadHandshakeMessage(data.data() + sizeof(uint16_t),
                           data.size() - sizeof(uint16_t), &stream_info);
  EXPECT_FALSE(success);
}

TEST(MessageParsingUtilsTest, InvalidFormat) {
  std::vector<char> data(sizeof(HandshakePacket), 0);
  StreamInfo stream_info = kStreamInfo;
  PopulateHandshakeMessage(data.data(), data.size(), stream_info);
  *(reinterpret_cast<uint8_t*>(data.data()) +
    offsetof(struct HandshakePacket, sample_format)) =
      static_cast<uint8_t>(SampleFormat::LAST_FORMAT) + 1;
  bool success =
      ReadHandshakeMessage(data.data() + sizeof(uint16_t),
                           data.size() - sizeof(uint16_t), &stream_info);
  EXPECT_FALSE(success);
}

TEST(MessageParsingUtilsTest, InvalidDataLength) {
  size_t data_size =
      sizeof(PcmPacketHeader) / sizeof(float) + kFrames * kChannels + 1;
  std::vector<float> data(data_size, 1.0f);
  PopulatePcmAudioHeader(reinterpret_cast<char*>(data.data()),
                         data.size() * sizeof(float), kStreamInfo.stream_type,
                         0);

  auto audio_bus = ::media::AudioBus::Create(kChannels, kFrames);
  bool success = ReadDataToAudioBus(
      kStreamInfo, reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(float) - sizeof(uint16_t), audio_bus.get());
  EXPECT_FALSE(success);
}

TEST(MessageParsingUtilsTest, NotAlignedData) {
  size_t data_size =
      sizeof(PcmPacketHeader) / sizeof(float) + kFrames * kChannels + 1;
  std::vector<float> data(data_size, 1.0f);
  PopulatePcmAudioHeader(reinterpret_cast<char*>(data.data()) + 1,
                         data.size() * sizeof(float) - 1,
                         kStreamInfo.stream_type, 0);

  auto audio_bus = ::media::AudioBus::Create(kChannels, kFrames);
  bool success = ReadDataToAudioBus(
      kStreamInfo, reinterpret_cast<char*>(data.data()) + 1 + sizeof(uint16_t),
      data_size * sizeof(float) - 1 - sizeof(uint16_t), audio_bus.get());
  EXPECT_FALSE(success);
}

INSTANTIATE_TEST_SUITE_P(
    MessageParsingUtilsTest,
    PacketHeaderTest,
    testing::Combine(testing::Values(StreamType::kMicRaw,
                                     StreamType::kHardwareEchoRescaled),
                     testing::Values(AudioCodec::kPcm, AudioCodec::kOpus),
                     testing::Values(1, 8),
                     testing::Values(SampleFormat::INTERLEAVED_INT16,
                                     SampleFormat::PLANAR_FLOAT),
                     testing::Values(16000, 96000),
                     testing::Values(0, 32761)));

}  // namespace
}  // namespace capture_service
}  // namespace media
}  // namespace chromecast
