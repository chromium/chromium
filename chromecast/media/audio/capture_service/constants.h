// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CONSTANTS_H_
#define CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CONSTANTS_H_

#include <cstdint>
#include <ostream>

namespace chromecast {
namespace media {
namespace capture_service {

constexpr char kDefaultUnixDomainSocketPath[] = "/tmp/capture-service";
constexpr int kDefaultTcpPort = 12855;

enum class SampleFormat : uint8_t {
  INTERLEAVED_INT16 = 0,
  INTERLEAVED_INT32 = 1,
  INTERLEAVED_FLOAT = 2,
  PLANAR_INT16 = 3,
  PLANAR_INT32 = 4,
  PLANAR_FLOAT = 5,
  LAST_FORMAT = PLANAR_FLOAT,
};

inline std::ostream& operator<<(std::ostream& os, SampleFormat sample_format) {
  switch (sample_format) {
    case SampleFormat::INTERLEAVED_INT16:
      return os << "INTERLEAVED_INT16";
    case SampleFormat::INTERLEAVED_INT32:
      return os << "INTERLEAVED_INT32";
    case SampleFormat::INTERLEAVED_FLOAT:
      return os << "INTERLEAVED_FLOAT";
    case SampleFormat::PLANAR_INT16:
      return os << "PLANAR_INT16";
    case SampleFormat::PLANAR_INT32:
      return os << "PLANAR_INT32";
    case SampleFormat::PLANAR_FLOAT:
      return os << "PLANAR_FLOAT";
    // Don't use default, so compiler can capture missed enums.
  }
}

enum class StreamType : uint8_t {
  // Raw microphone capture from ALSA or other platform interface.
  kMicRaw = 0,
  // Echo cancelled capture using software AEC.
  kSoftwareEchoCancelled,
  // Echo linearly removed capture using software eraser that has lower cost but
  // will have some non-linear echo residual left.
  kSoftwareEchoCancelledLinear,
  // Hardware echo cancelled capture, e.g., from DSP.
  kHardwareEchoCancelled,
  // Software echo rescaled capture that balances the volume of both far-end and
  // near-end captures. The far-end sound is the echo voice that travels out
  // from the loudspeaker and then is picked up by the system microphone,
  // whereas the near-end sound is the remaining capture sound without the echo
  // voice.
  kSoftwareEchoRescaled,
  // Hardware echo rescaled capture, e.g., from DSP.
  kHardwareEchoRescaled,
  // TDM loopback, e.g., from DSP output processing output.
  kTdmLoopback,
  // USB audio input.
  kUsbAudio,
  // Mark the last type.
  kLastType = kUsbAudio,
};

enum class AudioCodec : uint8_t {
  kPcm = 0,
  kOpus,
  // Mark the last codec.
  kLastCodec = kOpus,
};

enum class MessageType : uint8_t {
  // Handshake message that has stream header but empty body. It is used by
  // receiver notifying the stream it is observing, and sender can confirm the
  // types/codec are supported and send back more detailed parameters.
  kHandshake = 0,
  // PCM audio message that has stream header and audio data in the message
  // body. The audio data will match the parameters in the header.
  kPcmAudio,
  // Opus encoded audio message that doesn't have stream header but a serialized
  // proto data besides the type bits.
  kOpusAudio,
  // Metadata message that doesn't have stream header but a serialized proto
  // data besides the type bits.
  kMetadata,
};

struct StreamInfo {
  StreamType stream_type = StreamType::kMicRaw;
  AudioCodec audio_codec = AudioCodec::kPcm;
  int num_channels = 0;
  SampleFormat sample_format = SampleFormat::INTERLEAVED_INT16;
  int sample_rate = 0;
  int frames_per_buffer = 0;
};

// Size of a PCM audio message header.
constexpr size_t kPcmAudioHeaderBytes = 10;

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CONSTANTS_H_
