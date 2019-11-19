// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/message_parsing_util.h"

#include <vector>

#include "base/big_endian.h"
#include "chromecast/media/audio/capture_service/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace capture_service {
namespace {

using chromecast::media::capture_service::SampleFormat;

constexpr size_t kTotalHeaderBytes = 16;
constexpr size_t kFrames = 10;
constexpr size_t kChannels = 2;

void FillHeader(char* data, size_t size, uint16_t format) {
  base::BigEndianWriter writer(data, size);
  uint16_t message_size = size - sizeof(uint16_t);
  writer.WriteU16(message_size);
  // Header.
  writer.WriteU16(static_cast<uint16_t>(kChannels));
  writer.WriteU16(format);
  writer.WriteU16(uint16_t(0));  // Padding.
  writer.WriteU64(uint64_t(0));  // Timestamp.
}

TEST(ReadDataToAudioBusTest, ValidInterleavedInt16) {
  size_t data_size = kTotalHeaderBytes / sizeof(int16_t) + kFrames * kChannels;
  std::vector<int16_t> data(data_size, std::numeric_limits<int16_t>::max());
  FillHeader(reinterpret_cast<char*>(data.data()),
             data.size() * sizeof(int16_t),
             static_cast<uint16_t>(SampleFormat::INTERLEAVED_INT16));
  // Fill the second channel with min().
  for (size_t i = kTotalHeaderBytes / sizeof(int16_t) + 1; i < data_size;
       i += 2) {
    data[i] = std::numeric_limits<int16_t>::min();
  }

  int64_t timestamp = -1;
  auto audio = ReadDataToAudioBus(
      reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(int16_t) - sizeof(uint16_t), &timestamp);
  EXPECT_TRUE(audio.has_value());
  ::media::AudioBus* audio_ptr = audio->get();
  EXPECT_EQ(audio_ptr->channels(), static_cast<int>(kChannels));
  EXPECT_EQ(static_cast<size_t>(audio_ptr->frames()), kFrames);
  EXPECT_EQ(timestamp, 0);
  for (size_t f = 0; f < kFrames; f++) {
    EXPECT_FLOAT_EQ(audio_ptr->channel(0)[f], 1.0f);
    EXPECT_FLOAT_EQ(audio_ptr->channel(1)[f], -1.0f);
  }
}

TEST(ReadDataToAudioBusTest, ValidInterleavedInt32) {
  size_t data_size = kTotalHeaderBytes / sizeof(int32_t) + kFrames * kChannels;
  std::vector<int32_t> data(data_size, std::numeric_limits<int32_t>::min());
  FillHeader(reinterpret_cast<char*>(data.data()),
             data.size() * sizeof(int32_t),
             static_cast<uint16_t>(SampleFormat::INTERLEAVED_INT32));
  // Fill the second channel with max().
  for (size_t i = kTotalHeaderBytes / sizeof(int32_t) + 1; i < data_size;
       i += 2) {
    data[i] = std::numeric_limits<int32_t>::max();
  }

  int64_t timestamp = -1;
  auto audio = ReadDataToAudioBus(
      reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(int32_t) - sizeof(uint16_t), &timestamp);
  EXPECT_TRUE(audio.has_value());
  ::media::AudioBus* audio_ptr = audio->get();
  EXPECT_EQ(audio_ptr->channels(), static_cast<int>(kChannels));
  EXPECT_EQ(static_cast<size_t>(audio_ptr->frames()), kFrames);
  EXPECT_EQ(timestamp, 0);
  for (size_t f = 0; f < kFrames; f++) {
    EXPECT_FLOAT_EQ(audio_ptr->channel(0)[f], -1.0f);
    EXPECT_FLOAT_EQ(audio_ptr->channel(1)[f], 1.0f);
  }
}

TEST(ReadDataToAudioBusTest, ValidPlanarFloat) {
  size_t data_size = kTotalHeaderBytes / sizeof(float) + kFrames * kChannels;
  std::vector<float> data(data_size, .0f);
  FillHeader(reinterpret_cast<char*>(data.data()), data.size() * sizeof(float),
             static_cast<uint16_t>(SampleFormat::PLANAR_FLOAT));
  // Fill the last k frames, i.e., the second channel, with 0.5f.
  for (size_t i = data_size - kFrames; i < data_size; i++) {
    data[i] = .5f;
  }

  int64_t timestamp = -1;
  auto audio = ReadDataToAudioBus(
      reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(float) - sizeof(uint16_t), &timestamp);
  EXPECT_TRUE(audio.has_value());
  ::media::AudioBus* audio_ptr = audio->get();
  EXPECT_EQ(audio_ptr->channels(), static_cast<int>(kChannels));
  EXPECT_EQ(static_cast<size_t>(audio_ptr->frames()), kFrames);
  EXPECT_EQ(timestamp, 0);
  for (size_t f = 0; f < kFrames; f++) {
    EXPECT_FLOAT_EQ(audio_ptr->channel(0)[f], .0f);
    EXPECT_FLOAT_EQ(audio_ptr->channel(1)[f], .5f);
  }
}

TEST(ReadDataToAudioBusTest, InvalidFormat) {
  size_t data_size = kTotalHeaderBytes / sizeof(float) + kFrames * kChannels;
  std::vector<float> data(data_size, 1.0f);
  FillHeader(reinterpret_cast<char*>(data.data()), data.size() * sizeof(float),
             static_cast<uint16_t>(SampleFormat::PLANAR_FLOAT) + 1);

  int64_t timestamp = -1;
  auto audio = ReadDataToAudioBus(
      reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(float) - sizeof(uint16_t), &timestamp);
  EXPECT_FALSE(audio.has_value());
}

TEST(ReadDataToAudioBusTest, EmptyMessageData) {
  size_t data_size = kTotalHeaderBytes / sizeof(float);
  std::vector<float> data(data_size, 1.0f);
  FillHeader(reinterpret_cast<char*>(data.data()), data.size() * sizeof(float),
             static_cast<uint16_t>(SampleFormat::PLANAR_FLOAT));

  int64_t timestamp = -1;
  auto audio = ReadDataToAudioBus(
      reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(float) - sizeof(uint16_t), &timestamp);
  EXPECT_FALSE(audio.has_value());
}

TEST(ReadDataToAudioBusTest, InvalidDataLength) {
  size_t data_size =
      kTotalHeaderBytes / sizeof(float) + kFrames * kChannels + 1;
  std::vector<float> data(data_size, 1.0f);
  FillHeader(reinterpret_cast<char*>(data.data()), data.size() * sizeof(float),
             static_cast<uint16_t>(SampleFormat::PLANAR_FLOAT));

  int64_t timestamp = -1;
  auto audio = ReadDataToAudioBus(
      reinterpret_cast<char*>(data.data()) + sizeof(uint16_t),
      data_size * sizeof(float) - sizeof(uint16_t), &timestamp);
  EXPECT_FALSE(audio.has_value());
}

TEST(ReadDataToAudioBusTest, NotAlignedData) {
  size_t data_size =
      kTotalHeaderBytes / sizeof(float) + kFrames * kChannels + 1;
  std::vector<float> data(data_size, 1.0f);
  FillHeader(reinterpret_cast<char*>(data.data()) + 1,
             data.size() * sizeof(float) - 1,
             static_cast<uint16_t>(SampleFormat::PLANAR_FLOAT));

  int64_t timestamp = -1;
  auto audio = ReadDataToAudioBus(
      reinterpret_cast<char*>(data.data()) + 1 + sizeof(uint16_t),
      data_size * sizeof(float) - 1 - sizeof(uint16_t), &timestamp);
  EXPECT_FALSE(audio.has_value());
}

}  // namespace
}  // namespace capture_service
}  // namespace media
}  // namespace chromecast
