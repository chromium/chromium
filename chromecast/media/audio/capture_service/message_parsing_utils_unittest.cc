// Copyright 2019 The Chromium Authors. All rights reserved.
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

constexpr size_t kTotalHeaderBytes = 16;
constexpr size_t kFrames = 10;
constexpr size_t kChannels = 2;
constexpr StreamInfo kStreamInfo =
    StreamInfo{StreamType::kSoftwareEchoCancelled,
               AudioCodec::kPcm,
               kChannels,
               SampleFormat::PLANAR_FLOAT,
               16000,
               kFrames};
constexpr PacketInfo kHandshakePacketInfo = {MessageType::kHandshake,
                                             kStreamInfo, 0};
constexpr PacketInfo kPcmAudioPacketInfo = {MessageType::kPcmAudio, kStreamInfo,
                                            0};

TEST(MessageParsingUtilsTest, ValidPlanarFloat) {
  size_t data_size = kTotalHeaderBytes / sizeof(float) + kFrames * kChannels;
  std::vector<float> data(data_size, .0f);
  PopulateHeader(reinterpret_cast<char*>(data.data()),
                 data.size() * sizeof(float), kPcmAudioPacketInfo);
  // Fill the last k frames, i.e., the second channel, with 0.5f.
  for (size_t i = data_size - kFrames; i < data_size; i++) {
    data[i] = .5f;
  }

  // Audio header.
  PacketInfo info;
  bool success =
      ReadHeader(reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
                 data_size * sizeof(float) - sizeof(uint16_t), &info);
  EXPECT_TRUE(success);
  EXPECT_EQ(info.message_type, kPcmAudioPacketInfo.message_type);
  EXPECT_EQ(info.stream_info.stream_type, kStreamInfo.stream_type);
  EXPECT_EQ(info.stream_info.num_channels, kStreamInfo.num_channels);
  EXPECT_EQ(info.stream_info.sample_format, kStreamInfo.sample_format);
  EXPECT_EQ(info.stream_info.sample_rate, kStreamInfo.sample_rate);
  EXPECT_EQ(info.timestamp_us, kPcmAudioPacketInfo.timestamp_us);

  // Audio data.
  auto audio_bus = ::media::AudioBus::Create(kChannels, kFrames);
  success = ReadDataToAudioBus(
      kStreamInfo, reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(float) - sizeof(uint16_t), audio_bus.get());
  EXPECT_TRUE(success);
  for (size_t f = 0; f < kFrames; f++) {
    EXPECT_FLOAT_EQ(audio_bus->channel(0)[f], .0f);
    EXPECT_FLOAT_EQ(audio_bus->channel(1)[f], .5f);
  }
}

TEST(MessageParsingUtilsTest, ValidInterleavedInt16) {
  size_t data_size = kTotalHeaderBytes / sizeof(int16_t) + kFrames * kChannels;
  std::vector<int16_t> data(data_size, std::numeric_limits<int16_t>::max());
  PacketInfo packet_info = kPcmAudioPacketInfo;
  packet_info.stream_info.sample_format = SampleFormat::INTERLEAVED_INT16;
  PopulateHeader(reinterpret_cast<char*>(data.data()),
                 data.size() * sizeof(int16_t), packet_info);
  // Fill the second channel with min().
  for (size_t i = kTotalHeaderBytes / sizeof(int16_t) + 1; i < data_size;
       i += 2) {
    data[i] = std::numeric_limits<int16_t>::min();
  }

  auto audio_bus = ::media::AudioBus::Create(kChannels, kFrames);
  bool success = ReadDataToAudioBus(
      packet_info.stream_info,
      reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(int16_t) - sizeof(uint16_t), audio_bus.get());
  EXPECT_TRUE(success);
  for (size_t f = 0; f < kFrames; f++) {
    EXPECT_FLOAT_EQ(audio_bus->channel(0)[f], 1.0f);
    EXPECT_FLOAT_EQ(audio_bus->channel(1)[f], -1.0f);
  }
}

TEST(MessageParsingUtilsTest, ValidInterleavedInt32) {
  size_t data_size = kTotalHeaderBytes / sizeof(int32_t) + kFrames * kChannels;
  std::vector<int32_t> data(data_size, std::numeric_limits<int32_t>::min());
  PacketInfo packet_info = kPcmAudioPacketInfo;
  packet_info.stream_info.sample_format = SampleFormat::INTERLEAVED_INT32;
  PopulateHeader(reinterpret_cast<char*>(data.data()),
                 data.size() * sizeof(int32_t), packet_info);
  // Fill the second channel with max().
  for (size_t i = kTotalHeaderBytes / sizeof(int32_t) + 1; i < data_size;
       i += 2) {
    data[i] = std::numeric_limits<int32_t>::max();
  }

  auto audio_bus = ::media::AudioBus::Create(kChannels, kFrames);
  bool success = ReadDataToAudioBus(
      packet_info.stream_info,
      reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(int32_t) - sizeof(uint16_t), audio_bus.get());
  EXPECT_TRUE(success);
  for (size_t f = 0; f < kFrames; f++) {
    EXPECT_FLOAT_EQ(audio_bus->channel(0)[f], -1.0f);
    EXPECT_FLOAT_EQ(audio_bus->channel(1)[f], 1.0f);
  }
}

TEST(MessageParsingUtilsTest, InvalidType) {
  size_t data_size = kTotalHeaderBytes / sizeof(float);
  std::vector<float> data(data_size, 1.0f);
  // Request packet
  PacketInfo request_packet_info = kHandshakePacketInfo;
  PopulateHeader(reinterpret_cast<char*>(data.data()),
                 data.size() * sizeof(float), request_packet_info);
  *(reinterpret_cast<uint8_t*>(data.data()) +
    offsetof(struct PacketHeader, stream_type)) =
      static_cast<uint8_t>(StreamType::kLastType) + 1;
  bool success = ReadHeader(
      reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(float) - sizeof(uint16_t), &request_packet_info);
  EXPECT_FALSE(success);

  // PCM audio packet
  PacketInfo pcm_audio_packet_info = kPcmAudioPacketInfo;
  PopulateHeader(reinterpret_cast<char*>(data.data()),
                 data.size() * sizeof(float), pcm_audio_packet_info);
  *(reinterpret_cast<uint8_t*>(data.data()) +
    offsetof(struct PacketHeader, stream_type)) =
      static_cast<uint8_t>(StreamType::kLastType) + 1;
  success = ReadHeader(reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
                       data_size * sizeof(float) - sizeof(uint16_t),
                       &pcm_audio_packet_info);
  EXPECT_FALSE(success);
}

TEST(MessageParsingUtilsTest, InvalidCodec) {
  size_t data_size = kTotalHeaderBytes / sizeof(float);
  std::vector<float> data(data_size, 1.0f);
  PacketInfo packet_info = kHandshakePacketInfo;
  PopulateHeader(reinterpret_cast<char*>(data.data()),
                 data.size() * sizeof(float), packet_info);
  *(reinterpret_cast<uint8_t*>(data.data()) +
    offsetof(struct PacketHeader, codec_or_sample_format)) =
      static_cast<uint8_t>(AudioCodec::kLastCodec) + 1;

  bool success =
      ReadHeader(reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
                 data_size * sizeof(float) - sizeof(uint16_t), &packet_info);
  EXPECT_FALSE(success);
}

TEST(MessageParsingUtilsTest, InvalidFormat) {
  size_t data_size = kTotalHeaderBytes / sizeof(float);
  std::vector<float> data(data_size, 1.0f);
  PacketInfo packet_info = kPcmAudioPacketInfo;
  PopulateHeader(reinterpret_cast<char*>(data.data()),
                 data.size() * sizeof(float), packet_info);
  *(reinterpret_cast<uint8_t*>(data.data()) +
    offsetof(struct PacketHeader, codec_or_sample_format)) =
      static_cast<uint8_t>(SampleFormat::LAST_FORMAT) + 1;

  bool success =
      ReadHeader(reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
                 data_size * sizeof(float) - sizeof(uint16_t), &packet_info);
  EXPECT_FALSE(success);
}

TEST(MessageParsingUtilsTest, RequestMessage) {
  size_t data_size = kTotalHeaderBytes / sizeof(float);
  std::vector<float> data(data_size, 1.0f);
  PacketInfo packet_info = kHandshakePacketInfo;
  PopulateHeader(reinterpret_cast<char*>(data.data()),
                 data.size() * sizeof(float), packet_info);

  PacketInfo info;
  bool success =
      ReadHeader(reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
                 data_size * sizeof(float) - sizeof(uint16_t), &info);
  EXPECT_TRUE(success);
  EXPECT_EQ(info.message_type, kHandshakePacketInfo.message_type);
  EXPECT_EQ(info.stream_info.stream_type, kStreamInfo.stream_type);
  EXPECT_EQ(info.stream_info.audio_codec, kStreamInfo.audio_codec);
  EXPECT_EQ(info.stream_info.num_channels, kStreamInfo.num_channels);
  EXPECT_EQ(info.stream_info.sample_rate, kStreamInfo.sample_rate);
  EXPECT_EQ(info.stream_info.frames_per_buffer, kStreamInfo.frames_per_buffer);
}

TEST(MessageParsingUtilsTest, InvalidDataLength) {
  size_t data_size =
      kTotalHeaderBytes / sizeof(float) + kFrames * kChannels + 1;
  std::vector<float> data(data_size, 1.0f);
  PopulateHeader(reinterpret_cast<char*>(data.data()),
                 data.size() * sizeof(float), kPcmAudioPacketInfo);

  auto audio_bus = ::media::AudioBus::Create(kChannels, kFrames);
  bool success = ReadDataToAudioBus(
      kStreamInfo, reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(float) - sizeof(uint16_t), audio_bus.get());
  EXPECT_FALSE(success);
}

TEST(MessageParsingUtilsTest, NotAlignedData) {
  size_t data_size =
      kTotalHeaderBytes / sizeof(float) + kFrames * kChannels + 1;
  std::vector<float> data(data_size, 1.0f);
  PopulateHeader(reinterpret_cast<char*>(data.data()) + 1,
                 data.size() * sizeof(float) - 1, kPcmAudioPacketInfo);

  auto audio_bus = ::media::AudioBus::Create(kChannels, kFrames);
  bool success = ReadDataToAudioBus(
      kStreamInfo, reinterpret_cast<char*>(data.data()) + 1 + sizeof(uint16_t),
      data_size * sizeof(float) - 1 - sizeof(uint16_t), audio_bus.get());
  EXPECT_FALSE(success);
}

}  // namespace
}  // namespace capture_service
}  // namespace media
}  // namespace chromecast
