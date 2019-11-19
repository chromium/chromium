// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/message_parsing_util.h"

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "chromecast/media/audio/capture_service/constants.h"
#include "media/base/limits.h"

namespace chromecast {
namespace media {
namespace capture_service {
namespace {

// Check if audio data is properly aligned and has valid frame size. Return the
// number of frames if they are all good, otherwise return 0 to indicate
// failure.
template <typename T>
int CheckAudioData(int channels, const char* data, size_t data_size) {
  if (reinterpret_cast<const uintptr_t>(data) % sizeof(T) != 0u) {
    LOG(ERROR) << "Misaligned audio data";
    return 0;
  }

  size_t frame_size = 0;
  int frames = 0;
  if (!base::CheckMul(channels, sizeof(T)).AssignIfValid(&frame_size) ||
      !base::CheckDiv(data_size, frame_size)
           .Cast<int>()
           .AssignIfValid(&frames)) {
    LOG(ERROR) << "Numeric overflow: " << data_size << " / (" << channels
               << " * " << sizeof(T) << ").";
    return 0;
  }
  if (!base::CheckMul(channels, frames).IsValid<int>()) {
    LOG(ERROR) << "Numeric overflow: " << channels << " * " << frames << ".";
    return 0;
  }
  if (data_size % frame_size != 0) {
    LOG(ERROR) << "Audio data size (" << data_size
               << ") is not an integer number of frames (" << frame_size
               << ").";
    return 0;
  }
  return frames;
}

template <typename Traits>
base::Optional<std::unique_ptr<::media::AudioBus>>
ConvertInterleavedData(int channels, const char* data, size_t data_size) {
  const int frames =
      CheckAudioData<typename Traits::ValueType>(channels, data, data_size);
  if (frames <= 0) {
    return base::nullopt;
  }
  auto audio = ::media::AudioBus::Create(channels, frames);
  audio->FromInterleaved<Traits>(
      reinterpret_cast<const typename Traits::ValueType*>(data), frames);
  return audio;
}

template <typename Traits>
base::Optional<std::unique_ptr<::media::AudioBus>>
ConvertPlanarData(int channels, const char* data, size_t data_size) {
  const int frames =
      CheckAudioData<typename Traits::ValueType>(channels, data, data_size);
  if (frames <= 0) {
    return base::nullopt;
  }
  auto audio = ::media::AudioBus::Create(channels, frames);

  const typename Traits::ValueType* base_data =
      reinterpret_cast<const typename Traits::ValueType*>(data);
  for (int c = 0; c < channels; ++c) {
    const typename Traits::ValueType* source = base_data + c * frames;
    float* dest = audio->channel(c);
    for (int f = 0; f < frames; ++f) {
      dest[f] = Traits::ToFloat(source[f]);
    }
  }

  return audio;
}

base::Optional<std::unique_ptr<::media::AudioBus>>
ConvertPlanarFloat(int channels, const char* data, size_t data_size) {
  const int frames = CheckAudioData<float>(channels, data, data_size);
  if (frames <= 0) {
    return base::nullopt;
  }
  auto audio = ::media::AudioBus::Create(channels, frames);

  const float* base_data = reinterpret_cast<const float*>(data);
  for (int c = 0; c < channels; ++c) {
    const float* source = base_data + c * frames;
    std::copy(source, source + frames, audio->channel(c));
  }

  return audio;
}

base::Optional<std::unique_ptr<::media::AudioBus>>
ConvertData(int channels, int format, const char* data, size_t data_size) {
  switch (format) {
    case capture_service::SampleFormat::INTERLEAVED_INT16:
      return ConvertInterleavedData<::media::SignedInt16SampleTypeTraits>(
          channels, data, data_size);
    case capture_service::SampleFormat::INTERLEAVED_INT32:
      return ConvertInterleavedData<::media::SignedInt32SampleTypeTraits>(
          channels, data, data_size);
    case capture_service::SampleFormat::INTERLEAVED_FLOAT:
      return ConvertInterleavedData<::media::Float32SampleTypeTraits>(
          channels, data, data_size);
    case capture_service::SampleFormat::PLANAR_INT16:
      return ConvertPlanarData<::media::SignedInt16SampleTypeTraits>(
          channels, data, data_size);
    case capture_service::SampleFormat::PLANAR_INT32:
      return ConvertPlanarData<::media::SignedInt32SampleTypeTraits>(
          channels, data, data_size);
    case capture_service::SampleFormat::PLANAR_FLOAT:
      return ConvertPlanarFloat(channels, data, data_size);
  }
  LOG(ERROR) << "Unknown sample format " << format;
  return base::nullopt;
}

}  // namespace

base::Optional<std::unique_ptr<::media::AudioBus>>
ReadDataToAudioBus(const char* data, size_t size, int64_t* timestamp) {
  // Padding bits are used make sure memory for |data| is well aligned for any
  // sample formats, i.e., the total bits of the header is a multiple of
  // 32-bits.
  uint16_t channels, format, padding;
  uint64_t timestamp_us;
  base::BigEndianReader data_reader(data, size);
  if (!data_reader.ReadU16(&channels) || !data_reader.ReadU16(&format) ||
      !data_reader.ReadU16(&padding) || padding != 0 ||
      !data_reader.ReadU64(&timestamp_us)) {
    LOG(ERROR) << "Invalid message header.";
    return base::nullopt;
  }
  if (channels > ::media::limits::kMaxChannels) {
    LOG(ERROR) << "Invalid number of channels: " << channels;
    return base::nullopt;
  }
  if (!base::CheckedNumeric<uint64_t>(timestamp_us)
           .Cast<int64_t>()
           .AssignIfValid(timestamp)) {
    LOG(ERROR) << "Invalid timestamp: " << timestamp_us;
  }
  return ConvertData(channels, format, data_reader.ptr(),
                     data_reader.remaining());
}

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast
