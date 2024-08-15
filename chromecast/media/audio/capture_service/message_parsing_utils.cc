// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/message_parsing_utils.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span_writer.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "chromecast/media/audio/capture_service/constants.h"
#include "chromecast/media/audio/capture_service/packet_header.h"
#include "media/base/limits.h"

namespace chromecast {
namespace media {
namespace capture_service {
namespace {

// Size in bytes of the header part of a handshake message.
constexpr size_t kHandshakeHeaderBytes =
    sizeof(HandshakePacket) - sizeof(uint16_t);

static_assert(sizeof(PcmPacketHeader) % 4 == 0,
              "Size of PCM audio packet header must be a multiple of 4 bytes.");
static_assert(sizeof(HandshakePacket) % 4 == 0,
              "Size of handshake packet must be a multiple of 4 bytes.");
static_assert(kPcmAudioHeaderBytes ==
                  sizeof(PcmPacketHeader) - sizeof(uint16_t),
              "Invalid message header size.");
static_assert(offsetof(struct PcmPacketHeader, message_type) ==
                  sizeof(uint16_t),
              "Invalid message header offset.");
static_assert(offsetof(struct HandshakePacket, message_type) ==
                  sizeof(uint16_t),
              "Invalid message header offset.");

// Check if audio data is properly aligned and has valid frame size. Return the
// number of frames if they are all good, otherwise return 0 to indicate
// failure.
template <typename T>
int CheckAudioData(int channels, const char* data, size_t data_size) {
  if (reinterpret_cast<const uintptr_t>(data) % sizeof(T) != 0u) {
    LOG(ERROR) << "Misaligned audio data";
    return 0;
  }

  const int frame_size = channels * sizeof(T);
  if (frame_size == 0) {
    LOG(ERROR) << "Frame size is 0.";
    return 0;
  }
  const int frames = data_size / frame_size;
  if (data_size % frame_size != 0) {
    LOG(ERROR) << "Audio data size (" << data_size
               << ") is not an integer number of frames (" << frame_size
               << ").";
    return 0;
  }
  return frames;
}

template <typename Traits>
bool ConvertInterleavedData(int channels,
                            const char* data,
                            size_t data_size,
                            ::media::AudioBus* audio) {
  const int frames =
      CheckAudioData<typename Traits::ValueType>(channels, data, data_size);
  if (frames <= 0) {
    return false;
  }

  DCHECK_EQ(frames, audio->frames());
  audio->FromInterleaved<Traits>(
      reinterpret_cast<const typename Traits::ValueType*>(data), frames);
  return true;
}

template <typename Traits>
bool ConvertPlanarData(int channels,
                       const char* data,
                       size_t data_size,
                       ::media::AudioBus* audio) {
  const int frames =
      CheckAudioData<typename Traits::ValueType>(channels, data, data_size);
  if (frames <= 0) {
    return false;
  }

  DCHECK_EQ(frames, audio->frames());
  const typename Traits::ValueType* base_data =
      reinterpret_cast<const typename Traits::ValueType*>(data);
  for (int c = 0; c < channels; ++c) {
    const typename Traits::ValueType* source = base_data + c * frames;
    float* dest = audio->channel(c);
    for (int f = 0; f < frames; ++f) {
      dest[f] = Traits::ToFloat(source[f]);
    }
  }
  return true;
}

bool ConvertPlanarFloat(int channels,
                        const char* data,
                        size_t data_size,
                        ::media::AudioBus* audio) {
  const int frames = CheckAudioData<float>(channels, data, data_size);
  if (frames <= 0) {
    return false;
  }

  DCHECK_EQ(frames, audio->frames());
  const float* base_data = reinterpret_cast<const float*>(data);
  for (int c = 0; c < channels; ++c) {
    const float* source = base_data + c * frames;
    std::copy(source, source + frames, audio->channel(c));
  }
  return true;
}

bool ConvertData(int channels,
                 SampleFormat format,
                 const char* data,
                 size_t data_size,
                 ::media::AudioBus* audio) {
  switch (format) {
    case capture_service::SampleFormat::INTERLEAVED_INT16:
      return ConvertInterleavedData<::media::SignedInt16SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::INTERLEAVED_INT32:
      return ConvertInterleavedData<::media::SignedInt32SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::INTERLEAVED_FLOAT:
      return ConvertInterleavedData<::media::Float32SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::PLANAR_INT16:
      return ConvertPlanarData<::media::SignedInt16SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::PLANAR_INT32:
      return ConvertPlanarData<::media::SignedInt32SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::PLANAR_FLOAT:
      return ConvertPlanarFloat(channels, data, data_size, audio);
  }
  LOG(ERROR) << "Unknown sample format " << static_cast<int>(format);
  return false;
}

}  // namespace

base::span<uint8_t> FillBuffer(base::span<uint8_t> buf,
                               base::span<const uint8_t> data) {
  auto [write_size, rem] = buf.split_at(sizeof(uint16_t));
  write_size.copy_from(base::numerics::U16ToBigEndian(
      base::checked_cast<uint16_t>(buf.size()) - uint16_t{sizeof(uint16_t)}));
  auto [write_data, uninit] = rem.split_at(data.size());
  write_data.copy_from(data);
  return uninit;
}

char* PopulatePcmAudioHeader(char* data_ptr,
                             size_t size,
                             StreamType stream_type,
                             int64_t timestamp_us) {
  PcmPacketHeader header;
  header.message_type = static_cast<uint8_t>(MessageType::kPcmAudio);
  header.stream_type = static_cast<uint8_t>(stream_type);
  header.timestamp_us = timestamp_us;

  auto data = base::as_writable_bytes(
      // TODO(crbug.com/328018028): PopulatePcmAudioHeader() should
      // get a span, not a pointer and length.
      UNSAFE_TODO(base::span(data_ptr, size)));
  auto header_as_bytes =
      base::byte_span_from_ref(header).subspan(sizeof(header.size));
  auto after = FillBuffer(data, header_as_bytes);
  // TODO(crbug.com/328018028): Return the span instead of just a pointer.
  return base::as_writable_chars(after).data();
}

void PopulateHandshakeMessage(char* data_ptr,
                              size_t size,
                              const StreamInfo& stream_info) {
  HandshakePacket packet;
  packet.message_type = static_cast<uint8_t>(MessageType::kHandshake);
  packet.stream_type = static_cast<uint8_t>(stream_info.stream_type);
  packet.audio_codec = static_cast<uint8_t>(stream_info.audio_codec);
  packet.sample_format = static_cast<uint8_t>(stream_info.sample_format);
  packet.num_channels = stream_info.num_channels;
  packet.num_frames = stream_info.frames_per_buffer;
  packet.sample_rate = stream_info.sample_rate;

  auto data = base::as_writable_bytes(
      // TODO(crbug.com/328018028): PopulateHandshakeMessage() should
      // get a span, not a pointer and length.
      UNSAFE_TODO(base::span(data_ptr, size)));
  auto packet_as_bytes =
      base::byte_span_from_ref(packet).subspan(sizeof(packet.size));
  FillBuffer(data, packet_as_bytes);
}

bool ReadPcmAudioHeader(const char* data,
                        size_t size,
                        const StreamInfo& stream_info,
                        int64_t* timestamp_us) {
  DCHECK(timestamp_us);
  if (size < kPcmAudioHeaderBytes) {
    LOG(ERROR) << "Message doesn't have a complete header: " << size << " v.s. "
               << kPcmAudioHeaderBytes << ".";
    return false;
  }
  PcmPacketHeader header;
  memcpy(&header.message_type, data, kPcmAudioHeaderBytes);
  if (static_cast<MessageType>(header.message_type) != MessageType::kPcmAudio) {
    LOG(ERROR) << "Message type mismatch.";
    return false;
  }
  if (static_cast<StreamType>(header.stream_type) != stream_info.stream_type) {
    LOG(ERROR) << "Stream type mistach.";
    return false;
  }
  *timestamp_us = header.timestamp_us;
  return true;
}

scoped_refptr<net::IOBufferWithSize> MakePcmAudioMessage(StreamType stream_type,
                                                         int64_t timestamp_us,
                                                         const char* data,
                                                         size_t data_size) {
  const size_t total_size = sizeof(PcmPacketHeader) + data_size;
  DCHECK_LE(total_size, std::numeric_limits<uint16_t>::max());
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(total_size);
  char* ptr = PopulatePcmAudioHeader(io_buffer->data(), io_buffer->size(),
                                     stream_type, timestamp_us);
  if (!ptr) {
    return nullptr;
  }
  if (data_size > 0) {
    DCHECK(data);
    std::copy(data, data + data_size, ptr);
  }
  return io_buffer;
}

scoped_refptr<net::IOBufferWithSize> MakeHandshakeMessage(
    const StreamInfo& stream_info) {
  auto io_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(sizeof(HandshakePacket));
  PopulateHandshakeMessage(io_buffer->data(), io_buffer->size(), stream_info);
  return io_buffer;
}

scoped_refptr<net::IOBufferWithSize> MakeSerializedMessage(
    MessageType message_type,
    base::span<const uint8_t> data) {
  if (data.empty()) {
    LOG(ERROR) << "Invalid data size: " << data.size() << ".";
    return nullptr;
  }

  const auto message_type_uint8 = static_cast<uint8_t>(message_type);
  const auto message_size =
      static_cast<uint16_t>(sizeof(message_type_uint8) + data.size());
  DCHECK_LE(message_size, std::numeric_limits<uint16_t>::max());
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(
      sizeof(message_size) + message_size);

  base::SpanWriter writer(base::as_writable_bytes(io_buffer->span()));
  writer.WriteU16BigEndian(message_size);
  writer.WriteU8BigEndian(message_type_uint8);
  writer.Write(data);
  return io_buffer;
}

bool ReadDataToAudioBus(const StreamInfo& stream_info,
                        const char* data,
                        size_t size,
                        ::media::AudioBus* audio_bus) {
  DCHECK(audio_bus);
  DCHECK_EQ(stream_info.num_channels, audio_bus->channels());
  return ConvertData(stream_info.num_channels, stream_info.sample_format,
                     data + kPcmAudioHeaderBytes, size - kPcmAudioHeaderBytes,
                     audio_bus);
}

bool ReadPcmAudioMessage(const char* data,
                         size_t size,
                         const StreamInfo& stream_info,
                         int64_t* timestamp_us,
                         ::media::AudioBus* audio_bus) {
  if (!ReadPcmAudioHeader(data, size, stream_info, timestamp_us)) {
    return false;
  }
  return ReadDataToAudioBus(stream_info, data, size, audio_bus);
}

bool ReadHandshakeMessage(const char* data,
                          size_t size,
                          StreamInfo* stream_info) {
  DCHECK(stream_info);
  if (size != kHandshakeHeaderBytes) {
    LOG(ERROR) << "Message doesn't have a complete handshake packet: " << size
               << " v.s. " << kHandshakeHeaderBytes << ".";
    return false;
  }
  HandshakePacket packet;
  memcpy(&packet.message_type, data, kHandshakeHeaderBytes);
  MessageType message_type = static_cast<MessageType>(packet.message_type);
  if (message_type != MessageType::kHandshake ||
      packet.stream_type > static_cast<uint8_t>(StreamType::kLastType) ||
      packet.audio_codec > static_cast<uint8_t>(AudioCodec::kLastCodec) ||
      packet.sample_format > static_cast<uint8_t>(SampleFormat::LAST_FORMAT)) {
    LOG(ERROR) << "Invalid message header.";
    return false;
  }
  if (packet.num_channels > ::media::limits::kMaxChannels) {
    LOG(ERROR) << "Invalid number of channels: " << packet.num_channels;
    return false;
  }
  stream_info->stream_type = static_cast<StreamType>(packet.stream_type);
  stream_info->audio_codec = static_cast<AudioCodec>(packet.audio_codec);
  stream_info->sample_format = static_cast<SampleFormat>(packet.sample_format);
  stream_info->num_channels = packet.num_channels;
  stream_info->frames_per_buffer = packet.num_frames;
  stream_info->sample_rate = packet.sample_rate;
  return true;
}

size_t DataSizeInBytes(const StreamInfo& stream_info) {
  switch (stream_info.sample_format) {
    case SampleFormat::INTERLEAVED_INT16:
    case SampleFormat::PLANAR_INT16:
      return sizeof(int16_t) * stream_info.num_channels *
             stream_info.frames_per_buffer;
    case SampleFormat::INTERLEAVED_INT32:
    case SampleFormat::PLANAR_INT32:
      return sizeof(int32_t) * stream_info.num_channels *
             stream_info.frames_per_buffer;
    case SampleFormat::INTERLEAVED_FLOAT:
    case SampleFormat::PLANAR_FLOAT:
      return sizeof(float) * stream_info.num_channels *
             stream_info.frames_per_buffer;
  }
}

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast
